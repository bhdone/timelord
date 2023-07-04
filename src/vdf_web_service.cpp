#include "vdf_web_service.h"

#include <json/json.h>

#include <functional>
using std::placeholders::_1;

#include <boost/url.hpp>
namespace urls = boost::urls;

#include "timelord_utils.h"

#include "ip_addr_querier.hpp"

Json::Value MakeRecordJson(VDFRecord const& record)
{
    Json::Value res;
    res["timestamp"] = record.timestamp;
    res["challenge"] = Uint256ToHex(record.challenge);
    res["height"] = record.height;
    return res;
}

Json::Value MakeRequestJson(VDFRequest const& request)
{
    Json::Value res;
    res["challenge"] = Uint256ToHex(request.challenge);
    res["iters"] = request.iters;
    res["estimated_seconds"] = request.estimated_seconds;
    res["group_hash"] = Uint256ToHex(request.group_hash);
    res["total_size"] = request.total_size;
    return res;
}

Json::Value MakePackJson(VDFRecordPack const& pack)
{
    Json::Value res;
    res = MakeRecordJson(pack.record);

    Json::Value request_values(Json::arrayValue);
    for (auto const& request : pack.requests) {
        request_values.append(MakeRequestJson(request));
    }
    res["requests"] = request_values;

    return res;
}

std::tuple<std::string, bool> ParseUrlParameter(std::string_view target, std::string_view name)
{
    auto req_path = urls::parse_origin_form(target);
    auto it = req_path->params().find(name);
    if (it == std::cend(req_path->params())) {
        return std::make_tuple("", false);
    }
    return std::make_tuple((*it).value, true);
}

VDFWebService::VDFWebService(asio::io_context& ioc, std::string_view addr, uint16_t port, int expired_after_secs, std::string api_path_prefix, int fork_height, NumHeightsByHoursQuerierType num_heights_by_hours_querier, BlockInfoRangeQuerierType block_info_range_querier, NetspaceQuerierType netspace_querier, TimelordStatusQuerierType status_querier, RankQuerierType rank_querier, SupplyQuerierType supply_querier, PledgeInfoQuerierType pledge_info_querier, RecentlyNetspaceSizeQuerierType recently_netspace_querier)
    : web_service_(ioc, tcp::endpoint(asio::ip::address::from_string(std::string(addr)), port), expired_after_secs, std::bind(&VDFWebService::HandleRequest, this, _1))
    , fork_height_(fork_height)
    , num_heights_by_hours_querier_(std::move(num_heights_by_hours_querier))
    , block_info_range_querier_(std::move(block_info_range_querier))
    , netspace_querier_(std::move(netspace_querier))
    , status_querier_(std::move(status_querier))
    , rank_querier_(std::move(rank_querier))
    , supply_querier_(std::move(supply_querier))
    , pledge_info_querier_(std::move(pledge_info_querier))
    , recently_netspace_querier_(std::move(recently_netspace_querier))
{
    web_req_handler_.Register(std::make_pair(http::verb::get, api_path_prefix + "/api/summary"), std::bind(&VDFWebService::Handle_API_Summary, this, _1));
    web_req_handler_.Register(std::make_pair(http::verb::get, api_path_prefix + "/api/status"), std::bind(&VDFWebService::Handle_API_Status, this, _1));
    web_req_handler_.Register(std::make_pair(http::verb::get, api_path_prefix + "/api/netspace"), std::bind(&VDFWebService::Handle_API_Netspace, this, _1));
    web_req_handler_.Register(std::make_pair(http::verb::get, api_path_prefix + "/api/rank"), std::bind(&VDFWebService::Handle_API_Rank, this, _1));
}

void VDFWebService::Run()
{
    return web_service_.Run();
}

void VDFWebService::Stop()
{
    return web_service_.Stop();
}

http::message_generator VDFWebService::HandleRequest(http::request<http::string_body> const& request)
{
    return web_req_handler_.Handle(request);
}

http::message_generator VDFWebService::Handle_API_Status(http::request<http::string_body> const& request)
{
    auto status = status_querier_();

    Json::Value status_value;
    status_value["server_ip"] = status.hostip;
    status_value["challenge"] = Uint256ToHex(status.challenge);
    status_value["difficulty"] = status.difficulty;
    status_value["height"] = status.height;
    status_value["fork_height"] = fork_height_;
    status_value["iters_per_sec"] = status.iters_per_sec;
    status_value["total_size"] = status.total_size;
    status_value["max_size"] = status.max_size;
    status_value["num_connections"] = status.num_connections;
    status_value["status_string"] = status.status_string;
    status_value["estimated_netspace"] = recently_netspace_querier_();

    Json::Value last_blk_info_value;
    last_blk_info_value["hash"] = Uint256ToHex(status.last_block_info.hash);
    last_blk_info_value["challenge"] = Uint256ToHex(status.last_block_info.challenge);
    last_blk_info_value["height"] = status.last_block_info.height;
    last_blk_info_value["filter_bits"] = status.last_block_info.filter_bits;
    last_blk_info_value["block_difficulty"] = status.last_block_info.block_difficulty;
    last_blk_info_value["challenge_difficulty"] = status.last_block_info.challenge_difficulty;
    last_blk_info_value["farmer_pk"] = BytesToHex(status.last_block_info.farmer_pk);
    last_blk_info_value["address"] = status.last_block_info.address;
    last_blk_info_value["reward"] = status.last_block_info.reward;
    last_blk_info_value["accumulate"] = status.last_block_info.accumulate;
    last_blk_info_value["vdf_time"] = status.last_block_info.vdf_time;
    last_blk_info_value["vdf_iters"] = status.last_block_info.vdf_iters;
    last_blk_info_value["vdf_speed"] = status.last_block_info.vdf_speed;
    last_blk_info_value["vdf_iters_req"] = status.last_block_info.vdf_iters_req;
    status_value["last_block_info"] = last_blk_info_value;

    status_value["vdf_pack"] = MakePackJson(status.vdf_pack);

    Supply supply = supply_querier_();

    Json::Value supply_json;
    supply_json["dist_height"] = supply.dist_height;

    Json::Value calc_json;
    calc_json["height"] = supply.calc.height;
    calc_json["burned"] = supply.calc.burned;
    calc_json["total"] = supply.calc.total;
    calc_json["actual"] = supply.calc.actual;

    Json::Value last_json;
    last_json["height"] = supply.last.height;
    last_json["burned"] = supply.last.burned;
    last_json["total"] = supply.last.total;
    last_json["actual"] = supply.last.actual;
    supply_json["calc"] = calc_json;
    supply_json["last"] = last_json;
    status_value["supply"] = supply_json;

    PledgeInfo pledge_info = pledge_info_querier_();
    Json::Value pledge_info_json;
    pledge_info_json["retarget_min_heights"] = pledge_info.retarget_min_heights;
    pledge_info_json["capacity_eval_window"] = pledge_info.capacity_eval_window;
    Json::Value pledges_json(Json::arrayValue);
    for (int i = 0; i < pledge_info.pledges.size(); ++i) {
        auto const& pledge = pledge_info.pledges[i];
        Json::Value pledge_json;
        pledge_json["lock_height"] = pledge.lock_height;
        pledge_json["actual_percent"] = pledge.actual_percent;
        pledges_json.append(std::move(pledge_json));
    }
    pledge_info_json["pledges"] = pledges_json;
    status_value["pledge_info"] = pledge_info_json;

    // prepare body
    return PrepareResponseWithContent(http::status::ok, status_value, request.version(), request.keep_alive());
}

http::message_generator VDFWebService::Handle_API_Summary(http::request<http::string_body> const& request)
{
    auto [hours_str, ok] = ParseUrlParameter(request.target(), "hours");
    if (!ok) {
        return PrepareResponseWithError(http::status::bad_request, "missing parameter `hours=?`", request.version(), request.keep_alive());
    }

    int pass_hours = std::atoi(hours_str.data());
    if (pass_hours <= 0 || pass_hours > 24 * 7) {
        return PrepareResponseWithError(http::status::bad_request, "the value of `hours` is invalid", request.version(), request.keep_alive());
    }

    int num_heights = num_heights_by_hours_querier_(pass_hours);
    auto blocks = block_info_range_querier_(num_heights);

    int segs[] = { 3, 5, 10, 30, 60 };
    std::map<int, int> summary;
    for (int seg : segs) {
        summary.insert_or_assign(seg, 0);
    }
    int total_duration { 0 };
    for (auto const& block : blocks) {
        int duration = block.vdf_iters / block.vdf_speed;
        total_duration += duration;
        int min = duration / 60;
        if (duration % 60 > 0) {
            ++min;
        }
        for (int seg : segs) {
            if (min <= seg) {
                ++summary[seg];
                break;
            }
        }
    }

    Json::Value res_json;
    res_json["num_blocks"] = static_cast<Json::Int64>(blocks.size());
    res_json["hours"] = pass_hours;
    res_json["high_height"] = blocks.empty() ? 0 : blocks.front().height;
    res_json["low_height"] = blocks.empty() ? 0 : blocks.back().height;
    res_json["avg_duration"] = static_cast<Json::Int64>(blocks.empty() ? 0 : total_duration / blocks.size());

    Json::Value summary_json(Json::arrayValue);
    for (auto entry : summary) {
        Json::Value entry_json;
        entry_json["min"] = entry.first;
        entry_json["count"] = entry.second;
        summary_json.append(std::move(entry_json));
    }
    res_json["summary"] = summary_json;

    return PrepareResponseWithContent(http::status::ok, res_json, request.version(), request.keep_alive());
}

template <typename T, typename OriginalValueQuerierType> T CalcSimValue(int index, int query_n, OriginalValueQuerierType original_val_querier, int original_n)
{
    int original_index = (static_cast<double>(index) / query_n) * original_n;
    int n = std::max(original_n / query_n, 1);
    T total { 0 }, count { 0 };
    for (int i = 0; i < n; ++i) {
        int index = original_index + i;
        if (index >= original_n) {
            break;
        }
        total += original_val_querier(index);
        ++count;
    }
    return count > 0 ? total / count : 0;
}

http::message_generator VDFWebService::Handle_API_Netspace(http::request<http::string_body> const& request)
{
    auto [hours_str, ok] = ParseUrlParameter(request.target(), "hours");
    if (!ok) {
        return PrepareResponseWithError(http::status::bad_request, "missing parameter `hours=?`", request.version(), request.keep_alive());
    }

    int pass_hours = std::atoi(hours_str.data());
    if (pass_hours <= 0 || pass_hours > 24 * 7) {
        return PrepareResponseWithError(http::status::bad_request, "the value of `hours` is invalid", request.version(), request.keep_alive());
    }

    Json::Value res_json;

    int num_heights = num_heights_by_hours_querier_(pass_hours);
    auto data = netspace_querier_(num_heights);
    int const DATA_N = 1000;
    for (int i = DATA_N - 1; i >= 0; --i) {
        Json::Value data_val;
        data_val["height"] = CalcSimValue<int>(
                i, DATA_N,
                [&data](int original_index) {
                    return data[original_index].height;
                },
                data.size());
        data_val["challenge_difficulty"] = CalcSimValue<uint64_t>(
                i, DATA_N,
                [&data](int original_index) {
                    return data[original_index].challenge_difficulty;
                },
                data.size());
        data_val["block_difficulty"] = CalcSimValue<uint64_t>(
                i, DATA_N,
                [&data](int original_index) {
                    return data[original_index].block_difficulty;
                },
                data.size());
        data_val["netspace"] = CalcSimValue<uint64_t>(
                i, DATA_N,
                [&data](int original_index) {
                    return data[original_index].netspace;
                },
                data.size());
        res_json.append(std::move(data_val));
    }

    return PrepareResponseWithContent(http::status::ok, res_json, request.version(), request.keep_alive());
}

http::message_generator VDFWebService::Handle_API_Rank(http::request<http::string_body> const& request)
{
    auto [hours_str, ok] = ParseUrlParameter(request.target(), "hours");
    int pass_hours { 0 };
    if (ok) {
        pass_hours = std::atoi(hours_str.c_str());
    }
    Json::Value res_json;

    auto [ranks, begin_height] = rank_querier_(pass_hours);
    res_json["begin_height"] = begin_height;

    std::sort(std::begin(ranks), std::end(ranks), [](auto const& lhs, auto const& rhs) {
        return lhs.produced_blocks > rhs.produced_blocks;
    });

    Json::Value entries_json(Json::arrayValue);
    for (auto const& entry : ranks) {
        Json::Value entry_json;
        entry_json["farmer_pk"] = entry.farmer_pk;
        entry_json["count"] = entry.produced_blocks;
        entry_json["average_difficulty"] = entry.average_difficulty;
        entry_json["total_reward"] = entry.total_reward;
        entries_json.append(std::move(entry_json));
    }
    res_json["entries"] = entries_json;

    return PrepareResponseWithContent(http::status::ok, res_json, request.version(), request.keep_alive());
}
