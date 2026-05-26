# syntax=docker/dockerfile:1.7

FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        ca-certificates \
        libboost-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY . .

RUN cmake -S . -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DKVDB_BUILD_TESTS=ON

RUN cmake --build /build --parallel

RUN mkdir -p /opt/kvdb \
    && cp -a /build/dist/. /opt/kvdb/ \
    && printf '%s\n' \
        'modules/engine_standard.so' \
        'modules/query_parser_standard.so' \
        'modules/access_interface_standard.so' \
        'modules/response_constructor_standard.so' \
        > /opt/kvdb/instance_settings.txt


FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libstdc++6 \
        libgcc-s1 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/kvdb

COPY --from=build /opt/kvdb /opt/kvdb
COPY docker-entrypoint.sh /usr/local/bin/kvdb-entrypoint

RUN mkdir -p /build \
    && ln -s /opt/kvdb /build/dist \
    && chmod +x /usr/local/bin/kvdb-entrypoint

EXPOSE 9002

ENTRYPOINT ["kvdb-entrypoint"]
CMD ["manager"]