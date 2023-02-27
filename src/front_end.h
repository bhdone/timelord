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

Json::Value ParseStringToJson(std::string_view str);

std::string AddressToString(void const* p);

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

    void Start(MessageReceiver msg_receiver);

    void SendMessage(Json::Value const& value);

    void Close();

    void SetErrorHandler(ErrorHandler err_handler);

private:
    void DoSendNext();

    void DoReadNext();

    tcp::socket s_;
    asio::streambuf read_buf_;
    std::string send_buf_;
    MessageReceiver msg_receiver_;
    std::deque<std::string> sending_msgs_;
    ErrorHandler err_handler_;
};

class FrontEnd
{
public:
    explicit FrontEnd(asio::io_context& ioc);

    void Run(std::string_view addr, unsigned short port, SessionConnectedHandler connected_handler, SessionMessageReceiver session_msg_receiver);

    void Close();

    void SetErrorHandler(SessionErrorHandler err_handler);

    std::size_t GetNumOfSessions() const;

    void DoAcceptNext();

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

    explicit Client(asio::io_context& ioc);

    void Connect(std::string_view host, unsigned short port, ConnectionHandler conn_handler, MessageHandler msg_handler, ErrorHandler err_handler, CloseHandler close_handler);

    void SendMessage(Json::Value const& msg);

    void SendShutdown();

    void Close();

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

} // namespace fe

#endif
