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

std::string BodyError(std::string_view why);

#endif
