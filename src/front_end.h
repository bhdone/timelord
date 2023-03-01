#ifndef TL_FRONT_END_HPP
#define TL_FRONT_END_HPP

#include <string>
#include <string_view>

#include <deque>
#include <map>

#include <functional>

#include <asio.hpp>
using asio::ip::tcp;

namespace Json
{
class Value;
} // namespace Json

class FrontEndSession;
using FrontEndSessionPtr = std::shared_ptr<FrontEndSession>;
enum class FrontEndSessionErrorType { CONNECT, READ, WRITE, CLOSE, PARSE, SHUTDOWN };

using FrontEndErrorHandler = std::function<void(FrontEndSessionErrorType type, std::string_view errs)>;
using FrontEndMessageHandler = std::function<void(Json::Value const& msg)>;

Json::Value ParseStringToJson(std::string_view str);

class MessageDispatcher
{
public:
    using Handler = std::function<void(FrontEndSessionPtr psession, Json::Value const& msg)>;

    void RegisterHandler(int id, Handler handler);

    void DispatchMessage(FrontEndSessionPtr psession, Json::Value const& msg);

private:
    std::map<int, Handler> handlers_;
};

class FrontEndSession : public std::enable_shared_from_this<FrontEndSession>
{
public:
    using ConnectionHandler = std::function<void(FrontEndSessionPtr psession)>;
    using ErrorHandler = std::function<void(FrontEndSessionPtr psession, FrontEndSessionErrorType type, std::string_view errs)>;
    using MessageHandler = std::function<void(FrontEndSessionPtr psession, Json::Value const& msg)>;

    explicit FrontEndSession(tcp::socket&& s);

    ~FrontEndSession();

    void SetMessageHandler(FrontEndMessageHandler msg_handler);

    void SetErrorHandler(FrontEndErrorHandler err_handler);

    void Start();

    void SendMessage(Json::Value const& value);

    void Stop();

private:
    void DoSendNext();

    void DoReadNext();

    tcp::socket s_;
    asio::streambuf read_buf_;
    std::string send_buf_;
    FrontEndMessageHandler msg_handler_;
    std::deque<std::string> sending_msgs_;
    FrontEndErrorHandler err_handler_;
};

class FrontEnd
{
public:
    explicit FrontEnd(asio::io_context& ioc);

    void SetConnectionHandler(FrontEndSession::ConnectionHandler conn_handler);

    void SetMessageHandler(FrontEndSession::MessageHandler msg_handler);

    void SetErrorHandler(FrontEndSession::ErrorHandler err_handler);

    void Run(std::string_view addr, unsigned short port);

    void Exit();

    std::size_t GetNumOfSessions() const;

private:
    void DoAcceptNext();

    tcp::acceptor acceptor_;
    std::vector<FrontEndSessionPtr> session_vec_;
    FrontEndSession::ConnectionHandler conn_handler_;
    FrontEndSession::MessageHandler msg_handler_;
    FrontEndSession::ErrorHandler err_handler_;
};

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
