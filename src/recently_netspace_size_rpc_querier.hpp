#ifndef RECENTLYNETSPACESIZERPCQUERIER_HPP
#define RECENTLYNETSPACESIZERPCQUERIER_HPP

class RecentlyNetspaceSizeRPCQuerier
{
public:
    explicit RecentlyNetspaceSizeRPCQuerier(RPCClient& rpc)
        : rpc_(rpc)
    {
    }

    uint64_t operator()() const
    {
        auto res = rpc_.Call("querynetspace");
        if (!res.result.isObject()) {
            throw std::runtime_error("require an object but the service didn't return one.");
        }
        return res.result["netspace"].get_int64();
    }

private:
    RPCClient& rpc_;
};

#endif
