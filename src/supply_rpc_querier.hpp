#ifndef SUPPLY_RPC_QUERIER_HPP
#define SUPPLY_RPC_QUERIER_HPP

#include "rpc_client.h"

class SupplyRPCQuerier
{
public:
    explicit SupplyRPCQuerier(RPCClient& rpc)
        : rpc_(rpc)
    {
    }

    Supply operator()() const
    {
        auto res = rpc_.Call("querysupply", std::string("0"));
        if (!res.result.isObject()) {
            throw std::runtime_error("the type of result from `querysupply` is not object");
        }

        if (!res.result.exists("calc")) {
            throw std::runtime_error("member `calc` cannot be found");
        }
        auto calcObj = res.result["calc"];

        if (!res.result.exists("last")) {
            throw std::runtime_error("member `last` cannot be found");
        }
        auto lastObj = res.result["last"];

        Supply supply;

        supply.dist_height = res.result["dist_height"].get_int();

        supply.calc.height = calcObj["calc_height"].get_int();
        supply.calc.burned = calcObj["burned"].get_real();
        supply.calc.total = calcObj["total_supplied"].get_real();
        supply.calc.actual = calcObj["actual_supplied"].get_real();

        supply.last.height = lastObj["last_height"].get_int();
        supply.last.burned = lastObj["burned"].get_real();
        supply.last.total = lastObj["total_supplied"].get_real();
        supply.last.actual = lastObj["actual_supplied"].get_real();

        return supply;
    }

private:
    RPCClient& rpc_;
};

#endif
