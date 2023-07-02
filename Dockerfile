FROM debian:stable AS build

RUN mkdir /app

RUN apt update && apt upgrade --yes
RUN apt install curl gcc zip unzip tar cmake g++ pkg-config automake autoconf git libssl-dev libtool yasm texinfo libboost-all-dev libgmp-dev --yes

RUN git clone https://github.com/microsoft/vcpkg /vcpkg && cd /vcpkg && ./bootstrap-vcpkg.sh && ./vcpkg install cxxopts plog jsoncpp fmt curl sqlite3 boost openssl
RUN git clone https://github.com/Kitware/CMake /cmake && cd /cmake && ./configure && make -j3 && make install

COPY . /timelord
RUN mkdir -p /timelord/build && cd /timelord/build && cmake .. -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake && make -j3 && cp /timelord/build/timelord /app

RUN git clone https://github.com/chia-network/chiavdf /chiavdf && cd /chiavdf/src && make -f Makefile.vdf-client && cp /chiavdf/src/vdf_client /app && cp /chiavdf/src/vdf_bench /app

FROM debian:stable AS main

COPY --from=build /app/* /

RUN apt update && apt install libgmp-dev --yes

EXPOSE 19191

EXPOSE 39393

CMD ["/timelord"]
