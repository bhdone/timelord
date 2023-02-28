#ifndef TL_FRONT_END_HPP
#define TL_FRONT_END_HPP

#include <string>
#include <string_view>

#include <deque>
#include <functional>
#include <istream>
#include <memory>
#include <sstream>

#include <asio.hpp>
using asio::ip::tcp;

#include <json/reader.h>
#include <json/value.h>

#include <fmt/core.h>
#include <plog/Log.h>

#include "msg_ids.h"

class Session;
using SessionPtr = std::shared_ptr<Session>;
enum class ErrorType { CONNECT, READ, WRITE, CLOSE, PARSE, SHUTDOWN };

using ErrorHandler = std::function<void(ErrorType type, std::string_view errs)>;
using MessageHandler = std::function<void(Json::Value const& msg)>;

using SessionConnectionHandler = std::function<void(SessionPtr psession)>;
using SessionErrorHandler = std::function<void(SessionPtr psession, ErrorType type, std::string_view errs)>;
using SessionMessageHandler = std::function<void(SessionPtr psession, Json::Value const& msg)>;

Json::Value ParseStringToJson(std::string_view str);

class MessageDispatcher
{
public:
    using Handler = std::function<void(SessionPtr psession, Json::Value const& msg)>;

    void RegisterHandler(int id, Handler handler);

    void DispatchMessage(SessionPtr psession, Json::Value const& msg);

private:
    std::map<int, Handler> handlers_;
};

class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(tcp::socket&& s);

    ~Session();

    void SetMessageHandler(MessageHandler receiver);

    void SetErrorHandler(ErrorHandler err_handler);

    void Start();

    void SendMessage(Json::Value const& value);

    void Stop();

private:
    void DoSendNext();

    void DoReadNext();

    tcp::socket s_;
    asio::streambuf read_buf_;
    std::string send_buf_;
    MessageHandler msg_handler_;
    std::deque<std::string> sending_msgs_;
    ErrorHandler err_handler_;
};

class FrontEnd
{
public:
    explicit FrontEnd(asio::io_context& ioc);

    void Run(std::string_view addr, unsigned short port);

    void Exit();

    void SetConnectionHandler(SessionConnectionHandler conn_handler);

    void SetMessageHandler(SessionMessageHandler receiver);

    void SetErrorHandler(SessionErrorHandler err_handler);

    std::size_t GetNumOfSessions() const;

private:
    void DoAcceptNext();

    tcp::acceptor acceptor_;
    std::vector<SessionPtr> session_vec_;
    SessionConnectionHandler conn_handler_;
    SessionMessageHandler msg_handler_;
    SessionErrorHandler err_handler_;
};

class Client
{
public:
    using ConnectionHandler = std::function<void()>;
    using MessageHandler = std::function<void(Json::Value const&)>;
    using ErrorHandler = std::function<void(ErrorType err_type, std::string_view errs)>;
    using CloseHandler = std::function<void()>;

    explicit Client(asio::io_context& ioc);

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
