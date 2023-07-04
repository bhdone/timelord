#include "block_querier_utils.h"

#include <regex>

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

std::tuple<uint64_t, uint64_t> AnalyzeVdfInfo(std::string_view vdf_str)
{
    std::regex re("(.*)\\((.*) ips\\)");
    std::smatch m;
    std::string s = std::string(vdf_str);
    if (std::regex_match(s, m, re)) {
        std::string total_iters_str = m[1].str();
        std::string iters_per_sec_str = m[2].str();
        return std::make_tuple(StrToInt(total_iters_str), StrToInt(iters_per_sec_str));
    }
    return std::make_tuple(0, 0);
}

BlockInfo ConvertToBlockInfo(UniValue const& block_json)
{
    BlockInfo block_info;
    block_info.hash = Uint256FromHex(block_json["new best"].get_str());
    if (block_json.exists("challenge")) {
        block_info.challenge = Uint256FromHex(block_json["challenge"].get_str());
    }
    block_info.height = StrToInt(block_json["height"].get_str());
    if (block_json.exists("filter-bit")) {
        block_info.filter_bits = StrToInt(block_json["filter-bit"].get_str());
    } else {
        block_info.filter_bits = 0;
    }
    if (block_json.exists("block-difficulty")) {
        block_info.block_difficulty = StrToInt(block_json["block-difficulty"].get_str());
    } else {
        block_info.block_difficulty = 0;
    }
    if (block_json.exists("challenge-diff")) {
        block_info.challenge_difficulty = StrToInt(block_json["challenge-diff"].get_str());
    } else {
        block_info.challenge_difficulty = 0;
    }
    if (block_json.exists("farmer-pk")) {
        block_info.farmer_pk = BytesFromHex(block_json["farmer-pk"].get_str());
    }
    if (block_json.exists("vdf")) {
        std::tie(block_info.vdf_iters, block_info.vdf_speed) = AnalyzeVdfInfo(block_json["vdf"].get_str());
    }
    if (block_json.exists("vdf-time")) {
        block_info.vdf_time = block_json["vdf-time"].get_str();
    }
    if (block_json.exists("vdf-iters-req")) {
        block_info.vdf_iters_req = StrToInt(block_json["vdf-iters-req"].get_str());
    }
    // find the first tx and retrieve the reward info
    if (block_json.exists("txs")) {
        auto txs_json = block_json["txs"];
        if (!txs_json.isArray()) {
            throw std::runtime_error("the type of txs is not an array");
        }
        auto tx_1_json = txs_json.getValues()[0];
        block_info.address = tx_1_json["address"].get_str();
        block_info.reward = tx_1_json["reward"].get_real();
        block_info.accumulate = tx_1_json["accumulate"].get_real();
    }
    return block_info;
}
