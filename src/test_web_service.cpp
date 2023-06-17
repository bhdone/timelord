#include <gtest/gtest.h>

#include <thread>

#include <filesystem>
namespace fs = std::filesystem;

#include <json/json.h>
#include <json/reader.h>

#include "local_sqlite_storage.h"
#include "netspace_data.h"
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
        persist_ = std::make_unique<LocalSQLiteDatabaseKeeper>(*storage_);
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
    std::unique_ptr<LocalSQLiteDatabaseKeeper> persist_;
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
    persist_->Save(pack);

    asio::io_context ioc;
    VDFWebService service(
            ioc, SZ_LISTEN_ADDR, LISTEN_PORT, 30, "", 0,
            [](int) {
                return 0;
            },
            [](int) -> std::vector<BlockInfo> {
                return {};
            },
            [](int) -> std::vector<NetspaceData> {
                return {};
            },
            []() -> TimelordStatus {
                return {};
            },
            [](int) -> std::tuple<std::vector<RankRecord>, int> {
                return {};
            },
            []() -> Supply {
                return {};
            },
            []() -> PledgeInfo {
                return {};
            },
            []() -> uint64_t {
                return 0;
            });
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

    service.Stop();
    service_thread.join();
}
