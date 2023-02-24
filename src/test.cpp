#include <gtest/gtest.h>

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

using std::placeholders::_1;
using std::placeholders::_2;

#include <plog/Log.h>

#include "front_end.hpp"

char const* SZ_LOCAL_ADDR = "127.0.0.1";
unsigned short LOCAL_PORT = 18181;

class ClientThreadWrap
{
public:
    ClientThreadWrap() : client_(ioc_) {}

    void Start()
    {
        client_.Connect(SZ_LOCAL_ADDR, LOCAL_PORT,
                std::bind(&ClientThreadWrap::HandleConnect, this),
                std::bind(&ClientThreadWrap::HandleMessage, this, _1),
                std::bind(&ClientThreadWrap::HandleError, this, _1, _2),
                std::bind(&ClientThreadWrap::HandleClose, this));
        pthread_ = std::make_unique<std::thread>(&ClientThreadWrap::ThreadProc, this);
    }

    void Join()
    {
        pthread_->join();
    }

    void SendMsg(Json::Value const& msg)
    {
        asio::post(ioc_,
                [this, msg]() {
                    client_.SendMessage(msg);
                });
    }

    fe::Client& GetClient() { return client_; }

    void SetConnHandler(fe::Client::ConnectionHandler conn_handler)
    {
        conn_handler_ = std::move(conn_handler);
    }

    void SetMsgHandler(fe::Client::MessageHandler msg_handler)
    {
        msg_handler_ = std::move(msg_handler);
    }

    void SetErrorHandler(fe::Client::ErrorHandler err_handler)
    {
        err_handler_ = std::move(err_handler);
    }

    void SetCloseHandler(fe::Client::CloseHandler close_handler)
    {
        close_handler_ = std::move(close_handler);
    }

private:
    void ThreadProc()
    {
        PLOGD << "io is running...";
        ioc_.run();
        PLOGD << "io exited";
    }

    void HandleConnect()
    {
        if (conn_handler_) {
            conn_handler_();
        }
    }

    void HandleMessage(Json::Value const& msg)
    {
        if (msg_handler_) {
            msg_handler_(msg);
        }
    }

    void HandleError(fe::ErrorType type, std::error_code const& ec)
    {
        if (err_handler_) {
            err_handler_(type, ec);
        }
    }

    void HandleClose()
    {
        if (close_handler_) {
            close_handler_();
        }
    }

    asio::io_context ioc_;
    std::unique_ptr<std::thread> pthread_;
    fe::Client client_;

    fe::Client::ConnectionHandler conn_handler_;
    fe::Client::MessageHandler msg_handler_;
    fe::Client::ErrorHandler err_handler_;
    fe::Client::CloseHandler close_handler_;
};

class BaseServer : public testing::Test
{
public:
    BaseServer() : fe_(ioc_)
    {
    }

protected:
    void SetUp() override
    {
        PLOGI << "Initializing";
        fe_.Run(SZ_LOCAL_ADDR, LOCAL_PORT,
                std::bind(&BaseServer::HandleSessionConnected, this, _1), std::bind(&BaseServer::HandleSessionMsg, this, _1, _2));
    }

    void TearDown() override
    {
        PLOGI << "Exit.";
    }

    std::size_t GetNumOfSessions() const
    {
        return fe_.GetNumOfSessions();
    }

    void Run()
    {
        pthread_ = std::make_unique<std::thread>([this]() { ioc_.run(); });
    }

    void Join()
    {
        if (pthread_) {
            pthread_->join();
        }
    }

    Json::Value MakeExitMsg()
    {
        Json::Value msg;
        msg["id"] = "exit";
        return msg;
    }

private:
    void HandleSessionConnected(fe::SessionPtr psession)
    {
        PLOGI << "session " << AddressToString(psession.get()) << " is connected";
    }

    void HandleSessionMsg(fe::SessionPtr psession, Json::Value const& msg)
    {
        PLOGI << "session " << fe::AddressToString(psession.get()) << " received a message: " << msg["id"].asString();
    }

    asio::io_context ioc_;
    fe::FrontEnd fe_;
    std::unique_ptr<std::thread> pthread_;
};

TEST_F(BaseServer, RunWith1Session_connection)
{
    Run();

    std::mutex m;
    std::condition_variable cv;
    bool connected{false};

    ClientThreadWrap client_thread;
    client_thread.SetConnHandler(
            [&m, &connected, &cv]() {
                PLOGD << "connected, now set the condition";
                {
                    std::lock_guard<std::mutex> lg(m);
                    connected = true;
                }
                cv.notify_one();
            });
    client_thread.Start();

    PLOGD << "waiting the connection...";
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [&connected]() { return connected; });

    // sending exit
    client_thread.GetClient().SendShutdown();

    EXPECT_TRUE(true);

    PLOGD << "exiting...";
    client_thread.Join();
    Join();
}

int main(int argc, char* argv[])
{
    plog::ConsoleAppender<plog::TxtFormatter> console_appender;
    plog::init(plog::Severity::debug, &console_appender);
    PLOGD << "hello";

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
