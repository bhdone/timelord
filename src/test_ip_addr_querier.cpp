#include <gtest/gtest.h>

#include "ip_addr_querier.hpp"

TEST(IpAddrQuerier, IpToStr)
{
    IpAddrQuerier::Ip ip { 222, 218, 208, 245 };
    std::string ip_str = IpAddrQuerier::ToString(ip);
    EXPECT_EQ(ip_str, "222.218.208.245");
}

TEST(IpAddrQuerier, Base)
{
    EXPECT_NO_THROW({ IpAddrQuerier()(); });
}
