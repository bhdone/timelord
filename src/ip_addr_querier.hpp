#ifndef IP_ADDR_QUERIER_HPP
#define IP_ADDR_QUERIER_HPP

#include <string>

#include <regex>

#include "asio_defs.hpp"

#include "https_querier.h"

class IpAddrQuerier
{
    char const* SZ_HOST = "dynamicdns.park-your-domain.com";

public:
    struct Ip {
        uint8_t p1;
        uint8_t p2;
        uint8_t p3;
        uint8_t p4;
    };

    static std::string ToString(Ip const& ip)
    {
        std::stringstream ss;
        ss << static_cast<int>(ip.p1) << "." << static_cast<int>(ip.p2) << "." << static_cast<int>(ip.p3) << "." << static_cast<int>(ip.p4);
        return ss.str();
    }

    static Ip StripIpAddr(std::string const& str)
    {
        std::regex re("(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)");
        std::smatch ma;
        if (!std::regex_match(str, ma, re)) {
            throw std::runtime_error("no ip address can be found");
        }
        Ip ip;
        ip.p1 = std::atoi(ma[1].str().c_str());
        ip.p2 = std::atoi(ma[2].str().c_str());
        ip.p3 = std::atoi(ma[3].str().c_str());
        ip.p4 = std::atoi(ma[4].str().c_str());
        return ip;
    }

    Ip operator()() const
    {
        asio::io_context ioc;
        return StripIpAddr(HttpsQuery(ioc, SZ_HOST, 443, "/getip"));
    }
};

#endif
