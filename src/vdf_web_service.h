#ifndef VDF_WEB_SERVICE_H
#define VDF_WEB_SERVICE_H

#include <string_view>

#include <vector>

#include "block_info.h"

#include "querier_defs.h"

#include "web_req_handler.h"
#include "web_service.hpp"

class VDFWebService
{
public:
    VDFWebService(asio::io_context& ioc, std::string_view addr, uint16_t port, int expired_after_secs, std::string api_path_prefix, int fork_height, NumHeightsByHoursQuerierType num_heights_by_hours_querier, BlockInfoRangeQuerierType block_info_range_querier, NetspaceQuerierType netspace_querier, TimelordStatusQuerierType status_querier, RankQuerierType rank_querier, SupplyQuerierType supply_querier, PledgeInfoQuerierType pledge_info_querier, RecentlyNetspaceSizeQuerierType recently_netspace_querier);

    void Run();

    void Stop();

private:
    http::message_generator HandleRequest(http::request<http::string_body> const& request);

    http::message_generator Handle_API_Status(http::request<http::string_body> const& request);

    http::message_generator Handle_API_Summary(http::request<http::string_body> const& request);

    http::message_generator Handle_API_Netspace(http::request<http::string_body> const& request);

    http::message_generator Handle_API_Rank(http::request<http::string_body> const& request);

    WebService web_service_;
    WebReqHandler web_req_handler_;
    int fork_height_;
    NumHeightsByHoursQuerierType num_heights_by_hours_querier_;
    BlockInfoRangeQuerierType block_info_range_querier_;
    NetspaceQuerierType netspace_querier_;
    TimelordStatusQuerierType status_querier_;
    RankQuerierType rank_querier_;
    SupplyQuerierType supply_querier_;
    PledgeInfoQuerierType pledge_info_querier_;
    RecentlyNetspaceSizeQuerierType recently_netspace_querier_;
};

#endif
