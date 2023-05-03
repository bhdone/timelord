#ifndef VDF_WEB_SERVICE_H
#define VDF_WEB_SERVICE_H

#include <string_view>

#include "web_req_handler.h"
#include "web_service.hpp"

#include "local_sqlite_storage.h"

class VDFWebService
{
public:
    VDFWebService(asio::io_context& ioc, std::string_view addr, uint16_t port, int expired_after_secs, VDFSQLitePersistOperator& persist_operator);

    void Run();

    void Stop();

private:
    http::message_generator HandleRequest(http::request<http::string_body> const& request);

    http::message_generator Handle_API_VDFRange(http::request<http::string_body> const& request);

    WebService web_service_;
    WebReqHandler web_req_handler_;
    VDFSQLitePersistOperator& persist_operator_;
};

#endif
