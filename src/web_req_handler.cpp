#include "web_req_handler.h"

#include <json/json.h>

#include <boost/url.hpp>

namespace urls = boost::urls;

http::response<http::string_body> PrepareResponse(http::status status, unsigned int version, bool keep_alive)
{
    http::response<http::string_body> response(status, version);
    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(http::field::content_type, "json/application");
    response.keep_alive(keep_alive);
    return response;
}

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

    RequestTarget req_target;
    // deal with method `HEAD`
    if (request.method() == http::verb::head) {
        req_target = std::make_pair(http::verb::get, req_path);
    } else {
        req_target = std::make_pair(request.method(), req_path);
    }
    auto it = handlers_.find(req_target);
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
