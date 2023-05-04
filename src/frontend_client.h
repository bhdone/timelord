#ifndef TL_FRONTEND_CLIENT_H
#define TL_FRONTEND_CLIENT_H

#include <functional>
#include <memory>

#include <deque>
#include <vector>

#include "asio_defs.hpp"

#include <json/value.h>

#include "frontend.h"

class FrontEndClient
{
public:
    using ConnectionHandler = std::function<void()>;
    using MessageHandler = std::function<void(Json::Value const&)>;
    using ErrorHandler = std::function<void(FrontEndSessionErrorType err_type, std::string_view errs)>;
    using CloseHandler = std::function<void()>;

    explicit FrontEndClient(asio::io_context& ioc);

    void SetConnectionHandler(ConnectionHandler conn_handler);

    void SetMessageHandler(MessageHandler msg_handler);

    void SetErrorHandler(ErrorHandler err_handler);

    void SetCloseHandler(CloseHandler close_handler);

    void Connect(std::string_view host, unsigned short port);

    void SendMessage(Json::Value const& msg);

    void SendShutdown();

    void Exit();

private:
    void DoReadNext();

    void DoSendNext();

    asio::io_context& ioc_;
    std::unique_ptr<tcp::socket> ps_;
    asio::streambuf read_buf_;
    std::vector<uint8_t> send_buf_;
    std::deque<std::string> sending_msgs_;
    ConnectionHandler conn_handler_;
    MessageHandler msg_handler_;
    ErrorHandler err_handler_;
    CloseHandler close_handler_;
};

#endif
