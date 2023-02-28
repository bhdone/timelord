#ifndef BTCHD_VDF_RECEIVER_H
#define BTCHD_VDF_RECEIVER_H

#include <spawn.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <string>
#include <string_view>

#include <asio.hpp>
using asio::ip::tcp;

#include "common_types.h"

struct ProofDetail {
    Bytes y;
    Bytes proof;
    uint8_t witness_type;
    uint64_t iters;
    int duration;
};

class VdfClientProc
{
public:
    VdfClientProc(std::string vdf_client_path, std::string addr, unsigned short port);

    void NewProc(uint256 const& challenge);

    bool ChallengeExists(uint256 const& challenge) const;

    void KillByChallenge(uint256 const& challenge);

    void KillAll();

    std::string const& GetAddress() const
    {
        return addr_;
    }

    unsigned short GetPort() const
    {
        return port_;
    }

    std::size_t GetCount() const
    {
        return pids_.size();
    }

private:
    std::string vdf_client_path_;
    std::string addr_;
    unsigned short port_;
    std::map<uint256, pid_t> pids_;
};

class SocketWriter
{
public:
    explicit SocketWriter(tcp::socket& s);

    void AsyncWrite(Bytes buff);

private:
    void DoWriteNext();

private:
    std::deque<Bytes> buff_deq_;
    tcp::socket& s_;
};

struct Command {
    enum class CommandType { UNKNOWN, OK, STOP, PROOF };

    CommandType type { CommandType::UNKNOWN };
    std::string body;
    std::size_t consumed { 0 };

    static std::string CommandTypeToString(CommandType type)
    {
        switch (type) {
        case CommandType::UNKNOWN:
            return "UNKNOWN";
        case CommandType::OK:
            return "OK";
        case CommandType::STOP:
            return "STOP";
        case CommandType::PROOF:
            return "PROOF";
        }
        return "(error-command-type)";
    }
};

using CommandAnalyzer = std::function<Command(Bytes const&)>;

class VdfClientSession;
using VdfClientSessionPtr = std::shared_ptr<VdfClientSession>;

using SessionNotify = std::function<void(VdfClientSessionPtr)>;
using ProofReceiver = std::function<void(uint256 const& challenge, Bytes const& y, Bytes const& proof, uint8_t witness_type, uint64_t iters, int duration)>;

enum class TimeType { S, N, T };

std::string TimeTypeToString(TimeType type);

class VdfClientSession : public std::enable_shared_from_this<VdfClientSession>
{
public:
    enum Status { INIT, READY, STOPPING };

    static std::string StatusToString(Status s) {
        switch (s) {
        case Status::INIT:
            return "INIT";
        case Status::READY:
            return "READY";
        case Status::STOPPING:
            return "STOPPING";
        }
        return "UNKNOWN";
    }

    VdfClientSession(tcp::socket&& s, uint256 challenge, TimeType time_type, CommandAnalyzer cmd_analyzer);

    ~VdfClientSession();

    void SetReadyHandler(SessionNotify ready_handler);

    void SetFinishedHandler(SessionNotify finished_handler);

    void SetProofReceiver(ProofReceiver proof_receiver);

    void Start();

    void Stop(std::function<void()> callback = []() {});

    void CalcIters(uint64_t iters);

    uint256 const& GetChallenge() const;

    Status GetStatus() const
    {
        return status_;
    }

private:
    void Close();

    uint64_t GetCurrDuration() const;

    void AsyncReadSomeNext();

    Command ParseCommand();

    void ExecuteCommand(Command const& cmd);

    void SendChallenge(uint256 const& challenge);

    void SendInitForm();

    void SendIters(uint64_t iters);

    void SendStrCmd(std::string const& cmd);

private:
    tcp::socket s_;
    Bytes rd_;
    SocketWriter wr_;

    uint256 challenge_;
    TimeType time_type_;

    std::atomic<Status> status_ { Status::INIT };
    std::chrono::system_clock::time_point start_time_;

    CommandAnalyzer cmd_analyzer_;
    SessionNotify ready_handler_;
    SessionNotify finished_handler_;
    ProofReceiver proof_receiver_;
};

class VdfClientMan
{
public:
    explicit VdfClientMan(asio::io_context& ioc, TimeType type, std::string_view vdf_client_path, std::string_view addr, unsigned short port);

    void SetProofReceiver(ProofReceiver proof_receiver);

    void Run();

    void StopByChallenge(uint256 const& challenge);

    void Exit();

    void CalcIters(uint256 challenge, uint64_t iters);

    std::tuple<ProofDetail, bool> QueryExistingProof(uint256 const& challenge, uint64_t iters);

private:
    void AcceptNext();

private:
    VdfClientProc proc_man_;
    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    TimeType time_type_;
    std::set<VdfClientSessionPtr> session_set_;
    uint256 curr_challenge_;
    ProofReceiver proof_receiver_;

    std::map<uint256, std::vector<uint64_t>> waiting_iters_;
    std::map<uint256, std::vector<ProofDetail>> saved_proofs_;
};

#endif
