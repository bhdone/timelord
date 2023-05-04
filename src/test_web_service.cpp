#include <gtest/gtest.h>

#include <thread>

#include <filesystem>
namespace fs = std::filesystem;

#include <json/json.h>
#include <json/reader.h>

#include "local_sqlite_storage.h"
#include "vdf_web_service.h"

#include "test_utils.h"

char const* SZ_WEBSERVICE_LOCAL_DBNAME = "webservice_db.sqlite3";

char const* SZ_HOST = "localhost";
char const* SZ_LISTEN_ADDR = "127.0.0.1";
const uint16_t LISTEN_PORT = 39393;

class WebServiceTest : public testing::Test
{
protected:
    void SetUp() override
    {
        if (fs::exists(SZ_WEBSERVICE_LOCAL_DBNAME) && fs::is_regular_file(SZ_WEBSERVICE_LOCAL_DBNAME)) {
            fs::remove(SZ_WEBSERVICE_LOCAL_DBNAME);
        }
        storage_ = std::make_unique<LocalSQLiteStorage>(SZ_WEBSERVICE_LOCAL_DBNAME);
        persist_ = std::make_unique<VDFSQLitePersistOperator>(*storage_);
    }

    void TearDown() override
    {
        storage_.reset();
        fs::remove(SZ_WEBSERVICE_LOCAL_DBNAME);
    }

    std::tuple<Json::Value, bool> RunTestClient(std::string_view path)
    {
        Json::Value json;
        try {
            // start client to query the record
            asio::io_context ioc_client;
            beast::tcp_stream stream(ioc_client);
            tcp::endpoint endpoint(asio::ip::address::from_string(SZ_LISTEN_ADDR), LISTEN_PORT);
            stream.connect(endpoint);

            http::request<http::string_body> request { http::verb::get, path, 11 };
            request.set(http::field::host, SZ_HOST);
            request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            http::write(stream, request);
            beast::flat_buffer buffer;

            http::response<http::string_body> response;
            http::read(stream, buffer, response);

            std::string body = response.body();

            Json::CharReaderBuilder builder;
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

            std::string errs;
            bool parsed = reader->parse(body.data(), body.data() + body.size(), &json, &errs);
            if (!parsed) {
                throw std::runtime_error(errs);
            }

            // close
            error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);

            if (ec && ec != beast::errc::not_connected) {
                throw beast::system_error(ec);
            }
        } catch (std::exception const& e) {
            return std::make_tuple(json, false);
        }

        return std::make_tuple(json, true);
    }

    std::unique_ptr<LocalSQLiteStorage> storage_;
    std::unique_ptr<VDFSQLitePersistOperator> persist_;
};

void check_json(Json::Value const& json, std::string const& member_name, uint64_t val)
{
    EXPECT_TRUE(json.isMember(member_name));
    EXPECT_TRUE(json[member_name].isInt());
    EXPECT_EQ(json[member_name].asInt64(), val);
}

void check_json(Json::Value const& json, std::string const& member_name, std::string_view val)
{
    EXPECT_TRUE(json.isMember(member_name));
    EXPECT_TRUE(json[member_name].isString());
    EXPECT_EQ(json[member_name].asString(), val);
}

TEST_F(WebServiceTest, FullTests)
{
    VDFRecordPack pack = GenerateRandomPack(time(nullptr), 20000, false);
    pack.requests.push_back(GenerateRandomRequest(pack.record.challenge));
    pack.results.push_back(GenerateRandomResult(pack.record.challenge, pack.requests[0].iters, 10000));
    persist_->Save(pack);

    asio::io_context ioc;
    VDFWebService service(ioc, SZ_LISTEN_ADDR, LISTEN_PORT, 30, *persist_);
    service.Run();

    std::thread service_thread([&ioc]() {
        ioc.run();
    });

    auto [json, succ] = RunTestClient("/api/vdf?hours=1");

    EXPECT_TRUE(succ);
    EXPECT_TRUE(json.isArray());
    EXPECT_EQ(json.size(), 1);

    Json::Value record_val = *json.begin();
    check_json(record_val, "timestamp", pack.record.timestamp);
    check_json(record_val, "challenge", Uint256ToHex(pack.record.challenge));
    check_json(record_val, "height", pack.record.height);

    EXPECT_TRUE(record_val.isMember("requests"));
    Json::Value request_vals = record_val["requests"];
    EXPECT_TRUE(request_vals.isArray());
    EXPECT_EQ(request_vals.size(), 1);

    Json::Value request_val = request_vals[0];
    EXPECT_TRUE(request_val.isObject());
    check_json(request_val, "challenge", Uint256ToHex(pack.requests[0].challenge));
    check_json(request_val, "iters", pack.requests[0].iters);
    check_json(request_val, "estimated_seconds", pack.requests[0].estimated_seconds);
    check_json(request_val, "group_hash", Uint256ToHex(pack.requests[0].group_hash));
    check_json(request_val, "total_size", pack.requests[0].total_size);

    EXPECT_TRUE(record_val.isMember("results"));
    Json::Value result_vals = record_val["results"];
    EXPECT_TRUE(result_vals.isArray());
    EXPECT_EQ(result_vals.size(), 1);

    Json::Value result_val = result_vals[0];
    EXPECT_TRUE(result_val.isObject());
    check_json(result_val, "challenge", Uint256ToHex(pack.results[0].challenge));
    check_json(result_val, "iters", pack.results[0].iters);
    check_json(result_val, "y", BytesToHex(pack.results[0].y));
    check_json(result_val, "proof", BytesToHex(pack.results[0].proof));
    check_json(result_val, "witness_type", pack.results[0].witness_type);
    check_json(result_val, "duration", pack.results[0].duration);

    service.Stop();
    service_thread.join();
}