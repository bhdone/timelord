cmake_minimum_required(VERSION 3.2)

project(timelord)

find_package(cxxopts CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
    bhd_vdf
    GIT_REPOSITORY  https://github.com/bhdone/bhd_vdf
    GIT_TAG         v0.0.51
    )

FetchContent_MakeAvailable(bhd_vdf)

set(BHD_VDF_SRCS
    ./build/_deps/bhd_vdf-src/src/vdf_test.cpp
    ./build/_deps/bhd_vdf-src/src/vdf_utils.cpp
    ./build/_deps/bhd_vdf-src/src/vdf_computer.cpp
    )

set(CMAKE_CXX_STANDARD 17)

set(TIMELORD_SRCS
    ./src/main.cpp
    ./src/vdf_client_man.cpp
    )

add_executable(timelord ${TIMELORD_SRCS})
target_include_directories(timelord PRIVATE ${bhd_vdf_SOURCE_DIR}/src)
target_link_libraries(timelord PRIVATE cxxopts::cxxopts plog::plog asio::asio JsonCpp::JsonCpp)
