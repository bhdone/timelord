#ifndef VDF_PROOF_SUBMITTER
#define VDF_PROOF_SUBMITTER

#include <plog/Log.h>
#include <tinyformat.h>

#include "timelord_utils.h"
#include "common_types.h"
#include "rpc_client.h"

class VDFProofSubmitter
{
public:
    explicit VDFProofSubmitter(RPCClient& rpc)
        : rpc_(rpc)
    {
    }

    void operator()(uint256 const& challenge, Bytes const& y, Bytes const& proof, int witness_type, uint64_t iters, int duration)
    {
        try {
            rpc_.Call("submitvdfproof", Uint256ToHex(challenge), BytesToHex(y), BytesToHex(proof), witness_type, iters, duration);
        } catch (std::exception const& e) {
            PLOGE << tinyformat::format("%s: cannot submit vdf proof to RPC server, %s", __func__, e.what());
        }
    }

private:
    RPCClient& rpc_;
};

#endif
