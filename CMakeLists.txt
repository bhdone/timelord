cmake_minimum_required(VERSION 3.1)

project(timelord)

find_package(cxxopts CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)

set(TIMELORD_SRCS
    src/main.cpp
    )

add_executable(timelord ${TIMELORD_SRCS})
target_compile_features(timelord PRIVATE cxx_std_14)
target_link_libraries(timelord PRIVATE cxxopts::cxxopts)
target_link_libraries(timelord PRIVATE asio::asio)
target_link_libraries(timelord PRIVATE plog::plog)
