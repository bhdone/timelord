#ifndef WEB_SERVICE_HPP
#define WEB_SERVICE_HPP

#include <plog/Log.h>
#include <tinyformat.h>

#include "asio_defs.hpp"

#include <chrono>

#include <boost/beast.hpp>
namespace beast = boost::beast;
namespace http = boost::beast::http;

using boost::system::error_code;

using RequestHandler = std::function<http::message_generator(http::request<http::string_body> const&)>;

class WebSession : public std::enable_shared_from_this<WebSession>
{
public:
    WebSession(tcp::socket&& socket, int expired_after_secs, RequestHandler handler)
        : stream_(std::move(socket))
        , expired_after_secs_(expired_after_secs)
        , handler_(std::move(handler))
    {
    }

    void Run()
    {
        ReadNext();
    }

    void Close()
    {
        error_code ignored_ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ignored_ec);
    }

private:
    void SendResponse(http::message_generator&& generator)
    {
        bool keep_alive = generator.keep_alive();
        beast::async_write(stream_, std::move(generator), [self = shared_from_this(), keep_alive](error_code const& ec, std::size_t bytes_transferred) {
            if (ec) {
                PLOGE << tinyformat::format("%s: %s", __func__, ec.message());
                return self->Close();
            }
            if (!keep_alive) {
                return self->Close();
            }
            self->ReadNext();
        });
    }

    void ReadNext()
    {
        request_ = {};
        stream_.expires_after(std::chrono::seconds(expired_after_secs_));
        http::async_read(stream_, buffer_, request_, [self = shared_from_this()](error_code const& ec, std::size_t bytes_transferred) {
            if (ec) {
                if (ec != http::error::end_of_stream) {
                    PLOGE << tinyformat::format("%s: %s", __func__, ec.message());
                }
                return self->Close();
            }
            PLOGI << tinyformat::format("request: (%s) %s", self->request_.method_string(), self->request_.target());
            auto start_time = std::chrono::steady_clock::now();
            self->SendResponse(self->handler_(self->request_));
            PLOGI << tinyformat::format("%d msecs -> (%s) %s", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count(), self->request_.method_string(), self->request_.target());
        });
    }

    beast::tcp_stream stream_;
    int expired_after_secs_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    RequestHandler handler_;
};

class WebService
{
public:
    WebService(asio::io_context& ioc, tcp::endpoint const& endpoint, int expired_after_secs, RequestHandler handler)
        : ioc_(ioc)
        , acceptor_(ioc)
        , expired_after_secs_(expired_after_secs)
        , handler_(std::move(handler))
    {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

    void Run()
    {
        AcceptNext();
    }

    void Stop()
    {
        error_code ignored_ec;
        acceptor_.cancel(ignored_ec);
    }

private:
    void AcceptNext()
    {
        acceptor_.async_accept([this](error_code const& ec, tcp::socket socket) {
            if (ec) {
                PLOGE << tinyformat::format("%s: %s", __func__, ec.message());
                return;
            }
            std::make_shared<WebSession>(std::move(socket), expired_after_secs_, handler_)->Run();
            AcceptNext();
        });
    }

    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    int expired_after_secs_;
    RequestHandler handler_;
};

using RequestTarget = std::pair<http::verb, std::string>;

#endif
