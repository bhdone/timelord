#include "front_end.h"

Json::Value ParseStringToJson(std::string_view str)
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

std::string AddressToString(void const* p)
{
    auto addr = reinterpret_cast<uint64_t>(p);
    std::stringstream ss;
    ss << "0x" << std::hex << addr;
    return ss.str();
}

void MessageDispatcher::RegisterHandler(int id, Handler handler)
{
    handlers_[id] = handler;
}

void MessageDispatcher::DispatchMessage(SessionPtr psession, Json::Value const& msg)
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

Session::Session(tcp::socket&& s)
    : s_(std::move(s))
{
    PLOGD << "Session " << AddressToString(this) << " is created";
}

Session::~Session()
{
    PLOGD << "Session " << AddressToString(this) << " is going to be released";
}

void Session::SetMessageHandler(MessageHandler receiver)
{
    msg_handler_ = std::move(receiver);
}

void Session::SetErrorHandler(ErrorHandler err_handler)
{
    err_handler_ = std::move(err_handler);
}

void Session::Start()
{
    DoReadNext();
}

void Session::SendMessage(Json::Value const& value)
{
    bool do_send = sending_msgs_.empty();
    sending_msgs_.push_back(value.toStyledString());
    if (do_send) {
        DoSendNext();
    }
}

void Session::Stop()
{
    std::error_code ignored_ec;
    s_.shutdown(tcp::socket::shutdown_both, ignored_ec);
    s_.close(ignored_ec);
    PLOGD << "Session " << AddressToString(this) << " is closed";
}

void Session::DoSendNext()
{
    assert(!sending_msgs_.empty());
    send_buf_ = sending_msgs_.front();
    send_buf_.resize(send_buf_.size() + 1);
    send_buf_[send_buf_.size()] = '\0';
    asio::async_write(s_, asio::buffer(send_buf_), [self = shared_from_this()](std::error_code const& ec, std::size_t bytes_wrote) {
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

void Session::DoReadNext()
{
    asio::async_read_until(s_, read_buf_, '\0', [self = shared_from_this()](std::error_code const& ec, std::size_t bytes_read) {
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
            // Check if it is ping
            auto msg_id = msg["id"].asInt();
            if (msg_id == static_cast<int>(BhdMsgs::MSGID_BHD_PING)) {
                // Just simply send it back
                msg["id"] = static_cast<Json::Int>(FeMsgs::MSGID_FE_PONG);
                self->SendMessage(msg);
            } else {
                self->msg_handler_(msg);
            }
        } catch (std::exception const& e) {
            PLOGE << "READ: failed to parse string into json: " << e.what();
            self->err_handler_(ErrorType::READ, ec.message());
        }
        // read next
        self->DoReadNext();
    });
}

FrontEnd::FrontEnd(asio::io_context& ioc)
    : acceptor_(ioc)
{
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
    for (auto psession : session_vec_) {
        psession->Stop();
    }
    session_vec_.clear();
    std::error_code ignored_ec;
    acceptor_.cancel(ignored_ec);
    acceptor_.close(ignored_ec);
}

void FrontEnd::SetConnectionHandler(SessionConnectionHandler conn_handler)
{
    conn_handler_ = std::move(conn_handler);
}

void FrontEnd::SetMessageHandler(SessionMessageHandler receiver)
{
    msg_handler_ = std::move(receiver);
}

void FrontEnd::SetErrorHandler(SessionErrorHandler err_handler)
{
    err_handler_ = err_handler;
}

std::size_t FrontEnd::GetNumOfSessions() const
{
    return session_vec_.size();
}

void FrontEnd::DoAcceptNext()
{
    acceptor_.async_accept([this](std::error_code const& ec, tcp::socket&& s) {
        PLOGD << "Accepting new session...";
        if (ec) {
            if (err_handler_) {
                err_handler_({}, ErrorType::CONNECT, ec.message());
            }
            PLOGE << "CONNECT: " << ec.message();
            return;
        }
        auto psession = std::make_shared<Session>(std::move(s));
        psession->SetErrorHandler([this, pweak_session = std::weak_ptr(psession)](ErrorType type, std::string_view errs) {
            if (type == ErrorType::SHUTDOWN) {
                // shutdown the service
                Exit();
            } else if (err_handler_) {
                auto psession = pweak_session.lock();
                if (psession) {
                    err_handler_(psession, type, errs);
                }
            }
        });
        psession->SetMessageHandler([this, pweak_session = std::weak_ptr(psession)](Json::Value const& msg) {
            auto psession = pweak_session.lock();
            if (psession) {
                msg_handler_(psession, msg);
            }
        });
        psession->Start();
        session_vec_.push_back(psession);
        conn_handler_(psession);
    });
}

Client::Client(asio::io_context& ioc)
    : ioc_(ioc)
{
}

void Client::SetConnectionHandler(ConnectionHandler conn_handler)
{
    conn_handler_ = std::move(conn_handler);
}

void Client::SetMessageHandler(MessageHandler msg_handler)
{
    msg_handler_ = std::move(msg_handler);
}

void Client::SetErrorHandler(ErrorHandler err_handler)
{
    err_handler_ = std::move(err_handler);
}

void Client::SetCloseHandler(CloseHandler close_handler)
{
    close_handler_ = std::move(close_handler);
}

void Client::Connect(std::string_view host, unsigned short port)
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
        ps_->async_connect(*it_result, [this, host = std::string(host), port](std::error_code const& ec) {
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

void Client::SendMessage(Json::Value const& msg)
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

void Client::SendShutdown()
{
    bool do_send = sending_msgs_.empty();
    sending_msgs_.push_back("shutdown");
    if (do_send) {
        DoSendNext();
    }
}

void Client::Exit()
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

void Client::DoReadNext()
{
    asio::async_read_until(*ps_, read_buf_, '\0', [this](std::error_code const& ec, std::size_t bytes) {
        if (ec) {
            if (ec != asio::error::eof) {
                PLOGE << ec.message();
                err_handler_(ErrorType::READ, ec.message());
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
            err_handler_(ErrorType::READ, ec.message());
        }
        DoReadNext();
    });
}

void Client::DoSendNext()
{
    assert(!sending_msgs_.empty());
    auto const& msg = sending_msgs_.front();
    send_buf_.resize(msg.size() + 1);
    memcpy(send_buf_.data(), msg.data(), msg.size());
    send_buf_[msg.size()] = '\0';
    asio::async_write(*ps_, asio::buffer(send_buf_), [this](std::error_code const& ec, std::size_t bytes) {
        if (ec) {
            err_handler_(ErrorType::WRITE, ec.message());
            Exit();
            return;
        }
        sending_msgs_.pop_front();
        if (!sending_msgs_.empty()) {
            DoSendNext();
        }
    });
}
