#include "frontend.h"

#include <json/reader.h>
#include <json/value.h>

#include <plog/Log.h>
#include <tinyformat.h>

#include "msg_ids.h"

#include "timelord_utils.h"

using boost::system::error_code;

static constexpr int RESPOND_TIMEOUT_SECONDS = 120;

FrontEndSession::FrontEndSession(asio::io_context& ioc, tcp::socket&& s)
    : ioc_(ioc)
    , s_(std::move(s))
{
    PLOGD << "Session " << AddressToString(this) << " is created";
}

FrontEndSession::~FrontEndSession()
{
    PLOGD << "Session " << AddressToString(this) << " is going to be released";
}

void FrontEndSession::SetMessageHandler(MessageHandler msg_handler)
{
    msg_handler_ = std::move(msg_handler);
}

void FrontEndSession::SetErrorHandler(ErrorHandler err_handler)
{
    err_handler_ = std::move(err_handler);
}

void FrontEndSession::Start()
{
    ResetTimeoutTimer();
    DoReadNext();
}

void FrontEndSession::SendMessage(Json::Value const& value)
{
    bool do_send = sending_msgs_.empty();
    sending_msgs_.push_back(value.toStyledString());
    if (do_send) {
        DoSendNext();
    }
}

void FrontEndSession::Stop()
{
    error_code ignored_ec;
    if (timeout_timer_) {
        timeout_timer_->cancel(ignored_ec);
    }
    s_.shutdown(tcp::socket::shutdown_both, ignored_ec);
    s_.close(ignored_ec);
    PLOGD << "Session " << AddressToString(this) << " is closed";
}

void FrontEndSession::DoSendNext()
{
    assert(!sending_msgs_.empty());
    send_buf_ = sending_msgs_.front();
    send_buf_.resize(send_buf_.size() + 1);
    send_buf_[send_buf_.size()] = '\0';
    asio::async_write(s_, asio::buffer(send_buf_), [self = shared_from_this()](error_code const& ec, std::size_t bytes_wrote) {
        if (ec) {
            if (self->err_handler_) {
                self->err_handler_(self, FrontEndSessionErrorType::WRITE, ec.message());
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

void FrontEndSession::DoReadNext()
{
    asio::async_read_until(s_, read_buf_, '\0', [self = shared_from_this()](error_code const& ec, std::size_t bytes_read) {
        if (ec) {
            if (ec == asio::error::eof) {
                if (self->err_handler_) {
                    self->err_handler_(self, FrontEndSessionErrorType::CLOSE, ec.message());
                }
                PLOGD << "READ: end of file";
            } else {
                if (self->err_handler_) {
                    self->err_handler_(self, FrontEndSessionErrorType::READ, ec.message());
                }
                PLOGE << "READ: " << ec.message();
            }
            return;
        }
        std::string result = static_cast<char const*>(self->read_buf_.data().data());
        self->read_buf_.consume(bytes_read);
        self->ResetTimeoutTimer();
        try {
            Json::Value msg = ParseStringToJson(result);
            // Check if it is ping
            auto msg_id = msg["id"].asInt();
            if (msg_id == static_cast<int>(TimelordClientMsgs::PING)) {
                // Just simply send it back
                msg["id"] = static_cast<Json::Int>(TimelordMsgs::PONG);
                self->SendMessage(msg);
            } else {
                self->msg_handler_(self, msg);
            }
        } catch (std::exception const& e) {
            PLOGE << "READ: failed to parse string into json: " << e.what();
            PLOGE << "DATA total=" << bytes_read << ": " << result;
            self->err_handler_(self, FrontEndSessionErrorType::READ, ec.message());
        }
        // read next
        self->DoReadNext();
    });
}

void FrontEndSession::ResetTimeoutTimer()
{
    timeout_timer_ = std::make_unique<asio::steady_timer>(ioc_);
    timeout_timer_->expires_after(std::chrono::seconds(RESPOND_TIMEOUT_SECONDS));
    timeout_timer_->async_wait([self_weak = std::weak_ptr(shared_from_this())](error_code const& ec) {
        if (ec) {
            return;
        }
        auto self = self_weak.lock();
        if (self) {
            self->err_handler_(self, FrontEndSessionErrorType::READ, "timeout");
        }
    });
}

FrontEnd::FrontEnd(asio::io_context& ioc)
    : ioc_(ioc)
    , acceptor_(ioc)
{
}

void FrontEnd::SetConnectionHandler(FrontEndSession::ConnectionHandler conn_handler)
{
    conn_handler_ = std::move(conn_handler);
}

void FrontEnd::SetMessageHandler(FrontEndSession::MessageHandler msg_handler)
{
    msg_handler_ = std::move(msg_handler);
}

void FrontEnd::SetErrorHandler(FrontEndSession::ErrorHandler err_handler)
{
    err_handler_ = err_handler;
}

void FrontEnd::Run(std::string_view addr, unsigned short port)
{
    // prepare to listen
    tcp::endpoint endpoint(asio::ip::address::from_string(std::string(addr)), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    PLOGD << "Listening on port: " << port;
    DoAcceptNext();
}

void FrontEnd::Exit()
{
    asio::post(ioc_, [this]() {
        for (auto psession : session_vec_) {
            psession->Stop();
        }
        error_code ignored_ec;
        acceptor_.cancel(ignored_ec);
        acceptor_.close(ignored_ec);
    });
}

std::size_t FrontEnd::GetNumOfSessions() const
{
    return num_of_sessions_;
}

void FrontEnd::DoAcceptNext()
{
    acceptor_.async_accept([this](error_code const& ec, tcp::socket&& s) {
        if (ec) {
            if (err_handler_) {
                err_handler_({}, FrontEndSessionErrorType::CONNECT, ec.message());
            }
            PLOGE << "CONNECT: " << ec.message();
            return;
        }
        auto psession = std::make_shared<FrontEndSession>(ioc_, std::move(s));
        psession->SetErrorHandler([this](FrontEndSessionPtr psession, FrontEndSessionErrorType type, std::string_view errs) {
            psession->Stop();
            auto it = std::find(std::begin(session_vec_), std::end(session_vec_), psession);
            if (it != std::end(session_vec_)) {
                session_vec_.erase(it);
                num_of_sessions_ = session_vec_.size();
            }
            // report to supervisor
            if (err_handler_) {
                err_handler_(psession, type, errs);
            }
        });
        psession->SetMessageHandler(msg_handler_);
        psession->Start();
        session_vec_.push_back(psession);
        num_of_sessions_ = session_vec_.size();
        conn_handler_(psession);
        DoAcceptNext();
    });
}
