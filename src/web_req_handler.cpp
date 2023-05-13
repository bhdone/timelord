#include "web_req_handler.h"

#include <json/json.h>

#include <boost/url.hpp>

namespace urls = boost::urls;

std::string BodyError(std::string_view why)
{
    Json::Value json;
    json["err"] = std::string(why);
    return json.toStyledString();
}

std::string BodyInvalidMethod()
{
    return BodyError("the method cannot be found");
}

std::string BodyBadRequest()
{
    return BodyError("the request is illegal");
}

void PrepareResponseHeaders(http::response<http::string_body>& response)
{
    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(http::field::content_type, "json/application");
    response.set(http::field::access_control_allow_origin, "*");
}

http::response<http::string_body> PrepareResponse(http::status status, unsigned int version, bool keep_alive)
{
    http::response<http::string_body> response(status, version);
    PrepareResponseHeaders(response);
    response.keep_alive(keep_alive);
    return response;
}

http::response<http::string_body> PrepareResponseWithError(http::status status, std::string_view error, unsigned int version, bool keep_alive)
{
    http::response<http::string_body> response(status, version);
    PrepareResponseHeaders(response);
    response.keep_alive(keep_alive);
    response.body() = BodyError(error);
    response.prepare_payload();
    return response;
}

http::response<http::string_body> PrepareResponseWithContent(http::status status, Json::Value const& json, unsigned int version, bool keep_alive)
{
    http::response<http::string_body> response(status, version);
    PrepareResponseHeaders(response);
    response.keep_alive(keep_alive);
    response.body() = json.toStyledString();
    response.prepare_payload();
    return response;
}

bool ValidTarget(std::string_view target)
{
    return !target.empty() && target[0] == '/' && target.find("..") == std::string_view::npos;
}

void WebReqHandler::Register(RequestTarget req_target, RequestHandler&& handler)
{
    handlers_.insert_or_assign(req_target, std::move(handler));
}

http::message_generator WebReqHandler::Handle(http::request<http::string_body> const& request) const
{
    if (!ValidTarget(request.target())) {
        auto response = PrepareResponse(http::status::bad_request, request.version(), request.keep_alive());
        if (request.method() != http::verb::head) {
            response.body() = BodyBadRequest();
            response.prepare_payload();
        }
        return response;
    }

    auto path_result = urls::parse_origin_form(request.target());
    std::string req_path = path_result->path();

    auto it = handlers_.find(std::make_pair(request.method(), req_path));
    if (it == std::cend(handlers_)) {
        auto response = PrepareResponse(http::status::not_found, request.version(), request.keep_alive());
        if (request.method() != http::verb::head) {
            response.body() = BodyInvalidMethod();
            response.prepare_payload();
        }
        return response;
    }

    return it->second(request);
}
