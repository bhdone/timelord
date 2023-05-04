#include "vdf_web_service.h"

#include <json/json.h>

#include <functional>
using std::placeholders::_1;

#include <boost/url.hpp>
namespace urls = boost::urls;

#include "timelord_utils.h"

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

VDFWebService::VDFWebService(asio::io_context& ioc, std::string_view addr, uint16_t port, int expired_after_secs, VDFSQLitePersistOperator& persist_operator)
    : web_service_(ioc, tcp::endpoint(asio::ip::address::from_string(std::string(addr)), port), expired_after_secs, std::bind(&VDFWebService::HandleRequest, this, _1))
    , persist_operator_(persist_operator)
{
    web_req_handler_.Register(std::make_pair(http::verb::get, "/api/vdf"), std::bind(&VDFWebService::Handle_API_VDFRange, this, _1));
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

http::message_generator VDFWebService::Handle_API_VDFRange(http::request<http::string_body> const& request)
{
    http::response<http::string_body> response;
    if (request.method() == http::verb::head) {
        return response;
    }

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

    response = PrepareResponse(http::status::ok, request.version(), request.keep_alive());
    Json::Value pack_values(Json::arrayValue);
    auto packs = persist_operator_.QueryRecordsInHours(hours);
    for (auto const& pack : packs) {
        pack_values.append(MakePackJson(pack));
    }

    response.body() = pack_values.toStyledString();
    response.prepare_payload();
    return response;
}