cmake_minimum_required(VERSION 3.2)

project(timelord)

find_package(cxxopts CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)

set(TIMELORD_SRCS
    ./src/main.cpp
    ./src/vdf_client_man.cpp
    )

add_executable(timelord ${TIMELORD_SRCS})
target_link_libraries(timelord PRIVATE cxxopts::cxxopts plog::plog asio::asio JsonCpp::JsonCpp)
