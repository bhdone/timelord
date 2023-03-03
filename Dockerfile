FROM debian:stable

RUN apt update && apt upgrade --yes
RUN apt install curl gcc zip unzip tar cmake g++ pkg-config automake autoconf git libssl-dev libtool yasm texinfo --yes

RUN git clone https://github.com/microsoft/vcpkg /vcpkg && cd /vcpkg && ./bootstrap-vcpkg.sh && ./vcpkg install cxxopts plog asio jsoncpp fmt curl
RUN git clone https://github.com/Kitware/CMake /cmake && cd /cmake && ./configure && make -j3 && make install

COPY . /timelord
RUN mkdir -p /timelord/build && cd /timelord/build && cmake .. -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake && make -j3
