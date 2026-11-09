FROM ubuntu:24.04 AS build
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates cmake g++ libboost-dev libicu-dev libpqxx-dev ninja-build pkg-config \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY src/ src/
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build --parallel 2

FROM ubuntu:24.04
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates libicu74 libpqxx-7.8t64 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/search_indexer /src/build/search_query /usr/local/bin/
USER 10001:10001
CMD ["search_query", "/run/search/config.ini", "--http"]
