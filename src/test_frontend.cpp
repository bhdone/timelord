#include <gtest/gtest.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include <json/value.h>

#include <plog/Log.h>

using std::placeholders::_1;
using std::placeholders::_2;

#include "frontend.h"
#include "frontend_client.h"

#include "msg_ids.h"

#include "test_utils.h"
#include "timelord_utils.h"

char const* SZ_LOCAL_ADDR = "127.0.0.1";
unsigned short LOCAL_PORT = 18181;

class ClientThreadWrap
{
public:
    ClientThreadWrap()
        : client_(ioc_)
    {
    }

    void Start()
    {
        client_.SetConnectionHandler(std::bind(&ClientThreadWrap::HandleConnect, this));
        client_.SetMessageHandler(std::bind(&ClientThreadWrap::HandleMessage, this, _1));
        client_.SetCloseHandler(std::bind(&ClientThreadWrap::HandleClose, this));
        client_.SetErrorHandler(std::bind(&ClientThreadWrap::HandleError, this, _1, _2));
        client_.Connect(SZ_LOCAL_ADDR, LOCAL_PORT);
        pthread_ = std::make_unique<std::thread>(&ClientThreadWrap::ThreadProc, this);
    }

    void Join()
    {
        pthread_->join();
    }

    void SendMsg(Json::Value const& msg)
    {
        client_.SendMessage(msg);
    }

    FrontEndClient& GetClient()
    {
        return client_;
    }

    void SetConnHandler(FrontEndClient::ConnectionHandler conn_handler)
    {
        conn_handler_ = std::move(conn_handler);
    }

    void SetMsgHandler(FrontEndClient::MessageHandler msg_handler)
    {
        msg_handler_ = std::move(msg_handler);
    }

    void SetErrorHandler(FrontEndClient::ErrorHandler err_handler)
    {
        err_handler_ = std::move(err_handler);
    }

    void SetCloseHandler(FrontEndClient::CloseHandler close_handler)
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

    void HandleError(FrontEndSessionErrorType type, std::string_view errs)
    {
        if (err_handler_) {
            err_handler_(type, errs);
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
    FrontEndClient client_;

    FrontEndClient::ConnectionHandler conn_handler_;
    FrontEndClient::MessageHandler msg_handler_;
    FrontEndClient::ErrorHandler err_handler_;
    FrontEndClient::CloseHandler close_handler_;
};

class BaseServer : public testing::Test
{
public:
    BaseServer()
        : frontend_(ioc_)
    {
    }

protected:
    void SetUp() override
    {
        PLOGD << "Initializing";
        frontend_.SetConnectionHandler(std::bind(&BaseServer::HandleSessionConnected, this, _1));
        frontend_.SetMessageHandler(std::bind(&BaseServer::HandleSessionMsg, this, _1, _2));
        frontend_.Run(SZ_LOCAL_ADDR, LOCAL_PORT);
    }

    void TearDown() override
    {
        PLOGD << "Exit.";
    }

    std::size_t GetNumOfSessions() const
    {
        return frontend_.GetNumOfSessions();
    }

    void Run()
    {
        pthread_ = std::make_unique<std::thread>([this]() {
            ioc_.run();
        });
    }

    void Join()
    {
        if (pthread_) {
            pthread_->join();
        }
    }

private:
    void HandleSessionConnected(FrontEndSessionPtr psession)
    {
        PLOGD << "session " << AddressToString(psession.get()) << " is connected";
    }

    void HandleSessionMsg(FrontEndSessionPtr psession, Json::Value const& msg)
    {
        PLOGD << "session " << AddressToString(psession.get()) << " received a message: " << msg["id"].asString();
    }

    asio::io_context ioc_;
    FrontEnd frontend_;
    std::unique_ptr<std::thread> pthread_;
};

TEST_F(BaseServer, RunWith1Session_msg)
{
    Run();

    std::mutex m;
    std::condition_variable cv;
    bool connected { false };

    ClientThreadWrap client_thread;
    client_thread.SetConnHandler([&client_thread]() {
        Json::Value msg;
        msg["id"] = static_cast<Json::Int>(TimelordClientMsgs::PING);
        msg["memo"] = "hello world!";
        PLOGD << "Ping";
        client_thread.GetClient().SendMessage(msg);
    });
    client_thread.SetMsgHandler([&m, &connected, &cv](Json::Value const& msg) {
        auto id = msg["id"].asInt();
        if (id == static_cast<Json::Int>(TimelordMsgs::PONG)) {
            auto memo = msg["memo"].asString();
            PLOGD << "Pong: " << memo;
            {
                std::lock_guard<std::mutex> lg(m);
                connected = true;
            }
            cv.notify_one();
        }
    });
    client_thread.Start();

    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [&connected]() {
        return connected;
    });

    // sending exit
    client_thread.GetClient().SendShutdown();

    EXPECT_TRUE(true);

    PLOGD << "exiting...";
    client_thread.Join();
    Join();
}
