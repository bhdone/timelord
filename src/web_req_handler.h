#ifndef WEB_REQ_HANDLER_H
#define WEB_REQ_HANDLER_H

#include <map>

#include "web_service.hpp"

struct WebReqHandler {
public:
    void Register(RequestTarget req_target, RequestHandler&& handler);

    http::message_generator Handle(http::request<http::string_body> const& request) const;

private:
    std::map<RequestTarget, RequestHandler> handlers_;
};

http::response<http::string_body> PrepareResponse(http::status status, unsigned int version, bool keep_alive);

http::response<http::string_body> PrepareResponseWithError(http::status status, std::string_view error, unsigned int version, bool keep_alive);

namespace Json {

class Value;

} // namespace Json

http::response<http::string_body> PrepareResponseWithContent(http::status status, Json::Value const& json, unsigned int version, bool keep_alive);

std::string BodyError(std::string_view why);

#endif
