# Hanuman Framework Makefile

CC = gcc
CXX = g++

# Directories
SRC_DIR = src
INC_DIR = src/include
BUILD_DIR = build
LIB_DIR = lib
EXAMPLE_DIR = examples

# Headers are organized by function under src/include/<module>/. Since all
# example and source files use bare includes (e.g. #include "application.h"),
# every module subfolder is added to the include path so files don't need to
# know which module owns a given header.
INCLUDE_DIRS = $(INC_DIR) $(wildcard $(INC_DIR)/*/)

CFLAGS = -Wall -Wextra -Werror -std=c99 $(addprefix -I,$(INCLUDE_DIRS)) $(shell pkg-config --cflags libmongoc-1.0)
LDFLAGS = -lrdkafka -lpthread -lssl -lcrypto -lz $(shell pkg-config --libs libmongoc-1.0)
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG

# OpenTelemetry C++ SDK (built from source, installed under /usr/local).
# NOTE: the SDK was built with -DWITH_STL=CXX17, which makes nostd::shared_ptr
# alias std::shared_ptr. Any translation unit using OTel headers/libs MUST
# define OPENTELEMETRY_STL_VERSION=2017 to match this ABI, or you will get
# hard-to-diagnose segfaults inside nostd::shared_ptr.
OTEL_INC = /usr/local/include
OTEL_CXXFLAGS = -std=c++17 -Wall -Wextra $(addprefix -I,$(INCLUDE_DIRS)) -I$(OTEL_INC) -DOPENTELEMETRY_STL_VERSION=2017
OTEL_LIB_DIR = /usr/local/lib
OTEL_LIBS = -Wl,--start-group \
            $(OTEL_LIB_DIR)/libopentelemetry_trace.a \
            $(OTEL_LIB_DIR)/libopentelemetry_metrics.a \
            $(OTEL_LIB_DIR)/libopentelemetry_logs.a \
            $(OTEL_LIB_DIR)/libopentelemetry_exporter_ostream_span.a \
            $(OTEL_LIB_DIR)/libopentelemetry_exporter_ostream_metrics.a \
            $(OTEL_LIB_DIR)/libopentelemetry_exporter_ostream_logs.a \
            $(OTEL_LIB_DIR)/libopentelemetry_exporter_otlp_http.a \
            $(OTEL_LIB_DIR)/libopentelemetry_exporter_otlp_http_metric.a \
            $(OTEL_LIB_DIR)/libopentelemetry_exporter_otlp_http_log.a \
            $(OTEL_LIB_DIR)/libopentelemetry_exporter_otlp_http_client.a \
            $(OTEL_LIB_DIR)/libopentelemetry_otlp_recordable.a \
            $(OTEL_LIB_DIR)/libopentelemetry_proto.a \
            $(OTEL_LIB_DIR)/libopentelemetry_http_client_curl.a \
            $(OTEL_LIB_DIR)/libopentelemetry_resources.a \
            $(OTEL_LIB_DIR)/libopentelemetry_common.a \
            $(OTEL_LIB_DIR)/libopentelemetry_version.a \
            -Wl,--end-group \
            -lprotobuf -lcurl

# Source files, grouped by function under src/<module>/
SOURCES = $(SRC_DIR)/core/application.c \
          $(SRC_DIR)/core/module.c \
          $(SRC_DIR)/core/service_controller.c \
          $(SRC_DIR)/core/framework.c \
          $(SRC_DIR)/http/http_server.c \
          $(SRC_DIR)/http/http_route.c \
          $(SRC_DIR)/http/http2.c \
          $(SRC_DIR)/http/http_client.c \
          $(SRC_DIR)/kafka/kafka_client.c \
          $(SRC_DIR)/json/json_parser.c \
          $(SRC_DIR)/mongo/mongo_client.c \
          $(SRC_DIR)/realtime/websocket.c \
          $(SRC_DIR)/realtime/socketio.c

# OpenTelemetry C++ wrapper (compiled separately with g++)
OTEL_SOURCE = $(SRC_DIR)/otel/otel.cpp
OTEL_OBJECT = $(BUILD_DIR)/otel/otel.o

# Object files (mirrors the src/<module>/ layout under build/<module>/)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) $(OTEL_OBJECT)
OBJECT_DIRS = $(sort $(dir $(OBJECTS)))

# Library
LIB_NAME = libequinox.a
STATIC_LIB = $(LIB_DIR)/$(LIB_NAME)

# Example applications
DEMO_APP = $(BUILD_DIR)/demo_app
HTTP_SERVER_APP = $(BUILD_DIR)/http_server_app
HTTP2_SERVER_APP = $(BUILD_DIR)/http2_server_app
PARAM_DEMO = $(BUILD_DIR)/param_demo
KAFKA_DEMO = $(BUILD_DIR)/kafka_demo
KAFKA_SSL_DEMO = $(BUILD_DIR)/kafka_ssl_demo
KAFKA_MULTI_TOPIC_DEMO = $(BUILD_DIR)/kafka_multi_topic_demo
KAFKA_AUTH_DEMO = $(BUILD_DIR)/kafka_auth_demo
UNIFIED_APP = $(BUILD_DIR)/unified_app
JSON_SCHEMA_DEMO = $(BUILD_DIR)/json_schema_demo
STATIC_SERVER_DEMO = $(BUILD_DIR)/static_server_demo
HTTP_CLIENT_DEMO = $(BUILD_DIR)/http_client_demo
HTTP_PROXY_DEMO = $(BUILD_DIR)/http_proxy_demo
MONGO_DEMO = $(BUILD_DIR)/mongo_demo
MONGO_ASYNC_DEMO = $(BUILD_DIR)/mongo_async_demo
MONGO_HANDLE_DEMO = $(BUILD_DIR)/mongo_handle_management_demo
WS_ECHO_SERVER = $(BUILD_DIR)/websocket_echo_server
SIO_CHAT_SERVER = $(BUILD_DIR)/socketio_chat_server
OTEL_DEMO = $(BUILD_DIR)/otel_demo

# Default target
.PHONY: all
all: debug

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(STATIC_LIB) $(DEBUG_TARGET) $(HTTP_SERVER_APP) $(HTTP2_SERVER_APP) $(PARAM_DEMO) $(KAFKA_DEMO) $(KAFKA_SSL_DEMO) $(KAFKA_MULTI_TOPIC_DEMO) $(KAFKA_AUTH_DEMO) $(UNIFIED_APP) $(JSON_SCHEMA_DEMO) $(STATIC_SERVER_DEMO) $(HTTP_CLIENT_DEMO) $(HTTP_PROXY_DEMO) $(MONGO_DEMO) $(MONGO_ASYNC_DEMO) $(MONGO_HANDLE_DEMO) $(WS_ECHO_SERVER) $(SIO_CHAT_SERVER) $(OTEL_DEMO)
	@echo "Debug build complete"

# Release build
#
# NOTE: as of the current codebase, `make release` fails to compile due to
# GCC's -Wformat-truncation/-Wstringop-truncation analysis (only triggered
# at -O2+) being escalated to hard errors by -Werror, in:
#   src/http/http_server.c, src/mongo/mongo_client.c,
#   src/realtime/websocket.c
# This is a pre-existing issue independent of Docker/otel/folder changes.
# Use `make debug` until the strncpy() truncation warnings are fixed at the
# source, or these specific warnings are silenced in CFLAGS.
.PHONY: release
release: CFLAGS += $(RELEASE_FLAGS)
release: $(STATIC_LIB) $(RELEASE_TARGET) $(HTTP_SERVER_APP) $(HTTP2_SERVER_APP) $(PARAM_DEMO) $(KAFKA_DEMO) $(KAFKA_SSL_DEMO) $(KAFKA_MULTI_TOPIC_DEMO) $(KAFKA_AUTH_DEMO) $(UNIFIED_APP) $(JSON_SCHEMA_DEMO) $(STATIC_SERVER_DEMO) $(HTTP_CLIENT_DEMO) $(HTTP_PROXY_DEMO) $(MONGO_DEMO) $(MONGO_ASYNC_DEMO) $(MONGO_HANDLE_DEMO) $(WS_ECHO_SERVER) $(SIO_CHAT_SERVER) $(OTEL_DEMO)
	@echo "Release build complete"

# Create directories
.PHONY: directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(LIB_DIR)
	@mkdir -p $(OBJECT_DIRS)

# Build object files (mirrors src/<module>/ under build/<module>/)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | directories
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build OpenTelemetry C++ wrapper object (separate compiler/flags)
$(OTEL_OBJECT): $(OTEL_SOURCE) | directories
	@echo "Compiling $<..."
	$(CXX) $(OTEL_CXXFLAGS) -c $< -o $@

# Build static library
$(STATIC_LIB): $(OBJECTS) | directories
	@echo "Creating static library $(STATIC_LIB)..."
	ar rcs $@ $^
	@echo "Library created successfully"

# Build demo application
$(DEMO_APP): $(EXAMPLE_DIR)/demo_app.c $(STATIC_LIB)
	@echo "Building demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Demo application built successfully"

# Build HTTP server application
$(HTTP_SERVER_APP): $(EXAMPLE_DIR)/http_server_app.c $(STATIC_LIB)
	@echo "Building HTTP server application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "HTTP server application built successfully"

# Build HTTP/2 server application
$(HTTP2_SERVER_APP): $(EXAMPLE_DIR)/http2_server_app.c $(STATIC_LIB)
	@echo "Building HTTP/2 server application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "HTTP/2 server application built successfully"

# Build parameter demo application
$(PARAM_DEMO): $(EXAMPLE_DIR)/param_demo.c $(STATIC_LIB)
	@echo "Building parameter demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Parameter demo application built successfully"

# Build Kafka demo application
$(KAFKA_DEMO): $(EXAMPLE_DIR)/kafka_demo.c $(STATIC_LIB)
	@echo "Building Kafka demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Kafka demo application built successfully"

# Build Kafka SSL demo application
$(KAFKA_SSL_DEMO): $(EXAMPLE_DIR)/kafka_ssl_demo.c $(STATIC_LIB)
	@echo "Building Kafka SSL demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Kafka SSL demo application built successfully"

# Build Kafka multi-topic demo application
$(KAFKA_MULTI_TOPIC_DEMO): $(EXAMPLE_DIR)/kafka_multi_topic_demo.c $(STATIC_LIB)
	@echo "Building Kafka multi-topic demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Kafka multi-topic demo application built successfully"

# Build Kafka authentication demo application
$(KAFKA_AUTH_DEMO): $(EXAMPLE_DIR)/kafka_auth_demo.c $(STATIC_LIB)
	@echo "Building Kafka authentication demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Kafka authentication demo application built successfully"

# Build unified application (HTTP + Kafka)
$(UNIFIED_APP): $(EXAMPLE_DIR)/unified_app.c $(STATIC_LIB)
	@echo "Building unified HTTP+Kafka application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Unified application built successfully"

# Build JSON schema demo application
$(JSON_SCHEMA_DEMO): $(EXAMPLE_DIR)/json_schema_demo.c $(STATIC_LIB)
	@echo "Building JSON schema demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "JSON schema demo application built successfully"

# Build static server demo application
$(STATIC_SERVER_DEMO): $(EXAMPLE_DIR)/static_server_demo.c $(STATIC_LIB)
	@echo "Building static server demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Static server demo application built successfully"

# Build HTTP client demo application
$(HTTP_CLIENT_DEMO): $(EXAMPLE_DIR)/http_client_demo.c $(STATIC_LIB)
	@echo "Building HTTP client demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "HTTP client demo application built successfully"

# Build HTTP proxy demo application
$(HTTP_PROXY_DEMO): $(EXAMPLE_DIR)/http_proxy_demo.c $(STATIC_LIB)
	@echo "Building HTTP proxy demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "HTTP proxy demo application built successfully"

# Build MongoDB demo application
$(MONGO_DEMO): $(EXAMPLE_DIR)/mongo_demo.c $(STATIC_LIB)
	@echo "Building MongoDB demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "MongoDB demo application built successfully"

# Build MongoDB async demo application
$(MONGO_ASYNC_DEMO): $(EXAMPLE_DIR)/mongo_async_demo.c $(STATIC_LIB)
	@echo "Building MongoDB async demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "MongoDB async demo application built successfully"

$(MONGO_HANDLE_DEMO): $(EXAMPLE_DIR)/mongo_handle_management_demo.c $(STATIC_LIB)
	@echo "Building MongoDB handle management demo application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "MongoDB handle management demo application built successfully"

$(WS_ECHO_SERVER): $(EXAMPLE_DIR)/websocket_echo_server.c $(STATIC_LIB)
	@echo "Building WebSocket echo server application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "WebSocket echo server application built successfully"

$(SIO_CHAT_SERVER): $(EXAMPLE_DIR)/socketio_chat_server.c $(STATIC_LIB)
	@echo "Building Socket.IO chat server application..."
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) -o $@
	@echo "Socket.IO chat server application built successfully"

# Build OpenTelemetry demo application.
# The demo source is plain C (compiled with $(CC)), but the final link must
# use $(CXX) because libequinox.a now contains a C++ object (otel.o) that
# depends on libstdc++ and the OpenTelemetry C++ SDK static libraries.
$(BUILD_DIR)/otel_demo.o: $(EXAMPLE_DIR)/otel_demo.c | directories
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I$(OTEL_INC) -c $< -o $@

$(OTEL_DEMO): $(BUILD_DIR)/otel_demo.o $(STATIC_LIB)
	@echo "Building OpenTelemetry demo application..."
	$(CXX) $< -L$(LIB_DIR) -lequinox $(LDFLAGS) $(OTEL_LIBS) -o $@
	@echo "OpenTelemetry demo application built successfully"

# Run HTTP server application

# Run HTTP/2 server application
.PHONY: run-http2
run-http2: debug
	@echo "Running HTTP/2 server application..."
	@echo ""
	./$(HTTP2_SERVER_APP)

.PHONY: run-server
run-server: debug
	@echo "Running HTTP server application..."
	@echo ""
	./$(HTTP_SERVER_APP)

# Run parameter demo application
.PHONY: run-param
run-param: debug
	@echo "Running parameter demo application..."
	@echo ""
	./$(PARAM_DEMO)

# Run Kafka demo application
.PHONY: run-kafka
run-kafka: debug
	@echo "Running Kafka demo application..."
	@echo ""
	./$(KAFKA_DEMO)

# Run Kafka SSL demo application
.PHONY: run-kafka-ssl
run-kafka-ssl: debug
	@echo "Running Kafka SSL demo application..."
	@echo ""
	@echo "Usage: ./build/kafka_ssl_demo <broker:port> <ca-cert> <client-cert> <client-key> [key-password]"
	@echo ""

# Run Kafka multi-topic demo application
.PHONY: run-kafka-multi
run-kafka-multi: debug
	@echo "Running Kafka multi-topic demo application..."
	@echo ""
	./$(KAFKA_MULTI_TOPIC_DEMO)

# Run Kafka authentication demo application
.PHONY: run-kafka-auth
run-kafka-auth: debug
	@echo "Running Kafka authentication demo application..."
	@echo ""
	@echo "Usage: ./build/kafka_auth_demo <broker> <username> <password> [auth-type]"
	@echo ""

# Run unified application (HTTP + Kafka)
.PHONY: run-unified
run-unified: debug
	@echo "Running unified HTTP+Kafka application..."
	@echo ""
	@echo "Usage: ./build/unified_app [broker] [http_port]"
	@echo "Default: localhost:9092 8080"
	@echo ""
	./$(UNIFIED_APP)

# Run JSON schema demo
.PHONY: run-json
run-json: debug
	@echo "Running JSON schema demo..."
	@echo ""
	./$(JSON_SCHEMA_DEMO)

# Run static server demo
.PHONY: run-static
run-static: debug
	@echo "Running static file server demo..."
	@echo ""
	./$(STATIC_SERVER_DEMO)


# Run HTTP proxy demo
.PHONY: run-proxy
run-proxy: debug
	@echo "Running HTTP proxy demo..."
	@echo ""
	./$(HTTP_PROXY_DEMO)
# Run HTTP client demo
.PHONY: run-http-client
run-http-client: debug
	@echo "Running HTTP client demo..."
	@echo ""
	./$(HTTP_CLIENT_DEMO)

# Run demo application
.PHONY: run
run: debug
	@echo "Running demo application..."
	@echo ""
	./$(DEMO_APP)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(LIB_DIR)
	@echo "Clean complete"

# Install library (optional)
# NOTE: depends on `release`, which currently fails to build - see the NOTE
# above the `release` target. Use `make debug` + manual copy as a workaround
# until that's fixed.
.PHONY: install
install: release
	@echo "Installing library..."
	@mkdir -p /usr/local/lib
	@mkdir -p /usr/local/include/equinox
	cp $(STATIC_LIB) /usr/local/lib/
	cp -r $(INC_DIR)/. /usr/local/include/equinox/
	@echo "Library installed"
	@echo "Installing man pages..."
	@$(MAKE) -C man install
	@echo "Installation complete"

# Uninstall library
.PHONY: uninstall
uninstall:
	@echo "Uninstalling library..."
	rm -f /usr/local/lib/$(LIB_NAME)
	rm -rf /usr/local/include/equinox
	@echo "Library uninstalled"
	@echo "Uninstalling man pages..."
	@$(MAKE) -C man uninstall
	@echo "Uninstallation complete"

# Help target
.PHONY: help
help:
	@echo "Hanuman Framework Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build debug version (default)"
	@echo "  debug        - Build debug version with debug symbols"
	@echo "  release      - Build optimized release version (see NOTE below)"
	@echo "  run          - Build and run the demo application"
	@echo "  run-server   - Build and run HTTP server"
	@echo "  run-http2    - Build and run HTTP/2 server"
	@echo "  run-static   - Build and run static file server demo"
	@echo "  run-proxy    - Build and run HTTP proxy demo"
	@echo "  run-http-client - Build and run HTTP client demo"
	@echo "  run-param    - Build and run path parameter demo"
	@echo "  run-kafka    - Build and run Kafka demo"
	@echo "  run-kafka-ssl   - Build and run Kafka SSL demo (see usage below)"
	@echo "  run-kafka-multi - Build and run Kafka multi-topic demo"
	@echo "  run-kafka-auth  - Build and run Kafka authentication demo (see usage below)"
	@echo "  run-unified  - Build and run unified HTTP+Kafka app"
	@echo "  run-json     - Build and run JSON schema demo"
	@echo "  clean        - Remove all build artifacts"
	@echo "  install      - Install library to system (requires sudo)"
	@echo "  uninstall    - Remove library from system (requires sudo)"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Other demo binaries built by 'make debug'/'make release' (no dedicated"
	@echo "run-* target - run directly from build/):"
	@echo "  build/mongo_demo, build/mongo_async_demo,"
	@echo "  build/mongo_handle_management_demo"
	@echo "  build/websocket_echo_server, build/socketio_chat_server"
	@echo "  build/otel_demo [ostream|otlp] [otlp-endpoint]"
	@echo ""
	@echo "Docker (see Dockerfile / README.md for details):"
	@echo "  docker build -t hanuman-framework .              # runtime image"
	@echo "  docker build --target sdk -t hanuman-framework:sdk .  # SDK base image"
	@echo ""
	@echo "Example usage:"
	@echo "  make              # Build debug version"
	@echo "  make release      # Build release version"
	@echo "  make run          # Build and run demo"
	@echo "  make clean        # Clean build files"
