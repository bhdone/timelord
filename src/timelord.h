#ifndef TL_TIMELORD_HPP
#define TL_TIMELORD_HPP

#include <functional>
#include <map>

#include <string>
#include <string_view>

#include <fmt/core.h>
#include <plog/Log.h>

#include "common_types.h"

#include "block_info.h"

#include "querier_defs.h"
#include "challenge_monitor.h"
#include "frontend.h"
#include "vdf_client_man.h"

#include "local_sqlite_storage.h"

class MessageDispatcher
{
public:
    using Handler = std::function<void(FrontEndSessionPtr psession, Json::Value const& msg)>;

    void RegisterHandler(int id, Handler handler);

    void operator()(FrontEndSessionPtr psession, Json::Value const& msg) const;

private:
    std::map<int, Handler> handlers_;
};

class Timelord
{
    struct ChallengeRequestSession {
        std::weak_ptr<FrontEndSession> pweak_session;
        uint64_t iters;
        uint256 group_hash;
        uint64_t total_size;
    };

public:
    struct Status {
        uint256 challenge;
        uint64_t difficulty;
        int height;
        uint64_t iters_per_sec;
        uint64_t total_size;
        int num_connections;
        std::string status_string;
    };

    Timelord(asio::io_context& ioc, RPCClient& rpc, std::string_view vdf_client_path, std::string_view vdf_client_addr, unsigned short vdf_client_port, int fork_height, LocalSQLiteDatabaseKeeper& persist_operator, LocalSQLiteStorage& storage, VDFProofSubmitterType submitter);

    void Run(std::string_view addr, unsigned short port);

    void Exit();

    Status QueryStatus() const;

private:
    void HandleChallengeMonitor_NewChallenge(uint256 const& old_challenge, uint256 const& new_challenge, int height, uint64_t difficulty);

    void HandleChallengeMonitor_NewVdfReqs(uint256 const& challenge, std::set<uint64_t> const& vdf_reqs);

    void HandleFrontEnd_NewSessionConnected(FrontEndSessionPtr psession);

    void HandleFrontEnd_SessionError(FrontEndSessionPtr psession, FrontEndSessionErrorType type, std::string_view errs);

    void HandleFrontEnd_SessionRequestChallenge(FrontEndSessionPtr psession, Json::Value const& msg);

    void HandleVdfClient_ProofIsReceived(uint256 const& challenge, vdf_client::ProofDetail const& detail);

    std::tuple<uint64_t, bool> AddAndSumNetspace(uint256 const& group_hash, uint64_t total_size);

    asio::io_context& ioc_;
    LocalSQLiteDatabaseKeeper& persist_operator_;
    BlockInfoRangeQuerierType block_info_querier_;
    BlockInfoSaverType block_info_saver_;
    VDFProofSubmitterType vdf_proof_submitter_;

    FrontEnd frontend_;
    MessageDispatcher msg_dispatcher_;
    std::map<uint256, std::vector<ChallengeRequestSession>> challenge_reqs_;

    int fork_height_;
    ChallengeMonitor challenge_monitor_;
    int height_ { 0 };
    uint64_t difficulty_ { 0 };

    vdf_client::VdfClientMan vdf_client_man_;

    std::set<std::shared_ptr<asio::steady_timer>> ptimer_wait_close_vdf_set_;
    uint64_t iters_per_sec_ { 0 };

    std::map<uint256, uint64_t> netspace_;
};

#endif
