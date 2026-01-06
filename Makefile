# Hanuman Framework Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -Isrc/include
LDFLAGS = -lrdkafka -lpthread -lssl -lcrypto -lz
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG

# Directories
SRC_DIR = src
INC_DIR = src/include
BUILD_DIR = build
LIB_DIR = lib
EXAMPLE_DIR = examples

# Source files
SOURCES = $(SRC_DIR)/application.c \
          $(SRC_DIR)/module.c \
          $(SRC_DIR)/service_controller.c \
          $(SRC_DIR)/framework.c \
          $(SRC_DIR)/http_server.c \
          $(SRC_DIR)/http_route.c \
          $(SRC_DIR)/http2.c \
          $(SRC_DIR)/http_client.c \
          $(SRC_DIR)/kafka_client.c \
          $(SRC_DIR)/json_parser.c

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

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

# Default target
.PHONY: all
all: debug

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: directories $(STATIC_LIB) $(DEMO_APP) $(HTTP_SERVER_APP) $(HTTP2_SERVER_APP) $(PARAM_DEMO) $(KAFKA_DEMO) $(KAFKA_SSL_DEMO) $(KAFKA_MULTI_TOPIC_DEMO) $(KAFKA_AUTH_DEMO) $(UNIFIED_APP) $(JSON_SCHEMA_DEMO) $(STATIC_SERVER_DEMO) $(HTTP_CLIENT_DEMO) $(HTTP_PROXY_DEMO)
	@echo "Debug build complete"

# Release build
.PHONY: release
release: CFLAGS += $(RELEASE_FLAGS)
release: directories $(STATIC_LIB) $(DEMO_APP) $(HTTP_SERVER_APP) $(HTTP2_SERVER_APP) $(PARAM_DEMO) $(KAFKA_DEMO) $(KAFKA_SSL_DEMO) $(KAFKA_MULTI_TOPIC_DEMO) $(KAFKA_AUTH_DEMO) $(UNIFIED_APP) $(JSON_SCHEMA_DEMO) $(STATIC_SERVER_DEMO) $(HTTP_CLIENT_DEMO) $(HTTP_PROXY_DEMO)
	@echo "Release build complete"

# Create directories
.PHONY: directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(LIB_DIR)

# Build object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build static library
$(STATIC_LIB): $(OBJECTS)
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
.PHONY: install
install: release
	@echo "Installing library..."
	@mkdir -p /usr/local/lib
	@mkdir -p /usr/local/include/equinox
	cp $(STATIC_LIB) /usr/local/lib/
	cp $(INC_DIR)/*.h /usr/local/include/equinox/
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
	@echo "  all        - Build debug version (default)"
	@echo "  debug      - Build debug version with debug symbols"
	@echo "  release    - Build optimized release version"
	@echo "  run        - Build and run the demo application"
	@echo "  run-http   - Build and run HTTP server"
	@echo "  run-http2  - Build and run HTTP/2 server"
	@echo "  run-kafka  - Build and run Kafka demo"
	@echo "  run-unified - Build and run unified HTTP+Kafka app"
	@echo "  run-json   - Build and run JSON schema demo"
	@echo "  clean      - Remove all build artifacts"
	@echo "  install    - Install library to system (requires sudo)"
	@echo "  uninstall  - Remove library from system (requires sudo)"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "  make              # Build debug version"
	@echo "  make release      # Build release version"
	@echo "  make run          # Build and run demo"
	@echo "  make clean        # Clean build files"
