cmake_minimum_required(VERSION 3.5)

project(timelord)

option(BUILD_TEST "Build and run tests" ON)

find_package(Threads REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(Boost 1.82.0 REQUIRED COMPONENTS system url)
find_package(OpenSSL REQUIRED)

set(ENABLE_TEST OFF CACHE INTERNAL "Turn off gears test")

include(FetchContent)
FetchContent_Declare(
    bhd_vdf
    GIT_REPOSITORY https://github.com/bhdone/bhd_vdf
    GIT_TAG ba1e28049bb34ce611b4c8190eac65c3ae35fcdc
)

FetchContent_Declare(
    gears
    GIT_REPOSITORY https://github.com/bhdone/gears
    GIT_TAG 1979324ec2bff6677a4f704d24cfb0616c59d240
)

FetchContent_MakeAvailable(bhd_vdf gears)

set(CMAKE_CXX_STANDARD 17)

set(TIMELORDLIB_SRCS
    ./src/timelord_utils.cpp
    ./src/timelord.cpp
    ./src/timelord_client.cpp
    ./src/frontend.cpp
    ./src/frontend_client.cpp
    ./src/challenge_monitor.cpp
    ./src/vdf_client_man.cpp
    ./src/vdf_record.cpp
    ./src/standard_status_querier.cpp
    ./src/block_querier_utils.cpp
    ./src/https_querier.cpp
)

set(SQLITE3_WRAP_SRCS
    ./src/sqlite_utils.cpp
    ./src/sqlite_wrap.cpp
    ./src/sqlite_stmt_wrap.cpp
    ./src/local_sqlite_storage.cpp
)

set(WEB_SERVICE_SRCS
    ./src/web_req_handler.cpp
    ./src/vdf_web_service.cpp
)

set(TIMELORD_SRCS
    ./src/main.cpp
    ${TIMELORDLIB_SRCS}
)

add_executable(timelord ${TIMELORD_SRCS} ${SQLITE3_WRAP_SRCS} ${WEB_SERVICE_SRCS})
add_dependencies(timelord gears bhd_vdf)
target_include_directories(timelord PRIVATE ${bhd_vdf_SOURCE_DIR}/src ${gears_SOURCE_DIR}/src ${UniValue_Include} ${tinyformat_Include})
target_link_libraries(timelord PRIVATE
    bhd_vdf
    gears
    cxxopts::cxxopts
    plog::plog
    JsonCpp::JsonCpp
    fmt::fmt
    CURL::libcurl
    unofficial::sqlite3::sqlite3
    Boost::url
    OpenSSL::SSL
    OpenSSL::Crypto
    ${UniValue_Lib}
)

set(TIMELORD_CLIENT_SRCS
    ./src/timelord_client.cpp
    ./src/timelord_utils.cpp
    ./src/frontend_client.cpp
)

add_library(timelord_client ${TIMELORD_CLIENT_SRCS})
add_dependencies(timelord_client gears bhd_vdf)
target_include_directories(timelord_client PRIVATE ${bhd_vdf_SOURCE_DIR}/src ${gears_SOURCE_DIR}/src ${UniValue_Include} ${tinyformat_Include})
target_link_libraries(timelord_client PRIVATE JsonCpp::JsonCpp)

if (BUILD_TEST)
    function(MakeTest TEST_TARGET_NAME)
        set(TEST_SRCS
            ${TIMELORDLIB_SRCS}
            ${SQLITE3_WRAP_SRCS}
            ${WEB_SERVICE_SRCS}
            ./src/test_utils.cpp
            ./src/test_main.cpp
            ./src/${TEST_TARGET_NAME}.cpp
        )
        add_executable(${TEST_TARGET_NAME} ${TEST_SRCS})
        add_dependencies(${TEST_TARGET_NAME} gears bhd_vdf)
        target_include_directories(${TEST_TARGET_NAME} PRIVATE ${bhd_vdf_SOURCE_DIR}/src ${gears_SOURCE_DIR}/src ${UniValue_Include} ${tinyformat_Include})
        target_link_libraries(${TEST_TARGET_NAME} PRIVATE
            bhd_vdf
            gears
            GTest::gtest
            plog::plog
            JsonCpp::JsonCpp
            fmt::fmt
            CURL::libcurl
            unofficial::sqlite3::sqlite3
            Boost::url
            OpenSSL::SSL
            OpenSSL::Crypto
            ${UniValue_Lib}
        )
        add_test(NAME ${TEST_TARGET_NAME} COMMAND ./${TEST_TARGET_NAME})
    endfunction()

    enable_testing()
    MakeTest(test_frontend)
    MakeTest(test_timelord)
    MakeTest(test_vdf_client)
    MakeTest(test_challenge_monitor)
    MakeTest(test_sqlite)
    MakeTest(test_vdf_persist)
    MakeTest(test_web_service)
    MakeTest(test_block_querier)
    MakeTest(test_ip_addr_querier)
endif()
