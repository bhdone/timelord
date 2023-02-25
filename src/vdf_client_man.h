#ifndef BTCHD_VDF_RECEIVER_H
#define BTCHD_VDF_RECEIVER_H

#include <spawn.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <map>
#include <vector>
#include <array>
#include <set>

#include <string>
#include <string_view>

#include <asio.hpp>
using asio::ip::tcp;

#include "common_types.h"

namespace vdf_client
{

class VdfClientProc
{
public:
    VdfClientProc(std::string vdf_client_path, std::string hostname, uint16_t port);

    void NewProc();

    int RemoveDeadChildren();

    void Wait();

    int GetCount() const { return m_children.size(); }

private:
    std::string m_vdf_client_path;
    std::string m_hostname;
    uint16_t m_port;
    std::vector<pid_t> m_children;
};

class SocketWriter
{
public:
    explicit SocketWriter(tcp::socket& s);

    void AsyncWrite(Bytes buff);

private:
    void DoWriteNext();

private:
    std::deque<Bytes> m_buffs;
    tcp::socket& m_s;
};

struct Command
{
    enum class CommandType { UNKNOWN, OK, STOP, PROOF };

    CommandType type{CommandType::UNKNOWN};
    std::string body;
    std::size_t consumed{0};

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

struct Proof
{
    Bytes y;
    Bytes proof;
    uint8_t witness_type;
};

using ProofReceiver = std::function<void(Proof const& proof, uint64_t iters, uint64_t duration, uint256 challenge)>;

class VdfClientSession;
using VdfClientSessionPtr = std::shared_ptr<VdfClientSession>;
using SessionNotify = std::function<void(VdfClientSessionPtr)>;

enum class TimeType { S, N, T };

std::string TimeTypeToString(TimeType type);

class VdfClientSession : public std::enable_shared_from_this<VdfClientSession>
{
public:
    VdfClientSession(asio::io_context& ioc, tcp::socket&& s, uint256 challenge, TimeType time_type, CommandAnalyzer cmd_analyzer);

    void Start(SessionNotify ready_callback, SessionNotify finished_callback, ProofReceiver receiver);

    void StopAfterSeconds(int secs);

    void Close();

    bool IsStopping() const;

    void CalcIters(uint64_t iters);

    uint256 const& GetChallenge() const;

private:
    uint64_t GetCurrDuration() const;

    void AsyncReadSomeNext();

    Command ParseCommand();

    void ExecuteCommand(Command const& cmd);

    void SendChallenge(uint256 const& challenge);

    void SendInitForm();

    void SendIters(uint64_t iters);

    void SendStrCmd(std::string const& cmd);

private:
    asio::io_context& m_ioc;
    tcp::socket m_socket;
    uint256 m_challenge;
    TimeType m_time_type;
    std::chrono::system_clock::time_point m_start_time;
    CommandAnalyzer m_cmd_analyzer;
    SessionNotify m_ready_callback;
    SessionNotify m_finished_callback;
    ProofReceiver m_proof_receiver;
    SocketWriter m_writer;
    Bytes m_temp;
    Bytes m_recv;
    std::vector<uint64_t> m_saved_iters;
    std::mutex m_saved_iters_mtx;
    std::atomic_bool m_ready{false};
    std::atomic_bool m_stopping{false};
};

class VdfClientMan
{
public:
    struct ProofRecord
    {
        Proof proof;
        uint64_t iters;
        uint64_t duration;
        uint256 challenge;
    };

    explicit VdfClientMan(asio::io_context& ioc);

    void Start(ProofReceiver receiver, std::string_view vdf_client_path, std::string_view hostname, uint16_t port);

    void StopByChallenge(uint256 const& challenge);

    void Stop();

    void GoChallenge(uint256 challenge, TimeType time_type, SessionNotify session_is_ready_callback);

    uint256 const& GetCurrentChallenge() const;

    bool QueryIters(uint256 const& challenge, uint64_t iters, ProofRecord& out);

    void CalcIters(uint256 const& challenge, uint64_t iters);

    void Wait();

    std::map<uint256, std::vector<ProofRecord>> const& GetCachedProofs() const { return m_cached_proofs; }

    std::set<uint256> GetRunningChallenges() const;

private:
    void AcceptNext();

    void StopAllSessions();

    void HandleSessionIsFinished(VdfClientSessionPtr psession);

    void HandleProof(Proof const& proof, uint64_t iters, uint64_t duration, uint256 challenge);

private:
    asio::io_context& m_ioc;
    tcp::acceptor m_acceptor;
    uint256 m_challenge;
    TimeType m_time_type;
    std::string m_hostname;
    uint16_t m_port;
    SessionNotify m_session_is_ready_callback;
    ProofReceiver m_receiver;
    std::vector<VdfClientSessionPtr> m_session_vec;
    std::mutex m_session_mtx;
    std::unique_ptr<VdfClientProc> m_proc_man;
    bool m_exiting{false};
    std::mutex m_cached_proofs_mtx;
    std::map<uint256, std::vector<ProofRecord>> m_cached_proofs;
};

}  // namespace vdf_client

#endif
