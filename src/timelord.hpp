#ifndef TL_TIMELORD_HPP
#define TL_TIMELORD_HPP

#include <map>
#include <functional>

#include <string_view>

#include <plog/Log.h>

#include "front_end.hpp"

using std::placeholders::_1;
using std::placeholders::_2;

class Timelord
{
public:
    Timelord() : fe_(ioc_)
    {
        // TODO register handlers
    }

    int Run(std::string_view addr, unsigned short port, std::string_view vdf_client_path)
    {
        try {
            fe_.Run(addr, port, std::bind(&fe::MessageDispatcher::DispatchMessage, &msg_dispatcher_, _1, _2));
            PLOGI << "Timelord is running...";
            ioc_.run();
        } catch (std::exception const& e) {
            PLOGE << e.what();
        }
        return 0;
    }

private:
    asio::io_context ioc_;
    fe::FrontEnd fe_;
    fe::MessageDispatcher msg_dispatcher_;
};

#endif
