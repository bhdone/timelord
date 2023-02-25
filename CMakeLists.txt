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

set(TIMELORD_SRCS
    ./src/main.cpp
    ./src/utils.cpp
    ./src/vdf_client_man.cpp
    )

add_executable(timelord ${TIMELORD_SRCS})
target_include_directories(timelord PRIVATE ${bhd_vdf_SOURCE_DIR}/src ${gears_SOURCE_DIR}/src ./src)
target_link_libraries(
    timelord PRIVATE
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
    find_package(gflags CONFIG REQUIRED)
    set(TEST_SRCS
        ./src/test.cpp
        )
    add_executable(timelord_test ${TEST_SRCS})
    target_include_directories(timelord_test PRIVATE ${bhd_vdf_SOURCE_DIR}/src ./src)
    target_link_libraries(timelord_test PRIVATE GTest::gtest asio::asio JsonCpp::JsonCpp fmt::fmt)

    enable_testing()
    add_test(NAME timelord_test COMMAND ./timelord_test)
endif()
