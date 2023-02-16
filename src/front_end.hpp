#ifndef TL_FRONT_END_HPP
#define TL_FRONT_END_HPP

#include <string>
#include <string_view>

#include <memory>
#include <functional>
#include <deque>
#include <istream>
#include <sstream>

#include <asio.hpp>
using tcp = asio::ip::tcp;

#include <json/value.h>
#include <json/reader.h>

#include <plog/Log.h>

namespace fe {


class Session;
using SessionPtr = std::shared_ptr<Session>;
enum class ErrorType { CONNECT, READ, WRITE, CLOSE, PARSE };

using ErrorHandler = std::function<void(ErrorType type, std::string_view errs)>;
using MessageReceiver = std::function<void(std::string_view)>;

using SessionErrorHandler = std::function<void(SessionPtr psession, ErrorType type, std::string_view errs)>;
using SessionMessageReceiver = std::function<void(SessionPtr psession, Json::Value const& msg)>;


class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket&& s) : s_(std::move(s)) {}

    void Start(MessageReceiver msg_receiver) {
        msg_receiver_ = std::move(msg_receiver);
        DoReadNext();
    }

    void SendMessage(Json::Value const& value) {
        bool do_send = sending_msgs_.empty();
        sending_msgs_.push_back(value.toStyledString());
        if (do_send) {
            DoSendNext();
        }
    }

    void SetErrorHandler(ErrorHandler err_handler) {
        err_handler_ = std::move(err_handler);
    }

private:
    void DoSendNext() {
        assert(!sending_msgs_.empty());
        send_buf_ = sending_msgs_.front();
        send_buf_.resize(send_buf_.size() + 1);
        send_buf_[send_buf_.size()] = '\0';
        asio::async_write(s_, asio::buffer(send_buf_),
                [self = shared_from_this()](std::error_code const& ec, std::size_t bytes_wrote) {
                    PLOGD << "total " << bytes_wrote << " bytes wrote";
                    if (ec) {
                        if (self->err_handler_) {
                            self->err_handler_(ErrorType::WRITE, ec.message());
                        }
                        PLOGE << "WRITE: " << ec.message();
                        return;
                    }
                    assert(bytes_wrote == self->send_buf_.size());
                    self->sending_msgs_.pop_front();
                    if (!self->sending_msgs_.empty()) {
                        self->DoSendNext();
                    }
                });
    }

    void DoReadNext() {
        asio::async_read_until(s_, read_buf_, '\0',
                [self = shared_from_this()](std::error_code const& ec, std::size_t bytes_read) {
                    PLOGD << "total " << bytes_read << " bytes are read";
                    if (ec) {
                        if (ec == asio::error::eof) {
                            if (self->err_handler_) {
                                self->err_handler_(ErrorType::CLOSE, ec.message());
                            }
                            PLOGI << "READ: end of file";
                        } else {
                            if (self->err_handler_) {
                                self->err_handler_(ErrorType::READ, ec.message());
                            }
                            PLOGE << "READ: " << ec.message();
                        }
                        return;
                    }
                    std::istreambuf_iterator<char> begin(&self->read_buf_), end;
                    std::string result(begin, end);
                    assert(self->msg_receiver_);
                    self->msg_receiver_(result);
                    // read next
                    self->DoReadNext();
                });
    }

    tcp::socket s_;
    asio::streambuf read_buf_;
    std::string send_buf_;
    MessageReceiver msg_receiver_;
    std::deque<std::string> sending_msgs_;
    ErrorHandler err_handler_;
};

class FrontEnd {
public:
    FrontEnd(asio::io_context& ioc, unsigned short port, SessionMessageReceiver session_msg_receiver)
            : acceptor_(ioc), session_msg_receiver_(session_msg_receiver) {
        tcp::endpoint endpoint(tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        PLOGI << "Listening on port: " << port;
    }

    void SetErrorHandler(SessionErrorHandler err_handler) {
        err_handler_ = err_handler;
    }

private:
    void DoAcceptNext() {
        acceptor_.async_accept(
                [this](std::error_code const& ec, tcp::socket&& s) {
                    if (ec) {
                        if (err_handler_) {
                            err_handler_({}, ErrorType::CONNECT, ec.message());
                        }
                        PLOGE << "CONNECT: " << ec.message();
                        return;
                    }
                    PLOGD << "Accepting new session...";
                    auto psession = std::make_shared<Session>(std::move(s));
                    psession->SetErrorHandler(
                            [this, psession](ErrorType type, std::string_view errs) {
                                if (err_handler_) {
                                    err_handler_(psession, type, errs);
                                }
                            });
                    psession->Start([this, psession](std::string_view msg) {
                        Json::CharReaderBuilder builder;
                        std::shared_ptr<Json::CharReader> reader(builder.newCharReader());
                        Json::Value value;
                        std::string errs;
                        if (reader->parse(std::cbegin(msg), std::cend(msg), &value, &errs)) {
                            assert(session_msg_receiver_);
                            session_msg_receiver_(psession, value);
                        } else {
                            if (err_handler_) {
                                err_handler_(psession, ErrorType::PARSE, "the string cannot be parsed into json object");
                            }
                            PLOGE << "PARSE: the string cannot be parsed into json object";
                        }
                    });
                });
    }

    tcp::acceptor acceptor_;
    SessionMessageReceiver session_msg_receiver_;
    SessionErrorHandler err_handler_;
};

} // namespace fe

#endif
