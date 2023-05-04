#include "frontend_client.h"

#include <fmt/core.h>
#include <plog/Log.h>

#include "timelord_utils.h"

using boost::system::error_code;

FrontEndClient::FrontEndClient(asio::io_context& ioc)
    : ioc_(ioc)
{
}

void FrontEndClient::SetConnectionHandler(ConnectionHandler conn_handler)
{
    conn_handler_ = std::move(conn_handler);
}

void FrontEndClient::SetMessageHandler(MessageHandler msg_handler)
{
    msg_handler_ = std::move(msg_handler);
}

void FrontEndClient::SetErrorHandler(ErrorHandler err_handler)
{
    err_handler_ = std::move(err_handler);
}

void FrontEndClient::SetCloseHandler(CloseHandler close_handler)
{
    close_handler_ = std::move(close_handler);
}

void FrontEndClient::Connect(std::string_view host, unsigned short port)
{
    // solve the address
    tcp::resolver r(ioc_);
    try {
        PLOGD << "resolving ip from " << host << "...";
        tcp::resolver::query q(std::string(host), std::to_string(port));
        auto it_result = r.resolve(q);
        if (it_result == std::cend(tcp::resolver::results_type())) {
            // cannot resolve the ip from host name
            throw std::runtime_error(fmt::format("cannot resolve host: `{}'", host));
        }
        // retrieve the first result and start the connection
        PLOGD << "connecting...";
        ps_ = std::make_unique<tcp::socket>(ioc_);
        ps_->async_connect(*it_result, [this, host = std::string(host), port](error_code const& ec) {
            PLOGD << "connected";
            if (ec) {
                PLOGE << ec.message();
                err_handler_(FrontEndSessionErrorType::CONNECT, ec.message());
                return;
            }
            DoReadNext();
            conn_handler_();
        });
    } catch (std::exception const& e) {
        PLOGE << e.what();
    }
}

void FrontEndClient::SendMessage(Json::Value const& msg)
{
    if (ps_ == nullptr) {
        throw std::runtime_error("please connect to server before sending message");
    }
    bool do_send = sending_msgs_.empty();
    sending_msgs_.push_back(msg.toStyledString());
    if (do_send) {
        DoSendNext();
    }
}

void FrontEndClient::SendShutdown()
{
    bool do_send = sending_msgs_.empty();
    sending_msgs_.push_back("shutdown");
    if (do_send) {
        DoSendNext();
    }
}

void FrontEndClient::Exit()
{
    if (ps_) {
        error_code ignored_ec;
        ps_->shutdown(tcp::socket::shutdown_both, ignored_ec);
        ps_->close(ignored_ec);
        ps_.reset();
        // callback
        close_handler_();
    }
}

void FrontEndClient::DoReadNext()
{
    asio::async_read_until(*ps_, read_buf_, '\0', [this](error_code const& ec, std::size_t bytes) {
        if (ec) {
            if (ec != asio::error::eof) {
                PLOGE << ec.message();
                err_handler_(FrontEndSessionErrorType::READ, ec.message());
            }
            Exit();
            return;
        }
        std::istreambuf_iterator<char> begin(&read_buf_), end;
        std::string result(begin, end);
        try {
            Json::Value msg = ParseStringToJson(result);
            msg_handler_(msg);
        } catch (std::exception const& e) {
            PLOGE << "READ: " << e.what();
            err_handler_(FrontEndSessionErrorType::READ, ec.message());
        }
        DoReadNext();
    });
}

void FrontEndClient::DoSendNext()
{
    assert(!sending_msgs_.empty());
    auto const& msg = sending_msgs_.front();
    send_buf_.resize(msg.size() + 1);
    memcpy(send_buf_.data(), msg.data(), msg.size());
    send_buf_[msg.size()] = '\0';
    asio::async_write(*ps_, asio::buffer(send_buf_), [this](error_code const& ec, std::size_t bytes) {
        if (ec) {
            err_handler_(FrontEndSessionErrorType::WRITE, ec.message());
            Exit();
            return;
        }
        sending_msgs_.pop_front();
        if (!sending_msgs_.empty()) {
            DoSendNext();
        }
    });
}
