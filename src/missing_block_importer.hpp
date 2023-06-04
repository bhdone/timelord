#ifndef MISSING_BLOCK_IMPORTER_HPP
#define MISSING_BLOCK_IMPORTER_HPP

#include <plog/Log.h>

#include "block_info.h"
#include "timelord_utils.h"

/**
 * @brief Find and import missing blocks from RPC server to local database
 *
 * @tparam LocalDBQuerier local database querier type
 * @tparam RPCQuerier RPC querier type
 * @tparam Saver local database saver type
 * @param local_querier local database querier
 * @param rpc_querier RPC querier
 * @param saver use this functor to save block to local database
 * @param min_height import blocks only when the height >= min_height
 * @param force_from_min_height ignore the existing blocks from local db
 *
 * @return the number of blocks are saved
 */
template <typename LocalDBQuerier, typename RPCQuerier, typename Saver> int ImportMissingBlocks(LocalDBQuerier local_querier, RPCQuerier rpc_querier, Saver saver, int min_height, bool force_from_min_height)
{
    auto blocks = rpc_querier(1);
    if (blocks.empty()) {
        // something is wrong of the RPC service, cannot proceed
        PLOGE << "RPC service returns no block";
        return 0;
    }
    int max_height = blocks.front().height;
    int start_height = min_height;
    if (!force_from_min_height) {
        blocks = local_querier(1);
        if (!blocks.empty()) {
            start_height = blocks.front().height;
        }
    }

    int num_heights = max_height - start_height;
    if (num_heights == 0) {
        return 0;
    }
    PLOGD << tinyformat::format("querying total %d blocks from RPC service", num_heights);

    blocks = rpc_querier(num_heights);

    int num_saved { 0 };
    for (auto const& block : blocks) {
        try {
            saver(block);
            ++num_saved;
        } catch (std::exception const& e) {
            PLOGE << tinyformat::format("cannot save block (hash=%s, height=%s) into local database, err: %s", Uint256ToHex(block.hash), block.height, e.what());
        }
    }

    return num_saved;
}

#endif
