# Hanuman (equinox) Framework - Multi-stage Docker build
#
# Stage 1 (otel-builder): builds the OpenTelemetry C++ SDK from source as
#   static libraries. This is cached separately since it rarely changes and
#   takes the longest to build.
# Stage 2 (builder):       installs all other build dependencies (Kafka,
#   MongoDB, OpenSSL, zlib) and compiles the framework + all example/demo
#   binaries with `make debug`.
# Stage 3 (sdk):           a reusable "base image" for other projects: full
#   toolchain + all -dev headers + libequinox.a + the OpenTelemetry static
#   libs, ready to compile application code against. Build with
#   `docker build --target sdk -t hanuman-framework:sdk .` and `FROM` it in
#   your own Dockerfile (see "Using the SDK image" below).
# Stage 4 (runtime):       minimal image containing only the runtime shared
#   libraries and the compiled binaries - no compilers or -dev headers. This
#   is the default target (`docker build -t hanuman-framework .`).
#
# Build:
#   docker build -t hanuman-framework .
#
# Run (HTTP server demo by default):
#   docker run --rm -p 8080:8080 hanuman-framework
#
# Run a different demo:
#   docker run --rm -p 3000:3000 hanuman-framework ./build/socketio_chat_server
#
# Using the SDK image (build your own app against this framework):
#   docker build --target sdk -t hanuman-framework:sdk .
#
#   # your-app/Dockerfile
#   FROM hanuman-framework:sdk
#   COPY myapp.c .
#   RUN gcc -std=c99 myapp.c -lequinox $EQUINOX_LDFLAGS -o myapp
#   CMD ["./myapp"]
#
#   # If your app also uses OpenTelemetry (otel.h), compile+link with g++:
#   RUN g++ -std=c++17 -DOPENTELEMETRY_STL_VERSION=2017 myapp.c \
#         -lequinox $EQUINOX_LDFLAGS $EQUINOX_OTEL_LDFLAGS -o myapp

# syntax=docker/dockerfile:1

########################################################################
# Stage 1: Build the OpenTelemetry C++ SDK (static libs) from source.
#
# IMPORTANT: built with -DWITH_STL=CXX17, which makes opentelemetry's
# nostd::shared_ptr<T> alias std::shared_ptr<T>. Any code that links against
# these libs (src/otel/otel.cpp) MUST be compiled with
# -DOPENTELEMETRY_STL_VERSION=2017 to match this ABI (already wired into the
# project's Makefile via OTEL_CXXFLAGS) or you get a silent ABI mismatch that
# segfaults deep inside nostd::shared_ptr.
########################################################################
FROM ubuntu:24.04 AS otel-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        libprotobuf-dev \
        protobuf-compiler \
        libcurl4-openssl-dev \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/build-deps

RUN git clone --depth 1 --branch v1.16.1 \
        https://github.com/open-telemetry/opentelemetry-cpp.git && \
    cd opentelemetry-cpp && \
    git submodule update --init --depth 1 third_party/opentelemetry-proto

WORKDIR /opt/build-deps/opentelemetry-cpp/build

RUN cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DWITH_STL=CXX17 \
        -DWITH_ABSEIL=OFF \
        -DWITH_OTLP_GRPC=OFF \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_ZIPKIN=OFF \
        -DWITH_PROMETHEUS=OFF \
        -DWITH_EXAMPLES=OFF \
        -DBUILD_TESTING=OFF \
        -DWITH_DEPRECATED_SDK_FACTORY=OFF \
    && cmake --build . -j"$(nproc)" \
    && cmake --build . --target install \
    && ldconfig

########################################################################
# Stage 2: Build the framework itself (pure C99, except src/otel/otel.cpp).
########################################################################
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# librdkafka on Ubuntu 24.04's "universe" repo is quite old; use Confluent's
# official APT repo (same source used to build this project originally) to
# get a current librdkafka-dev.
RUN apt-get update && apt-get install -y --no-install-recommends \
        curl ca-certificates gnupg \
    && mkdir -p /etc/apt/keyrings \
    && curl -fsSL https://packages.confluent.io/clients/deb/archive.key \
        | gpg --dearmor -o /etc/apt/keyrings/confluent.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/confluent.gpg] https://packages.confluent.io/clients/deb noble main" \
        > /etc/apt/sources.list.d/confluent-clients.list \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config \
        libssl-dev \
        zlib1g-dev \
        librdkafka-dev \
        libmongoc-dev \
        libprotobuf-dev \
        libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Pre-built OpenTelemetry C++ SDK (static libs + headers) from stage 1.
COPY --from=otel-builder /usr/local/include/opentelemetry /usr/local/include/opentelemetry
COPY --from=otel-builder /usr/local/lib/ /usr/local/lib/
RUN ldconfig

WORKDIR /app
COPY . .

# NOTE: `make release` (-O2) currently fails on this codebase due to GCC's
# stricter -Wformat-truncation/-Wstringop-truncation analysis at -O2 being
# escalated to hard errors by -Werror (pre-existing issue, unrelated to
# Docker - reproduces identically with a native build). Use the debug
# profile, which builds cleanly and produces fully working binaries.
RUN make clean 2>/dev/null; make debug -j"$(nproc)"

########################################################################
# Stage 3: SDK / dev base image - for OTHER projects to build FROM.
#
# Contains the full compiler toolchain, all -dev headers, the compiled
# libequinox.a, and the OpenTelemetry static libs. Framework headers are
# installed flat into /usr/local/include so plain `#include "http_server.h"`
# (the convention used throughout this codebase) works with no extra -I
# flags. EQUINOX_LDFLAGS/EQUINOX_OTEL_LDFLAGS are exported so consumers don't
# have to reconstruct the linker invocation by hand.
########################################################################
FROM builder AS sdk

LABEL org.opencontainers.image.description="Hanuman framework SDK - compiler toolchain + libequinox.a for building applications against this framework"

RUN mkdir -p /usr/local/include && \
    find src/include -mindepth 2 -name '*.h' -exec cp {} /usr/local/include/ \; && \
    cp lib/libequinox.a /usr/local/lib/ && \
    ldconfig

ENV EQUINOX_LDFLAGS="-lrdkafka -lpthread -lssl -lcrypto -lz -lmongoc-1.0 -lbson-1.0 -lrt"
ENV EQUINOX_OTEL_LDFLAGS="-Wl,--start-group /usr/local/lib/libopentelemetry_trace.a /usr/local/lib/libopentelemetry_metrics.a /usr/local/lib/libopentelemetry_logs.a /usr/local/lib/libopentelemetry_exporter_ostream_span.a /usr/local/lib/libopentelemetry_exporter_ostream_metrics.a /usr/local/lib/libopentelemetry_exporter_ostream_logs.a /usr/local/lib/libopentelemetry_exporter_otlp_http.a /usr/local/lib/libopentelemetry_exporter_otlp_http_metric.a /usr/local/lib/libopentelemetry_exporter_otlp_http_log.a /usr/local/lib/libopentelemetry_exporter_otlp_http_client.a /usr/local/lib/libopentelemetry_otlp_recordable.a /usr/local/lib/libopentelemetry_proto.a /usr/local/lib/libopentelemetry_http_client_curl.a /usr/local/lib/libopentelemetry_resources.a /usr/local/lib/libopentelemetry_common.a /usr/local/lib/libopentelemetry_version.a -Wl,--end-group -lprotobuf -lcurl"

WORKDIR /workspace

########################################################################
# Stage 4: Minimal runtime image - shared libs + compiled binaries only.
########################################################################
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# librdkafka's runtime .so also comes from the Confluent repo (matches the
# version librdkafka-dev built against in stage 2).
RUN apt-get update && apt-get install -y --no-install-recommends \
        curl ca-certificates gnupg \
    && mkdir -p /etc/apt/keyrings \
    && curl -fsSL https://packages.confluent.io/clients/deb/archive.key \
        | gpg --dearmor -o /etc/apt/keyrings/confluent.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/confluent.gpg] https://packages.confluent.io/clients/deb noble main" \
        > /etc/apt/sources.list.d/confluent-clients.list \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3t64 \
        zlib1g \
        librdkafka1 \
        libmongoc-1.0-0t64 \
        libbson-1.0-0t64 \
        libcurl4t64 \
        libprotobuf32t64 \
        libstdc++6 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get purge -y curl gnupg && apt-get autoremove -y

RUN useradd --system --create-home --shell /usr/sbin/nologin hanuman
WORKDIR /app

COPY --from=builder /app/build ./build
COPY --from=builder /app/lib ./lib
COPY --from=builder /app/public ./public

RUN chown -R hanuman:hanuman /app
USER hanuman

# HTTP demos (8080), Socket.IO chat demo (3000), WebSocket echo demo (8080)
EXPOSE 8080 3000

CMD ["./build/http_server_app"]
