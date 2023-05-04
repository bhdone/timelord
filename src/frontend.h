#ifndef TL_FRONT_END_HPP
#define TL_FRONT_END_HPP

#include <string>
#include <string_view>

#include <deque>
#include <map>

#include <functional>

#include "asio_defs.hpp"

namespace Json
{
class Value;
} // namespace Json

class FrontEndSession;
using FrontEndSessionPtr = std::shared_ptr<FrontEndSession>;
enum class FrontEndSessionErrorType { CONNECT, READ, WRITE, CLOSE };

class FrontEndSession : public std::enable_shared_from_this<FrontEndSession>
{
public:
    using ConnectionHandler = std::function<void(FrontEndSessionPtr psession)>;
    using MessageHandler = std::function<void(FrontEndSessionPtr psession, Json::Value const& msg)>;
    using ErrorHandler = std::function<void(FrontEndSessionPtr psession, FrontEndSessionErrorType type, std::string_view errs)>;

    FrontEndSession(asio::io_context& ioc, tcp::socket&& s);

    ~FrontEndSession();

    void SetMessageHandler(MessageHandler msg_handler);

    void SetErrorHandler(ErrorHandler err_handler);

    void Start();

    void SendMessage(Json::Value const& value);

    void Stop();

private:
    void DoSendNext();

    void DoReadNext();

    void ResetTimeoutTimer();

    asio::io_context& ioc_;
    tcp::socket s_;
    asio::streambuf read_buf_;
    std::string send_buf_;
    std::deque<std::string> sending_msgs_;
    std::unique_ptr<asio::steady_timer> timeout_timer_;
    MessageHandler msg_handler_;
    ErrorHandler err_handler_;
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

    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::atomic_int num_of_sessions_ { 0 };
    std::vector<FrontEndSessionPtr> session_vec_;
    FrontEndSession::ConnectionHandler conn_handler_;
    FrontEndSession::MessageHandler msg_handler_;
    FrontEndSession::ErrorHandler err_handler_;
};

#endif
