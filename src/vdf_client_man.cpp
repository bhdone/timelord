#include "vdf_client_man.h"

#include <spawn.h>
#include <sys/wait.h>

#include <plog/Log.h>
#include <tinyformat.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "vdf_computer.h"
#include "vdf_types.h"
#include "vdf_utils.h"

#include "timelord_utils.h"
#include "utils.h"

using boost::system::error_code;

namespace vdf_client
{
namespace
{

Command AnalyzeStrCmd(Bytes const& buf, char const* cmd_str, Command::CommandType type)
{
    PLOGD << "checking for command: " << cmd_str;
    Command res;
    if (buf.size() < std::strlen(cmd_str)) {
        return res;
    }
    if (std::memcmp(buf.data(), cmd_str, std::strlen(cmd_str)) == 0) {
        PLOGD << "recv command: " << cmd_str;
        res.body = cmd_str;
        res.type = type;
        res.consumed = res.body.size();
    }
    return res;
}

#ifdef __APPLE__

#include <libkern/OSByteOrder.h>
uint64_t bswap_common(uint64_t x)
{
    return OSSwapInt64(x);
}
uint32_t bswap_common(uint32_t x)
{
    return OSSwapInt32(x);
}

#else

uint64_t bswap_common(uint64_t x)
{
    return __bswap_64(x);
}
uint32_t bswap_common(uint32_t x)
{
    return __bswap_32(x);
}

#endif

struct Slice {
    uint8_t const* p { nullptr };
    std::size_t n { 0 };
};

Slice MakeSlice(Bytes const& buf, uint64_t offset)
{
    assert(offset < buf.size());
    Slice s;
    s.p = buf.data() + offset;
    s.n = buf.size() - offset;
    return s;
}

template <typename Int> std::tuple<Int, bool> IntFromBytes(Slice const& slice)
{
    if (slice.n < sizeof(Int)) {
        return std::make_tuple(0, false);
    }
    Int x;
    std::memcpy(&x, slice.p, sizeof(Int));
    return std::make_tuple(bswap_common(x), true);
}

struct Proof {
    Bytes y;
    Bytes proof;
    uint8_t witness_type;
};

std::tuple<Proof, uint64_t> ParseProofIters(Bytes const& buf)
{
    Proof proof;
    // Analyze proof from bytes
    bool succ;
    uint64_t offset { 0 };
    // Read iterators
    uint64_t iters;
    std::tie(iters, succ) = IntFromBytes<uint64_t>(MakeSlice(buf, 0));
    if (!succ) {
        return std::make_tuple(proof, 0);
    }
    offset += sizeof(iters);
    // Read y
    uint64_t y_size;
    std::tie(y_size, succ) = IntFromBytes<uint64_t>(MakeSlice(buf, offset));
    if (!succ) {
        return std::make_tuple(proof, 0);
    }
    offset += sizeof(y_size);
    if (buf.size() - offset < y_size) {
        return std::make_tuple(proof, 0);
    }
    proof.y.resize(y_size);
    std::memcpy(proof.y.data(), buf.data() + offset, y_size);
    offset += y_size;
    // Read witness type
    if (buf.size() - offset == 0) {
        return std::make_tuple(proof, 0);
    }
    proof.witness_type = buf.data()[offset++];
    // Read proof
    std::size_t remain_size = buf.size() - offset;
    proof.proof.resize(remain_size);
    std::memcpy(proof.proof.data(), buf.data() + offset, remain_size);
    // Return proof
    PLOGD << "== y(len=" << proof.y.size() << "): " << BytesToHex(proof.y);
    PLOGD << "== proof(len=" << proof.proof.size() << "): " << BytesToHex(proof.proof);
    PLOGD << "== witness_type: " << static_cast<int>(proof.witness_type);
    return std::make_tuple(proof, iters);
}

Bytes MakeChallengeBuf(uint256 const& challenge)
{
    auto disc = vdf::utils::CreateDiscriminant(MakeBytes(challenge));
    std::string disc_str = disc.FormatString();
    int disc_size = disc_str.size();
    std::string disc_size_str = std::to_string(disc_size);
    assert(disc_size_str.size() <= 3);
    std::size_t size = disc_str.size() + 3;
    Bytes buf(size, 0);
    std::memcpy(buf.data(), disc_size_str.data(), disc_size_str.size());
    std::memcpy(buf.data() + 3, disc_str.data(), disc_str.size());
    return buf;
}

Bytes MakeFormBuf(VdfForm const& form)
{
    Bytes form_buf(form.size() + 1);
    form_buf[0] = form.size();
    std::memcpy(form_buf.data() + 1, form.data(), form.size());
    return form_buf;
}

Command AnalyzeProofCommand(Bytes const& buf)
{
    Command cmd;
    bool succ;
    uint32_t size;
    std::tie(size, succ) = IntFromBytes<uint32_t>(MakeSlice(buf, 0));
    if (!succ) {
        return cmd;
    }
    if (buf.size() - sizeof(size) < size) {
        return cmd;
    }
    cmd.body = std::string(buf.begin() + sizeof(size), buf.end());
    cmd.type = Command::CommandType::PROOF;
    cmd.consumed = sizeof(size) + cmd.body.size();
    return cmd;
}

class VDFCommandAnalyzer
{
public:
    VDFCommandAnalyzer()
    {
        analyzers_.push_back(std::bind(AnalyzeStrCmd, std::placeholders::_1, "OK", Command::CommandType::OK));
        analyzers_.push_back(std::bind(AnalyzeStrCmd, std::placeholders::_1, "STOP", Command::CommandType::STOP));
        analyzers_.push_back(AnalyzeProofCommand);
    }

    Command operator()(Bytes const& buf) const
    {
        Command cmd;
        PLOGD << "total " << analyzers_.size() << " analyzers";
        for (CommandAnalyzer const& analyzer : analyzers_) {
            cmd = analyzer(buf);
            if (cmd.type != Command::CommandType::UNKNOWN) {
                break;
            }
        }
        return cmd;
    }

private:
    std::vector<CommandAnalyzer> analyzers_;
};

} // namespace

static int const SECS_TO_WAIT_STOPPING = 2;
static int const BUFLEN = 1024 * 8;

VdfClientProc::VdfClientProc(std::string vdf_client_path, std::string addr, unsigned short port)
    : vdf_client_path_(std::move(vdf_client_path))
    , addr_(std::move(addr))
    , port_(port)
{
}

void VdfClientProc::NewProc(uint256 const& challenge)
{
    if (pids_.find(challenge) != std::cend(pids_)) {
        PLOGE << "the challenge of the vdf_client is already running";
        return;
    }
    pid_t pid;
    auto port_str = std::to_string(port_);
    PLOGD << "spawn process: " << vdf_client_path_ << " " << addr_ << " " << port_str;
    char const* argv[] = { vdf_client_path_.c_str(), addr_.c_str(), port_str.c_str(), "0", nullptr };
    int ret = posix_spawn(&pid, vdf_client_path_.c_str(), nullptr, nullptr, const_cast<char**>(argv), nullptr);
    if (ret == 0) {
        pids_.insert(std::make_pair(challenge, pid));
    } else {
        PLOGE << "cannot create a new vdf_client process, command: " << vdf_client_path_ << " " << addr_ << " " << port_str;
    }
}

bool VdfClientProc::ChallengeExists(uint256 const& challenge) const
{
    return pids_.find(challenge) != std::cend(pids_);
}

void VdfClientProc::KillByChallenge(uint256 const& challenge)
{
    auto it = pids_.find(challenge);
    if (it == std::cend(pids_)) {
        return;
    }
    pid_t pid = it->second;
    auto r = kill(pid, SIGKILL);
    if (r != 0) {
        PLOGE << "failed to kill process " << pid << ", challenge: " << challenge;
        return;
    }
    pids_.erase(it);
}

void VdfClientProc::KillAll()
{
    for (auto e : pids_) {
        PLOGD << "killing pid: " << e.second;
        auto r = kill(e.second, SIGKILL);
        if (r != 0) {
            PLOGE << "failed to kill process " << e.second;
        }
    }
    pids_.clear();
}

SocketWriter::SocketWriter(tcp::socket& s)
    : s_(s)
{
}

void SocketWriter::AsyncWrite(Bytes buff)
{
    PLOGD << "prepare to write total " << buff.size() << " bytes: " << BytesToHex(buff);
    bool do_write = buff_deq_.empty();
    buff_deq_.push_back(std::move(buff));
    if (do_write) {
        DoWriteNext();
    }
}

void SocketWriter::DoWriteNext()
{
    assert(!buff_deq_.empty());
    Bytes const& buf = buff_deq_.front();
    asio::async_write(s_, asio::buffer(buf), [this](error_code const& ec, std::size_t size) {
        if (ec) {
            PLOG_ERROR << "write error: " << ec.message();
            return;
        }
        PLOGD << "total wrote " << size << " bytes";
        buff_deq_.pop_front();
        if (!buff_deq_.empty()) {
            DoWriteNext();
        }
    });
}

std::string TimeTypeToString(TimeType type)
{
    switch (type) {
    case TimeType::S:
        return "S";
    case TimeType::N:
        return "N";
    case TimeType::T:
        return "T";
    }
    return "(error-timetype)";
}

VdfClientSession::VdfClientSession(tcp::socket&& s, uint256 challenge, TimeType time_type, CommandAnalyzer cmd_analyzer)
    : s_(std::move(s))
    , challenge_(std::move(challenge))
    , time_type_(time_type)
    , cmd_analyzer_(std::move(cmd_analyzer))
    , wr_(s_)
{
    PLOGD << "session " << AddressToString(this) << " is created";
}

VdfClientSession::~VdfClientSession()
{
    PLOGD << "Session " << AddressToString(this) << " is about to release";
}

void VdfClientSession::SetReadyHandler(SessionNotify ready_handler)
{
    ready_handler_ = std::move(ready_handler);
}

void VdfClientSession::SetFinishedHandler(SessionNotify finished_handler)
{
    finished_handler_ = std::move(finished_handler);
}

void VdfClientSession::SetProofReceiver(ProofReceiver proof_receiver)
{
    proof_receiver_ = std::move(proof_receiver);
}

void VdfClientSession::Start()
{
    // type
    SendStrCmd(TimeTypeToString(time_type_));
    // challenge
    SendChallenge(challenge_);
    // initial form (zero)
    SendInitForm();
    // Start to read
    AsyncReadSomeNext();
}

void VdfClientSession::Stop(std::function<void()> callback)
{
    auto timer = std::make_unique<asio::steady_timer>(s_.get_executor());
    timer->expires_after(std::chrono::seconds(SECS_TO_WAIT_STOPPING));
    timer->async_wait([weak_self = std::weak_ptr(shared_from_this()), timer = std::move(timer), callback = std::move(callback)](error_code const& ec) {
        if (ec) {
            // error occurs
            PLOGE << "error when waiting to close a session: " << ec.message();
            return;
        }
        auto self = weak_self.lock();
        if (self) {
            PLOGE << "force to close a session";
            self->Close(); // force to close the session
        }
        callback();
    });
    PLOGD << "sending iters=0 to stop the session";
    CalcIters(0);
    status_ = Status::STOPPING;
}

bool VdfClientSession::CalcIters(uint64_t iters)
{
    if (status_ != Status::READY) {
        // ignore iters when the session isn't READY
        PLOGE << "warning, trying to calculate iters " << iters << " on a " << VdfClientSession::StatusToString(status_) << " session";
        return false;
    }
    if (SendIters(iters)) {
        if (best_iters_ == 0) {
            best_iters_ = iters;
        } else if (best_iters_ > iters) {
            best_iters_ = iters;
        }
        ++answers_count_;
        return true;
    }
    return false;
}

uint64_t VdfClientSession::GetBestIters() const
{
    return best_iters_;
}

uint256 const& VdfClientSession::GetChallenge() const
{
    return challenge_;
}

void VdfClientSession::Close()
{
    error_code ignored_ec;
    s_.shutdown(tcp::socket::shutdown_both, ignored_ec);
    s_.close(ignored_ec);
}

uint64_t VdfClientSession::GetCurrDuration() const
{
    auto now = std::chrono::system_clock::now();
    uint64_t duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    return duration;
}

void VdfClientSession::AsyncReadSomeNext()
{
    auto tmp = std::make_shared<Bytes>(BUFLEN, '\0');
    s_.async_read_some(asio::buffer(*tmp), [self = shared_from_this(), tmp](error_code const& ec, std::size_t size) {
        if (ec) {
            if (ec != asio::error::eof) {
                // Only show the error message when it isn't `eof`.
                PLOGE << "error occurs when reading: " << ec.message();
            } else {
                PLOGD << "read eof";
            }
            self->finished_handler_(self);
            return;
        }
        if (size) {
            PLOGD << "total read " << size << " bytes";
            std::size_t remaining_size = self->rd_.size();
            self->rd_.resize(self->rd_.size() + size);
            memcpy(self->rd_.data() + remaining_size, tmp->data(), size);
            // Parse and run command
            self->ExecuteCommand(self->ParseCommand());
        }
        self->AsyncReadSomeNext();
    });
}

Command VdfClientSession::ParseCommand()
{
    Command cmd = cmd_analyzer_(rd_);
    if (cmd.type != Command::CommandType::UNKNOWN) {
        assert(cmd.body.empty() == false && cmd.consumed != 0 && cmd.consumed >= cmd.body.size());
        Bytes new_recv(rd_.size() - cmd.consumed);
        memcpy(new_recv.data(), rd_.data() + cmd.consumed, rd_.size() - cmd.consumed);
        rd_ = new_recv;
        PLOGD << "parsed command: " << Command::CommandTypeToString(cmd.type) << ", remains " << rd_.size() << " bytes";
    }
    return cmd;
}

void VdfClientSession::ExecuteCommand(Command const& cmd)
{
    if (cmd.type == Command::CommandType::UNKNOWN) {
        return;
    }
    PLOGD << "execute command: " << Command::CommandTypeToString(cmd.type);
    if (cmd.type == Command::CommandType::OK) {
        // The session is ready for iters
        PLOGD << "ready to send iters";
        start_time_ = std::chrono::system_clock::now();
        status_ = Status::READY;
        // start all waiting iters
        ready_handler_(shared_from_this());
    } else if (cmd.type == Command::CommandType::STOP) {
        // The vdf_client is about to close, session will read eof
        SendStrCmd("ACK");
    } else if (cmd.type == Command::CommandType::PROOF) {
        // Analyze the proof back and invoke the receiver
        PLOGD << "proof is ready";
        Proof proof;
        uint64_t iters;
        std::tie(proof, iters) = ParseProofIters(BytesFromHex(cmd.body));
        ProofDetail detail;
        detail.y = proof.y;
        detail.proof = proof.proof;
        detail.witness_type = proof.witness_type;
        detail.iters = iters;
        detail.duration = GetCurrDuration();
        proof_receiver_(challenge_, detail);
    }
}

void VdfClientSession::SendChallenge(uint256 const& challenge)
{
    PLOGD << "sending challenge: " << Uint256ToHex(challenge);
    Bytes challenge_buf = MakeChallengeBuf(challenge);
    wr_.AsyncWrite(std::move(challenge_buf));
}

void VdfClientSession::SendInitForm()
{
    PLOGD << "sending initial form";
    Bytes initial_form = MakeFormBuf(MakeZeroForm());
    wr_.AsyncWrite(std::move(initial_form));
}

bool VdfClientSession::SendIters(uint64_t iters)
{
    if (delivered_iters_.find(iters) != std::end(delivered_iters_)) {
        // already delivered
        return false;
    }
    delivered_iters_.insert(iters);
    PLOGD << "sending iters: " << iters;
    std::string iters_str = std::to_string(iters);
    std::string size_str = std::to_string(iters_str.size());
    if (size_str.size() == 1) {
        size_str = std::string("0") + size_str;
    }
    PLOGD << "size_str: " << size_str;
    Bytes buf(2 + iters_str.size(), '\0');
    std::memcpy(buf.data(), size_str.data(), 2);
    std::memcpy(buf.data() + 2, iters_str.data(), iters_str.size());
    wr_.AsyncWrite(std::move(buf));
    return true;
}

void VdfClientSession::SendStrCmd(std::string const& cmd)
{
    PLOGD << "sending str command: " << cmd;
    Bytes buf(cmd.size(), '\0');
    std::memcpy(buf.data(), cmd.data(), cmd.size());
    wr_.AsyncWrite(std::move(buf));
}

VdfClientMan::VdfClientMan(asio::io_context& ioc, TimeType type, std::string_view vdf_client_path, std::string_view addr, unsigned short port)
    : proc_man_(std::string(vdf_client_path), std::string(addr), port)
    , ioc_(ioc)
    , acceptor_(ioc)
    , time_type_(type)
{
    MakeZero(init_challenge_, 0);
}

void VdfClientMan::SetProofReceiver(ProofReceiver proof_receiver)
{
    proof_receiver_ = std::move(proof_receiver);
}

void VdfClientMan::Run()
{
    tcp::endpoint endpoint(asio::ip::address::from_string(proc_man_.GetAddress()), proc_man_.GetPort());
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    PLOGD << "accept connection to " << proc_man_.GetAddress() << ":" << proc_man_.GetPort();
    AcceptNext();
}

void VdfClientMan::StopByChallenge(uint256 const& challenge)
{
    for (auto psession : session_set_) {
        if (psession->GetChallenge() == challenge) {
            psession->Stop([this, challenge]() {
                proc_man_.KillByChallenge(challenge);
            });
            break;
        }
    }
}

void VdfClientMan::Exit()
{
    // Tell all client to stop
    PLOGD << "stopping... total " << session_set_.size() << " session(s)";
    error_code ignored_ec;
    acceptor_.close(ignored_ec);
    for (auto psession : session_set_) {
        psession->Stop();
    }
    auto ptimer = std::make_unique<asio::steady_timer>(ioc_);
    ptimer->expires_after(std::chrono::seconds(SECS_TO_WAIT_STOPPING));
    ptimer->async_wait([this, ptimer = std::move(ptimer)](error_code const& ec) {
        proc_man_.KillAll(); // so rude
    });
}

void VdfClientMan::CalcIters(uint256 const& challenge, uint64_t iters)
{
    PLOGD << "request: " << Uint256ToHex(challenge) << ", iters=" << iters;
    auto exist_detail = QueryExistingProof(challenge, iters);
    if (exist_detail.has_value()) {
        PLOGI << "the request is already calculated, skip";
        return;
    }
    if (!IsZero(init_challenge_) && challenge != init_challenge_) {
        // there is a running procedure to create a vdf_client, run it later
        PLOGE << "cannot run another vdf_client while there is already one creating, try it later";
        auto ptimer = std::make_unique<asio::steady_timer>(ioc_);
        ptimer->expires_after(std::chrono::milliseconds(100));
        ptimer->async_wait([this, challenge, iters, ptimer = std::move(ptimer)](error_code const& ec) {
            CalcIters(challenge, iters);
        });
        return;
    }
    for (auto psession : session_set_) {
        if (psession->GetStatus() == VdfClientSession::Status::READY && psession->GetChallenge() == challenge) {
            if (psession->CalcIters(iters)) {
                ShowTheBest(psession->GetChallenge(), psession->GetBestIters(), iters, psession->GetAnswersCount());
            }
            return;
        }
    }
    // all iters will be delivered as soon as the vdf_client session is connected and ready
    auto it = waiting_iters_.find(challenge);
    if (it == std::cend(waiting_iters_)) {
        waiting_iters_.insert(std::make_pair(challenge, std::set<uint64_t> { iters }));
    } else {
        it->second.insert(iters);
    }
    PLOGD << "the request is saved and it will be retrieved when the vdf_client is ready";
    // cannot find the challenge from existing sessions, check the related process from proc manager
    if (!proc_man_.ChallengeExists(challenge)) {
        PLOGI << tinyformat::format("creating vdf_client for challenge %s, proc count=%d", Uint256ToHex(challenge), proc_man_.GetCount());
        proc_man_.NewProc(challenge);
        init_challenge_ = challenge;
    }
}

std::optional<ProofDetail> VdfClientMan::QueryExistingProof(uint256 const& challenge, uint64_t iters)
{
    auto it = saved_proofs_.find(challenge);
    if (it == std::cend(saved_proofs_)) {
        return {};
    }
    for (auto const& proof_detail : it->second) {
        if (proof_detail.iters >= iters) {
            return proof_detail;
        }
    }
    return {};
}

void VdfClientMan::AcceptNext()
{
    acceptor_.async_accept([this](error_code const& ec, tcp::socket s) {
        if (ec) {
            PLOGE << "error occurs when accepting next session... " << ec.message();
            return;
        } else {
            // Create new session
            auto psession = std::make_shared<VdfClientSession>(std::move(s), init_challenge_, time_type_, VDFCommandAnalyzer());
            psession->SetReadyHandler([this](VdfClientSessionPtr psession) {
                assert(init_challenge_ == psession->GetChallenge());
                MakeZero(init_challenge_,
                        0); // reset current challenge to zero will allow user to start another vdf_client
                // get the iters
                auto it = waiting_iters_.find(psession->GetChallenge());
                if (it == std::cend(waiting_iters_)) {
                    // cannot find waiting iters, just simply exit
                    return;
                }
                for (auto iters : it->second) {
                    PLOGI << "saved request is awaken: " << Uint256ToHex(psession->GetChallenge()) << ", iters=" << iters;
                    if (psession->CalcIters(iters)) {
                        ShowTheBest(psession->GetChallenge(), psession->GetBestIters(), iters, psession->GetAnswersCount());
                    }
                }
                waiting_iters_.erase(it);
            });
            psession->SetFinishedHandler([this](VdfClientSessionPtr psession) {
                session_set_.erase(psession);
            });
            psession->SetProofReceiver([this](uint256 const& challenge, ProofDetail const& detail) {
                // we need to save the proof to memories as well
                auto it = saved_proofs_.find(challenge);
                if (it == std::cend(saved_proofs_)) {
                    saved_proofs_.insert(std::make_pair(challenge, std::vector<ProofDetail> { detail }));
                } else {
                    it->second.push_back(detail);
                }
                // update vdf speed
                if (detail.duration > 3) {
                    vdf_speed_ = detail.iters / detail.duration;
                }
                // invoke callback
                proof_receiver_(challenge, detail);
            });
            psession->Start();
            session_set_.insert(psession);
        }
        AcceptNext();
    });
}

void VdfClientMan::ShowTheBest(uint256 const& challenge, uint64_t best_iters, uint64_t curr_iters, int answers_count)
{
    PLOGI << tinyformat::format("next block %s, curr %s, count %d, challenge=%s", FormatTime(best_iters / vdf_speed_), FormatTime(curr_iters / vdf_speed_), answers_count, Uint256ToHex(challenge));
}

} // namespace vdf_client
