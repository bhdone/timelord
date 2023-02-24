#include <gtest/gtest.h>

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <functional>

using std::placeholders::_1;
using std::placeholders::_2;

#include <plog/Log.h>

#include "front_end.hpp"

char const* SZ_LOCAL_ADDR = "127.0.0.1";
unsigned short LOCAL_PORT = 18181;

class ClientThreadWrap
{
public:
    ClientThreadWrap() : client_(ioc_), thread_(std::bind(&ClientThreadWrap::ThreadProc, this)) {}

    void Start(fe::Client::ConnectionHandler conn_handler, fe::Client::MessageHandler msg_handler, fe::Client::ErrorHandler err_handler, fe::Client::CloseHandler close_handler)
    {
        client_.Connect(SZ_LOCAL_ADDR, LOCAL_PORT, conn_handler, msg_handler, err_handler, close_handler);
    }

    void SendMsg(Json::Value const& msg)
    {
        asio::post(ioc_,
                [this, msg]() {
                    client_.SendMessage(msg);
                });
    }

    fe::Client& GetClient() { return client_; }

private:
    void ThreadProc()
    {
        ioc_.run();
    }

    asio::io_context ioc_;
    std::thread thread_;
    fe::Client client_;
};

class BaseServer : public testing::Test
{
public:
    BaseServer() : fe_(ioc_)
    {
    }

    void Run()
    {
        ioc_.run();
    }

    void StartClient()
    {
        asio::io_context ioc_client;
        fe::Client client(ioc_client);
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
};

TEST_F(BaseServer, RunWithEmptySession)
{
    Run();
    EXPECT_TRUE(true);
    EXPECT_EQ(GetNumOfSessions(), 0);
}

int main(int argc, char* argv[])
{
    plog::ConsoleAppender<plog::TxtFormatter> console_appender;
    plog::init(plog::Severity::debug, &console_appender);
    PLOGD << "hello";

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
