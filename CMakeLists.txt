cmake_minimum_required(VERSION 3.2)

project(timelord)

option(BUILD_TEST "Build and run tests" ON)

find_package(cxxopts CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
    bhd_vdf
    GIT_REPOSITORY https://github.com/bhdone/bhd_vdf
    GIT_TAG v0.0.51
    )

FetchContent_MakeAvailable(bhd_vdf)

set(CMAKE_CXX_STANDARD 17)

set(TIMELORD_SRCS
    ./src/main.cpp
    ./src/utils.cpp
    ./src/vdf_client_man.cpp
    )

add_executable(timelord ${TIMELORD_SRCS})
target_include_directories(timelord PRIVATE ${bhd_vdf_SOURCE_DIR}/src)
target_link_libraries(
    timelord PRIVATE
    bhd_vdf
    cxxopts::cxxopts
    plog::plog
    asio::asio
    JsonCpp::JsonCpp
    fmt::fmt
    )

if (BUILD_TEST)
    set(TEST_SRCS
        ./src/test.cpp
        )
    add_executable(timelord_test ${TEST_SRCS})
    target_include_directories(timelord_test PRIVATE ${bhd_vdf_SOURCE_DIR}/src)
    target_link_libraries(timelord_test PRIVATE GTest::gtest GTest::gtest_main)

    enable_testing()
    add_test(NAME timelord_test COMMAND ./timelord_test)
endif()
