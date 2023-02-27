cmake_minimum_required(VERSION 3.2)

project(timelord)

option(BUILD_TEST "Build and run tests" ON)

find_package(cxxopts CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
    bhd_vdf
    GIT_REPOSITORY https://github.com/bhdone/bhd_vdf
    GIT_TAG v0.0.51
    )

FetchContent_Declare(
    gears
    GIT_REPOSITORY https://github.com/bhdone/gears
    GIT_TAG fb9a444fd7a6f028f51d115d8e69d324a531f27d
)

FetchContent_MakeAvailable(bhd_vdf gears)

set(CMAKE_CXX_STANDARD 17)

set(TIMELORDLIB_SRCS
    ./src/timelord_utils.cpp
    ./src/timelord.cpp
    ./src/front_end.cpp
    ./src/challenge_monitor.cpp
    ./src/vdf_client_man.cpp
    )

set(TIMELORD_SRCS
    ./src/main.cpp
    ${TIMELORDLIB_SRCS}
    )

add_executable(timelord ${TIMELORD_SRCS})
target_include_directories(timelord PRIVATE ${bhd_vdf_SOURCE_DIR}/src ${gears_SOURCE_DIR}/src)
target_link_libraries(timelord PRIVATE
    bhd_vdf
    gears
    cxxopts::cxxopts
    plog::plog
    asio::asio
    JsonCpp::JsonCpp
    fmt::fmt
    CURL::libcurl
    )

if (BUILD_TEST)
    function(MakeTest TEST_TARGET_NAME)
        set(TEST_SRCS
            ${TIMELORDLIB_SRCS}
            ./src/test_utils.cpp
            ./src/test_main.cpp
            ./src/${TEST_TARGET_NAME}.cpp
            )
        add_executable(${TEST_TARGET_NAME} ${TEST_SRCS})
        target_include_directories(${TEST_TARGET_NAME} PRIVATE ${bhd_vdf_SOURCE_DIR}/src ${gears_SOURCE_DIR}/src)
        target_link_libraries(${TEST_TARGET_NAME} PRIVATE
            bhd_vdf
            gears
            GTest::gtest
            plog::plog
            asio::asio
            JsonCpp::JsonCpp
            fmt::fmt
            CURL::libcurl
            )
        add_test(NAME ${TEST_TARGET_NAME} COMMAND ./${TEST_TARGET_NAME})
    endfunction()

    enable_testing()
    MakeTest(test_frontend)
    MakeTest(test_timelord)
    MakeTest(test_challenge_monitor)
endif()
