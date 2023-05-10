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

Json::Value MakeResultJson(VDFResult const& result)
{
    Json::Value res;
    res["challenge"] = Uint256ToHex(result.challenge);
    res["iters"] = result.iters;
    res["y"] = BytesToHex(result.y);
    res["proof"] = BytesToHex(result.proof);
    res["witness_type"] = result.witness_type;
    res["duration"] = result.duration;
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

    Json::Value result_values(Json::arrayValue);
    for (auto const& result : pack.results) {
        result_values.append(MakeResultJson(result));
    }
    res["results"] = result_values;

    return res;
}

VDFWebService::VDFWebService(asio::io_context& ioc, std::string_view addr, uint16_t port, int expired_after_secs, NumHeightsByHoursQuerierType num_heights_by_hours_querier, BlockInfoRangeQuerierType block_info_range_querier, TimelordStatusQuerierType status_querier)
    : web_service_(ioc, tcp::endpoint(asio::ip::address::from_string(std::string(addr)), port), expired_after_secs, std::bind(&VDFWebService::HandleRequest, this, _1))
    , num_heights_by_hours_querier_(std::move(num_heights_by_hours_querier))
    , block_info_range_querier_(std::move(block_info_range_querier))
    , status_querier_(std::move(status_querier))
{
    web_req_handler_.Register(std::make_pair(http::verb::get, "/api/summary"), std::bind(&VDFWebService::Handle_API_Summary, this, _1));
    web_req_handler_.Register(std::make_pair(http::verb::get, "/api/status"), std::bind(&VDFWebService::Handle_API_Status, this, _1));
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
    http::response<http::string_body> response;
    if (request.method() == http::verb::head) {
        return response;
    }

    if (my_ip_str_.empty()) {
        try {
            my_ip_str_ = IpAddrQuerier::ToString(IpAddrQuerier()());
        } catch (std::exception const& e) {
            PLOGE << tinyformat::format("cannot retrieve server ip address, reason: %s", e.what());
        }
    }

    response = PrepareResponse(http::status::ok, request.version(), request.keep_alive());
    auto status = status_querier_();

    Json::Value status_value;
    status_value["server_ip"] = my_ip_str_;
    status_value["challenge"] = Uint256ToHex(status.challenge);
    status_value["height"] = status.height;
    status_value["iters_per_sec"] = status.iters_per_sec;
    status_value["total_size"] = status.total_size;

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
    status_value["last_block_info"] = last_blk_info_value;

    status_value["vdf_pack"] = MakePackJson(status.vdf_pack);

    // prepare body
    response.body() = status_value.toStyledString();
    response.prepare_payload();

    return response;
}

http::message_generator VDFWebService::Handle_API_Summary(http::request<http::string_body> const& request)
{
    http::response<http::string_body> response;
    if (request.method() == http::verb::head) {
        return response;
    }

    response = PrepareResponse(http::status::ok, request.version(), request.keep_alive());
    auto req_path = urls::parse_origin_form(request.target());
    auto it_hours = req_path->params().find("hours");
    if (it_hours == std::cend(req_path->params())) {
        response = PrepareResponse(http::status::bad_request, request.version(), request.keep_alive());
        response.body() = BodyError("missing parameter `hours=?`");
        response.prepare_payload();
        return response;
    }

    std::string hours_str = (*it_hours).value;
    int hours = std::atoi(hours_str.data());
    if (hours <= 0 || hours > 24 * 7) {
        response = PrepareResponse(http::status::bad_request, request.version(), request.keep_alive());
        response.body() = BodyError("the value of `hours` is invalid");
        response.prepare_payload();
        return response;
    }

    int num_heights = num_heights_by_hours_querier_(hours);
    auto blocks = block_info_range_querier_(num_heights);

    int segs[] = { 3, 10, 30, 60 };
    std::map<int, int> summary;
    for (int seg : segs) {
        summary.insert_or_assign(seg, 0);
    }
    for (auto const& block : blocks) {
        int duration = block.vdf_iters / block.vdf_speed;
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
    res_json["num_blocks"] = blocks.size();
    res_json["hours"] = hours;
    res_json["high_height"] = blocks.front().height;
    res_json["low_height"] = blocks.back().height;

    Json::Value summary_json(Json::arrayValue);
    for (auto entry : summary) {
        Json::Value entry_json;
        entry_json["min"] = entry.first;
        entry_json["count"] = entry.second;
        summary_json.append(std::move(entry_json));
    }
    res_json["summary"] = summary_json;

    response.body() = res_json.toStyledString();
    response.prepare_payload();

    return response;
}
