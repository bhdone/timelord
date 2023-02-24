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
using asio::ip::tcp;

#include <json/value.h>
#include <json/reader.h>

#include <plog/Log.h>
#include <fmt/core.h>

namespace fe
{

class Session;
using SessionPtr = std::shared_ptr<Session>;
enum class ErrorType { CONNECT, READ, WRITE, CLOSE, PARSE, SHUTDOWN };

using ErrorHandler = std::function<void(ErrorType type, std::string_view errs)>;
using MessageReceiver = std::function<void(Json::Value const& msg)>;

using SessionConnectedHandler = std::function<void(SessionPtr psession)>;
using SessionErrorHandler = std::function<void(SessionPtr psession, ErrorType type, std::string_view errs)>;
using SessionMessageReceiver = std::function<void(SessionPtr psession, Json::Value const& msg)>;

inline Json::Value ParseStringToJson(std::string_view str)
{
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errs;
    bool succ = reader->parse(std::cbegin(str), std::cend(str), &root, &errs);
    if (!succ) {
        throw std::runtime_error(errs);
    }
    return root;
}

inline std::string AddressToString(void const* p)
{
    auto addr = reinterpret_cast<uint64_t>(p);
    std::stringstream ss;
    ss << "0x" << std::hex << addr;
    return ss.str();
}

class MessageDispatcher
{
public:
    using Handler = std::function<void(SessionPtr psession, Json::Value const& msg)>;

    void RegisterHandler(int id, Handler handler)
    {
        handlers_[id] = handler;
    }

    void DispatchMessage(SessionPtr psession, Json::Value const& msg)
    {
        if (!msg.isMember("id")) {
            PLOGE << "`id' is not found from the received message";
            PLOGD << msg.toStyledString();
            return;
        }
        auto id = msg["id"].asInt();
        auto it = handlers_.find(id);
        if (it == std::cend(handlers_)) {
            return;
        }
        it->second(psession, msg);
    }

private:
    std::map<int, Handler> handlers_;
};

class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(tcp::socket&& s) : s_(std::move(s))
    {
        PLOGD << "Session " << AddressToString(this) << " is created";
    }

    ~Session()
    {
        PLOGD << "Session " << AddressToString(this) << " is going to be released";
    }

    void Start(MessageReceiver msg_receiver)
    {
        msg_receiver_ = std::move(msg_receiver);
        DoReadNext();
    }

    void SendMessage(Json::Value const& value)
    {
        bool do_send = sending_msgs_.empty();
        sending_msgs_.push_back(value.toStyledString());
        if (do_send) {
            DoSendNext();
        }
    }

    void Close()
    {
        std::error_code ignored_ec;
        s_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        s_.close(ignored_ec);
        PLOGD << "Session " << AddressToString(this) << " is closed";
    }

    void SetErrorHandler(ErrorHandler err_handler)
    {
        err_handler_ = std::move(err_handler);
    }

private:
    void DoSendNext()
    {
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

    void DoReadNext()
    {
        asio::async_read_until(s_, read_buf_, '\0',
                [self = shared_from_this()](std::error_code const& ec, std::size_t bytes_read) {
                    PLOGD << "total " << bytes_read << " bytes are read";
                    if (ec) {
                        if (ec == asio::error::eof) {
                            if (self->err_handler_) {
                                self->err_handler_(ErrorType::CLOSE, ec.message());
                            }
                            PLOGD << "READ: end of file";
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
                    PLOGD << "==== msg received ==== size: " << result.size();
                    PLOGD << result;
                    PLOGD << "==== end of msg ====";
                    if (std::string(result.c_str()) == "shutdown") {
                        if (self->err_handler_) {
                            self->err_handler_(ErrorType::SHUTDOWN, "shutdown is requested");
                            return;
                        }
                        PLOGE << "shutdown is requested but the frontend didn't install a handler for errors, the request is ignored";
                    }
                    try {
                        Json::Value msg = ParseStringToJson(result);
                        self->msg_receiver_(msg);
                    } catch (std::exception const& e) {
                        PLOGE << "READ: failed to parse string into json: " << e.what();
                        self->err_handler_(ErrorType::READ, ec.message());
                    }
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
    explicit FrontEnd(asio::io_context& ioc)
        : acceptor_(ioc)
    {
    }

    void Run(std::string_view addr, unsigned short port, SessionConnectedHandler connected_handler, SessionMessageReceiver session_msg_receiver)
    {
        connected_handler_ = std::move(connected_handler);
        session_msg_receiver_ = std::move(session_msg_receiver);
        // prepare to listen
        tcp::endpoint endpoint(asio::ip::address::from_string(std::string(addr)), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        PLOGD << "Listening on port: " << port;
        DoAcceptNext();
    }

    template <typename Sender>
    void SendMsgToAllSessions(Sender sender)
    {
        for (auto psession : session_vec_) {
            sender(psession);
        }
    }

    void Close()
    {
        for (auto psession : session_vec_) {
            psession->Close();
        }
        session_vec_.clear();
        std::error_code ignored_ec;
        acceptor_.cancel(ignored_ec);
        acceptor_.close(ignored_ec);
    }

    void SetErrorHandler(SessionErrorHandler err_handler)
    {
        err_handler_ = err_handler;
    }

    std::size_t GetNumOfSessions() const
    {
        return session_vec_.size();
    }

private:
    void DoAcceptNext()
    {
        acceptor_.async_accept(
                [this](std::error_code const& ec, tcp::socket&& s) {
                    PLOGD << "Accepting new session...";
                    if (ec) {
                        if (err_handler_) {
                            err_handler_({}, ErrorType::CONNECT, ec.message());
                        }
                        PLOGE << "CONNECT: " << ec.message();
                        return;
                    }
                    auto psession = std::make_shared<Session>(std::move(s));
                    psession->SetErrorHandler(
                            [this, pweak_session = psession->weak_from_this()](ErrorType type, std::string_view errs) {
                                if (type == ErrorType::SHUTDOWN) {
                                    // shutdown the service
                                    Close();
                                } else if (err_handler_) {
                                    auto psession = pweak_session.lock();
                                    if (psession) {
                                        err_handler_(psession, type, errs);
                                    }
                                }
                            });
                    psession->Start([this, pweak_session = psession->weak_from_this()](Json::Value const& msg) {
                        auto psession = pweak_session.lock();
                        if (psession) {
                            session_msg_receiver_(psession, msg);
                        }
                    });
                    session_vec_.push_back(psession);
                    connected_handler_(psession);
                });
    }

    tcp::acceptor acceptor_;
    std::vector<SessionPtr> session_vec_;
    SessionConnectedHandler connected_handler_;
    SessionMessageReceiver session_msg_receiver_;
    SessionErrorHandler err_handler_;
};

class Client
{
public:
    using ConnectionHandler = std::function<void()>;
    using MessageHandler = std::function<void(Json::Value const&)>;
    using ErrorHandler = std::function<void(ErrorType err_type, std::string_view errs)>;
    using CloseHandler = std::function<void()>;

    explicit Client(asio::io_context& ioc) : ioc_(ioc) {}

    void Connect(std::string_view host, unsigned short port, ConnectionHandler conn_handler, MessageHandler msg_handler, ErrorHandler err_handler, CloseHandler close_handler)
    {
        conn_handler_ = std::move(conn_handler);
        msg_handler_ = std::move(msg_handler);
        err_handler_ = std::move(err_handler);
        close_handler_ = std::move(close_handler);
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
            ps_->async_connect(*it_result,
                    [this, host = std::string(host), port](std::error_code const& ec) {
                        PLOGD << "connected";
                        if (ec) {
                            PLOGE << ec.message();
                            err_handler_(ErrorType::CONNECT, ec.message());
                            return;
                        }
                        DoReadNext();
                        conn_handler_();
                    });
        } catch (std::exception const& e) {
            PLOGE << e.what();
        }
    }

    void SendMessage(Json::Value const& msg)
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

    void SendShutdown()
    {
        bool do_send = sending_msgs_.empty();
        sending_msgs_.push_back("shutdown");
        if (do_send) {
            DoSendNext();
        }
    }

    void Close()
    {
        if (ps_) {
            std::error_code ignored_ec;
            ps_->shutdown(tcp::socket::shutdown_both, ignored_ec);
            ps_->close(ignored_ec);
            ps_.reset();
            // callback
            close_handler_();
        }
    }

private:
    void DoReadNext()
    {
        asio::async_read_until(*ps_, read_buf_, '\0',
                [this](std::error_code const& ec, std::size_t bytes) {
                    if (ec) {
                        if (ec != asio::error::eof) {
                            PLOGE << ec.message();
                            err_handler_(ErrorType::READ, ec.message());
                        }
                        Close();
                        return;
                    }
                    std::istreambuf_iterator<char> begin(&read_buf_), end;
                    std::string result(begin, end);
                    try {
                        Json::Value msg = ParseStringToJson(result);
                        msg_handler_(msg);
                    } catch (std::exception const& e) {
                        PLOGE << "READ: " << e.what();
                        err_handler_(ErrorType::READ, ec.message());
                    }
                    DoReadNext();
                });
    }

    void DoSendNext()
    {
        assert(!sending_msgs_.empty());
        auto const& msg = sending_msgs_.front();
        send_buf_.resize(msg.size() + 1);
        memcpy(send_buf_.data(), msg.data(), msg.size());
        send_buf_[msg.size()] = '\0';
        asio::async_write(*ps_, asio::buffer(send_buf_),
                [this](std::error_code const& ec, std::size_t bytes) {
                    if (ec) {
                        err_handler_(ErrorType::WRITE, ec.message());
                        Close();
                        return;
                    }
                    sending_msgs_.pop_front();
                    if (!sending_msgs_.empty()) {
                        DoSendNext();
                    }
                });
    }

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

} // namespace fe

#endif
