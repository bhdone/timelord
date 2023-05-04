#include "block_querier.h"

#include "timelord_utils.h"

int64_t StrToInt(std::string_view str)
{
    std::string num_str;
    for (auto ch : str) {
        if (ch != ',') {
            num_str.push_back(ch);
        }
    }
    return std::atoll(num_str.c_str());
}

template <typename Pred> std::tuple<BlockQuerier::BlockInfo, bool> FindBlockByPred(RPCClient& rpc, int max_heights_to_search, Pred func)
{
    BlockQuerier::BlockInfo block_info;

    auto res = rpc.Call("queryupdatetiphistory", std::to_string(max_heights_to_search));
    if (!res.result.isArray()) {
        throw std::runtime_error("the return value is not an array");
    }

    bool found { false };
    for (auto const& block_json : res.result.getValues()) {
        int block_height = StrToInt(block_json["height"].get_str());
        if (func(block_json)) {
            block_info.challenge = Uint256FromHex(block_json["challenge"].get_str());
            block_info.height = block_height;
            block_info.filter_bits = StrToInt(block_json["filter-bit"].get_str());
            block_info.block_difficulty = StrToInt(block_json["block-difficulty"].get_str());
            block_info.challenge_difficulty = StrToInt(block_json["challenge-diff"].get_str());
            block_info.farmer_pk = BytesFromHex(block_json["farmer-pk"].get_str());
            // find the first tx and retrieve the reward info
            auto txs_json = block_json["txs"];
            if (!txs_json.isArray()) {
                throw std::runtime_error("the type of txs is not an array");
            }
            auto tx_1_json = txs_json.getValues()[0];
            block_info.address = tx_1_json["address"].get_str();
            block_info.reward = tx_1_json["reward"].get_real();
            block_info.accumulate = tx_1_json["accumulate"].get_real();
            found = true;
            break;
        }
    }

    return std::make_tuple(block_info, found);
}

BlockQuerier::BlockQuerier(RPCClient& rpc)
    : rpc_(rpc)
{
}

std::tuple<BlockQuerier::BlockInfo, bool> BlockQuerier::QueryBlockInfo(int height, int max_heights_to_search) const
{
    return FindBlockByPred(rpc_, max_heights_to_search, [height](UniValue const& block_json) {
        int block_height = StrToInt(block_json["height"].get_str());
        return height == -1 || block_height == height;
    });
}

std::tuple<BlockQuerier::BlockInfo, bool> BlockQuerier::QueryBlockInfo(uint256 const& challenge, int max_heights_to_search) const
{
    return FindBlockByPred(rpc_, max_heights_to_search, [&challenge](UniValue const& block_json) {
        uint256 block_challenge = Uint256FromHex(block_json["challenge"].get_str());
        return block_challenge == challenge;
    });
}
