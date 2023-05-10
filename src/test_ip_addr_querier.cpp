#include <gtest/gtest.h>

#include "ip_addr_querier.hpp"

TEST(IpAddrQuerier, IpToStr)
{
    IpAddrQuerier::Ip ip { 222, 218, 208, 245 };
    std::string ip_str = IpAddrQuerier::ToString(ip);
    EXPECT_EQ(ip_str, "222.218.208.245");
}

TEST(IpAddrQuerier, StripIpAddr)
{
    char const* SZ_IP = "52.9.29.12";
    IpAddrQuerier::Ip ip;
    EXPECT_NO_THROW({ ip = IpAddrQuerier::StripIpAddr(SZ_IP); });
    EXPECT_EQ(IpAddrQuerier::ToString(ip), SZ_IP);
}

TEST(IpAddrQuerier, Base)
{
    EXPECT_NO_THROW({ IpAddrQuerier()(); });
}
