#ifndef VDF_WEB_SERVICE_H
#define VDF_WEB_SERVICE_H

#include <string_view>

#include <vector>

#include "block_info.h"

#include "querier_defs.h"

#include "web_req_handler.h"
#include "web_service.hpp"

#include "local_sqlite_storage.h"

class VDFWebService
{
public:
    VDFWebService(asio::io_context& ioc, std::string_view addr, uint16_t port, int expired_after_secs, NumHeightsByHoursQuerierType num_heights_by_hours_querier, BlockInfoRangeQuerierType block_info_range_querier, TimelordStatusQuerierType status_querier);

    void Run();

    void Stop();

private:
    http::message_generator HandleRequest(http::request<http::string_body> const& request);

    http::message_generator Handle_API_Status(http::request<http::string_body> const& request);

    http::message_generator Handle_API_Summary(http::request<http::string_body> const& request);

    WebService web_service_;
    WebReqHandler web_req_handler_;
    NumHeightsByHoursQuerierType num_heights_by_hours_querier_;
    BlockInfoRangeQuerierType block_info_range_querier_;
    TimelordStatusQuerierType status_querier_;
};

#endif
