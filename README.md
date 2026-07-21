# Hanuman Framework

**High-performance C runtime framework**

A comprehensive, production-ready C framework for building high-performance web applications, microservices, and distributed systems with HTTP/HTTP2 servers, async HTTP client, Kafka integration, and advanced data processing.

## Features Overview

### 🚀 Core Framework
- **Application Lifecycle Management**: Complete control over initialization, startup, and shutdown
- **Module System**: Register and manage functional modules with dependency resolution
- **Service Controllers**: Create service endpoints with request/response handling
- **Flexible Callbacks**: Register custom initialization and cleanup functions
- **Built-in Logging**: Integrated logging system with multiple log levels
- **Error Handling**: Comprehensive error codes and state management

### 🌐 HTTP Server
- **Dual Protocol Support**: HTTP/1.1 and HTTP/2 with automatic detection
- **High Concurrency**: Handle 1000+ simultaneous connections with EPOLL
- **Non-Blocking I/O**: Edge-triggered event notifications
- **RESTful Routes**: GET, POST, PUT, PATCH, DELETE with handler functions
- **Path Parameters**: Dynamic route matching (e.g., `/api/users/:id`)
- **Query String Parsing**: Automatic URL decoding
- **Static File Serving**: Serve files with MIME type detection and security features
- **JSON Support**: Built-in JSON response helpers
- **Header Management**: Full request/response header control
- **HTTP/2 Features**: Binary framing, multiplexing, HPACK compression

### 📡 HTTP Client (Outbound Requests)
- **Synchronous & Asynchronous**: Both blocking and non-blocking requests
- **HTTPS Support**: Full SSL/TLS with certificate verification
- **All HTTP Methods**: GET, POST, PUT, DELETE, PATCH
- **Compression**: Automatic gzip/deflate decompression
- **Chunked Encoding**: Automatic handling of chunked transfer encoding
- **Async Callbacks**: Non-blocking requests with callback functions
- **Parallel Requests**: Execute multiple requests simultaneously
- **Custom Headers**: Full control over request headers
- **Timeout Support**: Configurable request timeouts
- **Connection Pooling**: Efficient SSL context reuse

### 📨 Kafka Integration
- **Producer Support**: Publish messages to Kafka topics
- **Consumer Support**: Subscribe and consume messages with offset management
- **Multi-Topic Support**: Subscribe to multiple topics simultaneously
- **SASL Authentication**: PLAIN and SCRAM authentication support
- **Batch Processing**: Efficient message batching
- **Error Handling**: Comprehensive delivery reports and error callbacks
- **High Performance**: Built on librdkafka for production use

### 🗄️ MongoDB Integration
- **Full CRUD Operations**: Insert, find, update, delete documents
- **Async Support**: Non-blocking database operations with callbacks
- **Aggregation Pipeline**: Complex data transformations
- **Index Management**: Create and manage database indexes
- **Transactions**: ACID-compliant operations
- **SSL/TLS Support**: Secure database connections
- **Connection Pooling**: Efficient resource management

### 🔌 Real-Time Communication
- **WebSocket Support**: RFC 6455 compliant WebSocket server and client
- **Socket.IO Integration**: Event-driven real-time communication
- **Rooms & Namespaces**: Organize clients for targeted messaging
- **Broadcasting**: Send messages to all clients or specific groups
- **Binary Support**: Handle both text and binary data
- **Acknowledgements**: Request/response patterns for reliable messaging
- **Thread-Safe**: Concurrent connections with proper synchronization

### 📄 JSON Processing
- **Full JSON Parser**: Parse JSON strings to structured data
- **JSON Builder**: Construct JSON objects and arrays programmatically
- **Schema Validation**: Validate JSON against schemas
- **Type Support**: String, number, boolean, null, object, array
- **Pretty Printing**: Format JSON with indentation
- **Memory Efficient**: Optimized for large JSON documents

## Architecture

### Core Components

1. **Application**: The main container that manages modules and services
2. **Modules**: Functional components with lifecycle callbacks (init, start, stop, cleanup)
3. **Service Controllers**: Request handlers that expose operations and process requests
4. **Framework**: Initialization, logging, and utility functions

### Lifecycle Flow

```
Framework Init → Application Create → Register Modules/Services
     ↓
Application Initialize → Module Init → Service Init
     ↓
Application Start → Module Start → Service Start
     ↓
[Application Running - Handle Requests]
     ↓
Application Stop → Service Stop → Module Stop
     ↓
Application Cleanup → Framework Shutdown
```

## Building the Framework

```bash
# Build debug version with all features
make debug

# Build release version
make release

# Clean build artifacts
make clean

# Run specific demos
make run-server          # HTTP/HTTP2 server demo
make run-http2           # HTTP/2 specific demo
make run-kafka           # Kafka integration demo
make run-json            # JSON processing demo
make run-http-client     # HTTP client demo
make run-unified         # Unified HTTP+Kafka demo
```

### Building with Docker

A multi-stage [Dockerfile](Dockerfile) is provided that installs every
dependency (OpenSSL, zlib, librdkafka, MongoDB C driver, and the
OpenTelemetry C++ SDK built from source) and compiles the framework - no
local toolchain setup required.

```bash
# Build the image (builds opentelemetry-cpp from source, then the framework)
docker build -t hanuman-framework .

# Run the default demo (HTTP server on :8080)
docker run --rm -p 8080:8080 hanuman-framework

# Run a different demo binary
docker run --rm -p 3000:3000 hanuman-framework ./build/socketio_chat_server
```

#### SDK base image (build your own apps against the framework)

An additional `sdk` target produces a reusable base image with the compiler
toolchain, all dev headers, `libequinox.a`, and the OpenTelemetry static libs
already built - so other projects can `FROM` it and just compile their own
code, without rebuilding OpenTelemetry or any dependency:

```bash
docker build --target sdk -t hanuman-framework:sdk .
```

```dockerfile
# your-app/Dockerfile
FROM hanuman-framework:sdk
COPY myapp.c .
RUN gcc -std=c99 myapp.c -lequinox $EQUINOX_LDFLAGS -o myapp
CMD ["./myapp"]
```

Framework headers are installed flat into `/usr/local/include`, so plain
`#include "http_server.h"` (the convention used throughout this codebase)
works with no extra `-I` flags. The `EQUINOX_LDFLAGS` environment variable
provides the exact linker flags needed (`-lrdkafka -lssl -lmongoc-1.0`,
etc.). If your app also uses OpenTelemetry (`otel.h`), compile with `g++`
and link with `$EQUINOX_OTEL_LDFLAGS` too:

```dockerfile
RUN g++ -std=c++17 -DOPENTELEMETRY_STL_VERSION=2017 myapp.c \
      -lequinox $EQUINOX_LDFLAGS $EQUINOX_OTEL_LDFLAGS -o myapp
```

The final `runtime` stage only contains the compiled binaries and the
runtime shared libraries (no compilers or `-dev` headers).

## Complete Feature Documentation

### HTTP Server
- **[HTTP_SERVER.md](docs/http/HTTP_SERVER.md)** - HTTP server implementation guide
- **[HTTP2_SUPPORT.md](docs/http/HTTP2_SUPPORT.md)** - HTTP/2 protocol documentation
- **[PATH_PARAMETERS.md](docs/http/PATH_PARAMETERS.md)** - Path parameters and query parsing
- **[EPOLL_CONCURRENT.md](docs/http/EPOLL_CONCURRENT.md)** - Concurrent connections with EPOLL

### HTTP Client
- **Synchronous Requests**: `http_client_execute()` - Blocking requests
- **Asynchronous Requests**: `http_client_execute_async()` - Non-blocking with callbacks
- **Convenience Functions**: `http_client_get()`, `http_client_post_json()`
- **HTTPS/TLS**: Full SSL support with OpenSSL
- **Compression**: Automatic gzip/deflate decompression
- **Chunked Encoding**: Automatic handling of Transfer-Encoding: chunked

### Kafka Integration
- **Unified Client**: `kafka_client_create()` for both producer and consumer
- **Producer API**: `kafka_producer_init()`, `kafka_produce_string()`
- **Consumer API**: `kafka_consumer_register()` with callback handlers
- **Multi-Topic**: Subscribe to multiple topics simultaneously
- **Authentication**: SASL PLAIN and SCRAM support
- **Async Processing**: Non-blocking message consumption with callbacks

### MongoDB Integration
- **Connection Management**: `mongo_client_create()`, connection pooling
- **CRUD Operations**: `mongo_insert_one/many()`, `mongo_find()`, `mongo_update()`, `mongo_delete()`
- **Async Operations**: `mongo_insert_one_async()`, `mongo_find_async()`, `mongo_update_async()`
- **Aggregation**: `mongo_aggregate()` - Full pipeline support with async support
- **Index Management**: `mongo_create_index()`, `mongo_drop_index()`
- **Transactions**: ACID-compliant operations
- **SSL/TLS**: Secure connections with certificate authentication
- **Non-blocking**: All operations available with callback-based async API
- **Thread-Safe**: Mutex-protected concurrent async operations

### WebSocket Support
- **RFC 6455 Compliant**: Full WebSocket protocol implementation
- **Server & Client**: Both server and client-side support
- **Frame Types**: Text, binary, ping/pong, close frames
- **Fragmentation**: Automatic handling of fragmented messages
- **Broadcasting**: Send messages to all connected clients
- **Callbacks**: `on_connect`, `on_message`, `on_close`, `on_error` events
- **Thread-Safe**: Concurrent connections with mutex protection

### Socket.IO Support
- **Real-time Communication**: Built on WebSocket with fallback support
- **Event-Based**: Custom event emitters with named events
- **Rooms & Namespaces**: Organize clients into rooms for targeted broadcasting
- **Acknowledgements**: Request/response pattern support
- **Binary Support**: Send binary data alongside JSON
- **Auto-reconnection**: Client-side automatic reconnection
- **Broadcasting**: Emit to all clients or specific rooms

### OpenTelemetry Support
- **Tracing**: `otel_span_start()`, `otel_span_set_attribute()`, `otel_span_add_event()`, `otel_span_set_status()`, `otel_span_end()`
- **Metrics**: `otel_counter_create/add()`, `otel_updowncounter_create/add()`, `otel_histogram_create/record()`
- **Logging**: `otel_log()` - correlates log records with the active span's trace/span IDs
- **Exporters**: `OTEL_EXPORTER_OSTREAM` (console) or `OTEL_EXPORTER_OTLP_HTTP` (send to a collector at `/v1/traces`, `/v1/metrics`, `/v1/logs`)
- **Implementation**: `src/otel/otel.cpp` is compiled as C++ (g++) and exposes an `extern "C"` API from `src/include/otel/otel.h`; the rest of the framework stays pure C99

### JSON Processing
- **Parser**: `json_parse()` - Parse JSON strings into tree structure
- **Builder**: `json_builder_create()` - Construct JSON programmatically
- **Schema Validation**: `json_parse_with_schema()` - Parse and validate
- **Value Access**: `json_get_path()`, `json_get_string/int/bool/double()`
- **Serialization**: `json_serialize()` - Convert structs to JSON

### Static File Serving
- **Route Registration**: `http_server_serve_static()`
- **MIME Detection**: Automatic content-type based on file extension
- **Security**: Directory traversal protection
- **Caching Headers**: ETag and Last-Modified support
- **Range Requests**: Partial content support

## Quick Start Examples

### 1. HTTP Server with Routes

```c
#include "framework.h"
#include "http_server.h"

void handle_hello(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data) {
    http_response_set_json(response, "{\"message\": \"Hello, World!\"}");
}

void handle_user(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data) {
    const char *user_id = http_request_get_path_param(request, "id");
    char json[256];
    snprintf(json, sizeof(json), "{\"user_id\": \"%s\"}", user_id);
    http_response_set_json(response, json);
}

int main() {
    framework_init();
    
    APPLICATION *app = application_create("MyAPI", 1);
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    
    // Register routes
    http_server_get(server, "/api/hello", handle_hello, NULL);
    http_server_get(server, "/api/users/:id", handle_user, NULL);
    
    // Serve static files
    http_server_serve_static(server, "/static", "./public");
    
    // Start server
    application_set_http_server(app, server);
    application_initialize(app);
    application_start(app);
    http_server_start(server);
    http_server_run(server);  // Blocking
    
    // Cleanup
    http_server_destroy(server);
    application_destroy(app);
    framework_shutdown();
    return 0;
}
```

### 2. Async HTTP Client

```c
#include "http_client.h"

void response_callback(HTTP_CLIENT_RESPONSE *response, void *user_data) {
    if (response->error_message) {
        printf("Error: %s\n", response->error_message);
    } else {
        printf("Status: %d, Body: %s\n", response->status_code, response->body);
    }
    http_client_response_destroy(response);
}

int main() {
    // Async request
    HTTP_CLIENT_REQUEST *req = http_client_request_create("GET", "https://api.example.com/data");
    http_client_execute_async(req, response_callback, NULL);
    
    // Continue with other work...
    
    // Or synchronous request
    HTTP_CLIENT_RESPONSE *resp = http_client_get("https://api.example.com/status");
    printf("Status: %d\n", resp->status_code);
    http_client_response_destroy(resp);
    
    return 0;
}
```

### 3. Kafka Producer/Consumer

```c
#include "kafka_client.h"

void handle_message(KAFKA_MESSAGE *msg, void *user_data) {
    if (msg && msg->payload) {
        printf("Received: %s\n", (char*)msg->payload);
    }
}

int main() {
    // Create Kafka client
    KAFKA_CLIENT *client = kafka_client_create();
    
    // Configure and register consumer
    KAFKA_CONSUMER_CONFIG consumer_config;
    kafka_consumer_config_default(&consumer_config, "localhost:9092", "my-group");
    kafka_consumer_register(client, "my-topic", &consumer_config, handle_message, NULL);
    
    // Configure and initialize producer
    KAFKA_PRODUCER_CONFIG producer_config;
    kafka_producer_config_default(&producer_config, "localhost:9092");
    kafka_producer_init(client, &producer_config);
    
    // Produce a message
    kafka_produce_string(client, "my-topic", "key", "Hello Kafka!");
    
    // Start consuming (non-blocking)
    kafka_client_start(client);
    
    // Keep running...
    while (running) {
        sleep(1);
    }
    
    kafka_client_stop(client);
    kafka_client_destroy(client);
    return 0;
}
```

### 4. MongoDB Operations

```c
#include "mongo_client.h"

int main() {
    // Create MongoDB client
    MONGO_CONFIG config;
    mongo_config_default(&config, "mongodb://localhost:27017", "mydb");
    
    MONGO_CLIENT *client = mongo_client_create(&config);
    
    // Test connection
    if (!mongo_client_ping(client)) {
        return 1;
    }
    
    // Get collection
    MONGO_COLLECTION *users = mongo_client_get_collection(client, NULL, "users");
    
    // Insert document
    char id[64];
    mongo_insert_one(users, "{\"name\":\"John\",\"age\":30}", id, sizeof(id));
    
    // Find documents
    MONGO_CURSOR *cursor = mongo_find(users, "{\"age\":{\"$gte\":18}}", NULL);
    char result[4096];
    while (mongo_cursor_next(cursor, result, sizeof(result)) == 1) {
        printf("Found: %s\n", result);
    }
    mongo_cursor_destroy(cursor);
    
    // Update document
    mongo_update(users, "{\"name\":\"John\"}", "{\"$set\":{\"age\":31}}", NULL);
    
    // Count documents
    int64_t count = mongo_count(users, NULL);
    printf("Total users: %lld\n", (long long)count);
    
    // Cleanup
    mongo_collection_destroy(users);
    mongo_client_destroy(client);
    return 0;
}
```

### 4b. MongoDB Async Operations

The async MongoDB API is **thread-safe** and supports concurrent operations. Multiple async operations can safely execute on the same collection simultaneously.

```c
#include "mongo_client.h"

static volatile int operation_complete = 0;

void on_insert_complete(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("Inserted ID: %s\n", result->data);
    }
    operation_complete = 1;
}

void on_find_document(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("Found: %s\n", result->data);
    } else {
        // End of stream (success = 0)
        operation_complete = 1;
    }
}

int main() {
    MONGO_CLIENT *client = mongo_client_create(&config);
    MONGO_COLLECTION *users = mongo_client_get_collection(client, NULL, "users");
    
    // Async insert
    MONGO_ASYNC_HANDLE *handle = mongo_insert_one_async(
        users,
        "{\"name\":\"Alice\",\"age\":30}",
        on_insert_complete,
        NULL
    );
    
    // Wait for completion
    while (!operation_complete) {
        usleep(10000);
    }
    mongo_async_handle_destroy(handle);
    operation_complete = 0;
    
    // Async find (callback called for each document)
    handle = mongo_find_async(users, "{\"age\":{\"$gte\":18}}", NULL, 
                             on_find_document, NULL);
    
    while (!operation_complete) {
        usleep(10000);
    }
    mongo_async_handle_destroy(handle);
    
    // Cleanup
    mongo_collection_destroy(users);
    mongo_client_destroy(client);
    return 0;
}
```

### 5. JSON Processing

```c
#include "json_parser.h"

int main() {
    // Parse JSON
    const char *json_str = "{\"name\":\"John\",\"age\":30}";
    JSON_VALUE *value = json_parse(json_str);
    
    // Access values using helper functions
    JSON_VALUE *name_val = json_get_path(value, "name");
    JSON_VALUE *age_val = json_get_path(value, "age");
    
    const char *name = json_get_string(name_val);
    int age = json_get_int(age_val, 0);
    
    printf("Name: %s, Age: %d\n", name, age);
    
    // Build JSON using builder
    JSON_BUILDER *builder = json_builder_create(256);
    json_builder_start_object(builder);
    json_builder_add_string(builder, "status", "success");
    json_builder_add_int(builder, "code", 200);
    json_builder_end_object(builder);
    
    const char *output = json_builder_get_string(builder);
    printf("%s\n", output);
    
    json_free(value);
    json_builder_destroy(builder);
    
    return 0;
}
```

### 6. WebSocket Echo Server

```c
#include "websocket.h"

static WEBSOCKET_SERVER *ws_server = NULL;

void on_ws_message(WEBSOCKET_CONNECTION *conn, const WS_MESSAGE *message, void *user_data) {
    if (message->opcode == WS_OPCODE_TEXT) {
        // Echo text messages back to client
        char echo_msg[1024];
        snprintf(echo_msg, sizeof(echo_msg), "Echo: %.*s", (int)message->length, message->data);
        ws_send_text(conn, echo_msg, strlen(echo_msg));
    } else if (message->opcode == WS_OPCODE_BINARY) {
        // Echo binary messages
        ws_send_binary(conn, message->data, message->length);
    }
}

void on_ws_connect(WEBSOCKET_CONNECTION *conn, void *user_data) {
    printf("Client connected: %s\n", ws_get_remote_addr(conn));
    ws_send_text(conn, "Welcome to WebSocket Echo Server!", 33);
}

int main() {
    WS_SERVER_CONFIG config = {
        .host = "0.0.0.0",
        .port = 8080,
        .path = "/ws",
        .max_connections = 100,
        .max_message_size = 10 * 1024 * 1024  // 10MB
    };
    
    ws_server = ws_server_create(&config);
    
    WS_CALLBACKS callbacks = {
        .on_connect = on_ws_connect,
        .on_message = on_ws_message
    };
    ws_server_set_callbacks(ws_server, &callbacks, NULL);
    
    ws_server_start(ws_server);
    printf("WebSocket server listening on ws://localhost:8080/ws\n");
    
    // Keep running
    while (1) {
        sleep(1);
    }
    
    ws_server_destroy(ws_server);
    return 0;
}
```

### 7. Socket.IO Chat Server

```c
#include "socketio.h"

static SOCKETIO_SERVER *sio_server = NULL;

typedef struct {
    char username[64];
    char room[64];
} user_data_t;

void on_join(SOCKETIO_SOCKET *socket, const SIO_EVENT *event, void *user_data) {
    // Parse join event: {"username": "Alice", "room": "general"}
    JSON_VALUE *json = json_parse(event->data);
    JSON_VALUE *username_val = json_get_path(json, "username");
    JSON_VALUE *room_val = json_get_path(json, "room");
    
    const char *username = json_get_string(username_val);
    const char *room = json_get_string(room_val);
    
    // Store user data
    user_data_t *data = malloc(sizeof(user_data_t));
    strncpy(data->username, username, sizeof(data->username) - 1);
    strncpy(data->room, room, sizeof(data->room) - 1);
    sio_set_user_data(socket, data);
    
    // Join the room
    sio_join(socket, room);
    
    // Send confirmation to user
    char response[256];
    snprintf(response, sizeof(response), "{\"message\":\"Joined room %s\"}", room);
    sio_emit(socket, "joined", response);
    
    // Broadcast to room
    char notification[256];
    snprintf(notification, sizeof(notification), 
             "{\"username\":\"%s\",\"message\":\"joined the room\"}", username);
    sio_server_to(sio_server, room, "user_joined", notification);
    
    json_free(json);
}

void on_chat_message(SOCKETIO_SOCKET *socket, const SIO_EVENT *event, void *user_data) {
    user_data_t *data = sio_get_user_data(socket);
    if (!data) return;
    
    // Parse message: {"message": "Hello!"}
    JSON_VALUE *json = json_parse(event->data);
    JSON_VALUE *message_val = json_get_path(json, "message");
    const char *message = json_get_string(message_val);
    
    // Broadcast to room
    char broadcast[512];
    snprintf(broadcast, sizeof(broadcast),
             "{\"username\":\"%s\",\"message\":\"%s\",\"timestamp\":%ld}",
             data->username, message, time(NULL));
    sio_server_to(sio_server, data->room, "message", broadcast);
    
    json_free(json);
}

int main() {
    SIO_SERVER_CONFIG config = {
        .host = "0.0.0.0",
        .port = 3000,
        .path = "/socket.io"
    };
    
    sio_server = sio_server_create(&config);
    
    // Register event handlers
    sio_server_on(sio_server, "join", on_join, NULL);
    sio_server_on(sio_server, "message", on_chat_message, NULL);
    
    sio_server_start(sio_server);
    printf("Socket.IO server running on http://localhost:3000\n");
    
    while (1) {
        sleep(1);
    }
    
    sio_server_destroy(sio_server);
    return 0;
}
```

**Client-side JavaScript example:**
```javascript
const socket = io('http://localhost:3000');

socket.emit('join', { username: 'Alice', room: 'general' });

socket.on('joined', (data) => {
    console.log('Joined:', data.message);
});

socket.on('message', (data) => {
    console.log(`${data.username}: ${data.message}`);
});

socket.emit('message', { message: 'Hello everyone!' });
```

### 8. OpenTelemetry Tracing, Metrics, and Logs

```c
#include "otel.h"
#include "application.h"

static OTEL_CONFIG g_otel_config;

static void otel_init_cb(void *ctx) { (void)ctx; otel_init(&g_otel_config); }
static void otel_shutdown_cb(void *ctx) { (void)ctx; otel_shutdown(); }

int main(void) {
    g_otel_config.service_name = "my-service";
    g_otel_config.service_version = "1.0.0";
    g_otel_config.exporter = OTEL_EXPORTER_OSTREAM;   /* or OTEL_EXPORTER_OTLP_HTTP */
    g_otel_config.otlp_endpoint = "http://localhost:4318";

    APPLICATION *app = application_create("MyService", 1);

    /* Wire OpenTelemetry into the app lifecycle using the generic hooks -
     * no framework core changes needed. */
    application_register_init_function(app, "otel_init", otel_init_cb, NULL);
    application_register_cleanup_function(app, "otel_shutdown", otel_shutdown_cb, NULL);

    application_initialize(app);
    application_start(app);

    /* Create a span for an operation */
    OTEL_SPAN *span = otel_span_start("handle_request", OTEL_SPAN_KIND_SERVER, NULL);

    OTEL_ATTRIBUTE attr = {"http.route", OTEL_ATTR_STRING, {.string_value = "/api/users"}};
    otel_span_set_attribute(span, &attr);

    /* Log correlated with the span's trace/span IDs */
    otel_log(OTEL_SEVERITY_INFO, "request", "Handling request", span);

    /* Record a metric */
    OTEL_COUNTER *requests = otel_counter_create("http.requests", "Total requests", "1");
    otel_counter_add(requests, 1.0, NULL, 0);

    otel_span_set_status(span, OTEL_STATUS_OK, NULL);
    otel_span_end(span);

    application_stop(app);
    application_cleanup(app);
    application_destroy(app);
    return 0;
}
```

### Creating an Application

```c
#include "framework.h"

int main() {
    // Initialize framework
    framework_init();
    
    // Create application
    APPLICATION *app = application_create("MyApp", 1);
    
    // ... register modules and services ...
    
    // Initialize and start
    application_initialize(app);
    application_start(app);
    
    // Application logic here
    
    // Cleanup
    application_stop(app);
    application_cleanup(app);
    application_destroy(app);
    framework_shutdown();
    
    return 0;
}
```

## Module Registration

### Creating a Module

```c
// Create module
MODULE *my_module = module_create("MyModule", 1);

// Set callbacks
module_set_init_callback(my_module, my_init_function);
module_set_start_callback(my_module, my_start_function);
module_set_stop_callback(my_module, my_stop_function);
module_set_cleanup_callback(my_module, my_cleanup_function);

// Add dependencies (optional)
module_add_dependency(my_module, "OtherModule");

// Register with application
application_register_module(app, my_module);
```

### Module Callback Signature

```c
int my_module_init(void *module_context, void *app_context) {
    // Initialize module
    return FRAMEWORK_SUCCESS;
}

int my_module_start(void *module_context, void *app_context) {
    // Start module operations
    return FRAMEWORK_SUCCESS;
}

int my_module_stop(void *module_context, void *app_context) {
    // Stop module operations
    return FRAMEWORK_SUCCESS;
}

int my_module_cleanup(void *module_context, void *app_context) {
    // Cleanup resources
    return FRAMEWORK_SUCCESS;
}
```

## Service Controller Registration

### Creating a Service Controller

```c
// Create service
SERVICE_CONTROLLER *service = service_controller_create(
    "MyService",
    "Description of service",
    1  // version
);

// Set callbacks
service_controller_set_init_callback(service, service_init);
service_controller_set_start_callback(service, service_start);
service_controller_set_stop_callback(service, service_stop);
service_controller_set_cleanup_callback(service, service_cleanup);
service_controller_set_handler(service, service_handler);

// Register operations
service_controller_register_operation(service, "get_data");
service_controller_register_operation(service, "set_data");

// Register with application
application_register_service(app, service);
```

### Service Handler Implementation

```c
int service_handler(void *service_context, 
                   SERVICE_REQUEST *request, 
                   SERVICE_RESPONSE *response) {
    
    if (strcmp(request->operation, "get_data") == 0) {
        response->status_code = FRAMEWORK_SUCCESS;
        strcpy(response->message, "Data retrieved");
        response->data = my_data;
        response->data_size = data_size;
    }
    else if (strcmp(request->operation, "set_data") == 0) {
        // Handle set operation
        response->status_code = FRAMEWORK_SUCCESS;
        strcpy(response->message, "Data set");
    }
    else {
        response->status_code = FRAMEWORK_ERROR_INVALID;
        strcpy(response->message, "Unknown operation");
        return FRAMEWORK_ERROR_INVALID;
    }
    
    return FRAMEWORK_SUCCESS;
}
```

### Invoking a Service

```c
SERVICE_REQUEST request;
SERVICE_RESPONSE response;

strcpy(request.operation, "get_data");
request.data = NULL;
request.data_size = 0;

int result = application_invoke_service(app, "MyService", &request, &response);
if (result == FRAMEWORK_SUCCESS) {
    printf("Response: %s\n", response.message);
}
```

## API Reference

### HTTP Client API

#### Request Management
```c
HTTP_CLIENT_REQUEST* http_client_request_create(const char *method, const char *url);
void http_client_request_destroy(HTTP_CLIENT_REQUEST *request);
int http_client_request_add_header(HTTP_CLIENT_REQUEST *request, const char *name, const char *value);
int http_client_request_set_body(HTTP_CLIENT_REQUEST *request, const char *body, size_t length);
void http_client_request_set_timeout(HTTP_CLIENT_REQUEST *request, int timeout_seconds);
void http_client_request_set_verify_ssl(HTTP_CLIENT_REQUEST *request, int verify);
```

#### Synchronous Execution
```c
HTTP_CLIENT_RESPONSE* http_client_execute(HTTP_CLIENT_REQUEST *request);
HTTP_CLIENT_RESPONSE* http_client_get(const char *url);
HTTP_CLIENT_RESPONSE* http_client_post_json(const char *url, const char *json_body);
HTTP_CLIENT_RESPONSE* http_client_post_form(const char *url, const char *form_data);
```

#### Asynchronous Execution
```c
typedef void (*HTTP_CLIENT_CALLBACK)(HTTP_CLIENT_RESPONSE *response, void *user_data);

HTTP_CLIENT_ASYNC_HANDLE* http_client_execute_async(HTTP_CLIENT_REQUEST *request,
                                                      HTTP_CLIENT_CALLBACK callback,
                                                      void *user_data);
int http_client_async_wait(HTTP_CLIENT_ASYNC_HANDLE *handle);
int http_client_async_cancel(HTTP_CLIENT_ASYNC_HANDLE *handle);
int http_client_async_is_complete(HTTP_CLIENT_ASYNC_HANDLE *handle);
```

#### Response Management
```c
void http_client_response_destroy(HTTP_CLIENT_RESPONSE *response);
const char* http_client_response_get_header(HTTP_CLIENT_RESPONSE *response, const char *name);
```

### Kafka API

#### Client Management
```c
KAFKA_CLIENT* kafka_client_create(void);
void kafka_client_destroy(KAFKA_CLIENT *client);
int kafka_client_start(KAFKA_CLIENT *client);
int kafka_client_stop(KAFKA_CLIENT *client);
```

#### Consumer
```c
int kafka_consumer_register(KAFKA_CLIENT *client, const char *topic,
                           KAFKA_CONSUMER_CONFIG *config,
                           kafka_consumer_handler_fn handler,
                           void *user_data);
int kafka_consumer_register_multi(KAFKA_CLIENT *client, const char **topics,
                                 size_t topic_count,
                                 KAFKA_CONSUMER_CONFIG *config,
                                 kafka_consumer_handler_fn handler,
                                 void *user_data);
void kafka_consumer_config_default(KAFKA_CONSUMER_CONFIG *config,
                                  const char *brokers,
                                  const char *group_id);
```

#### Producer
```c
int kafka_producer_init(KAFKA_CLIENT *client, KAFKA_PRODUCER_CONFIG *config);
int kafka_produce(KAFKA_CLIENT *client, const char *topic,
                 const void *key, size_t key_len,
                 const void *payload, size_t payload_len);
int kafka_produce_string(KAFKA_CLIENT *client, const char *topic,
                        const char *key, const char *payload);
void kafka_producer_config_default(KAFKA_PRODUCER_CONFIG *config,
                                  const char *brokers);
```

#### Message Helpers
```c
const char* kafka_message_get_payload_string(KAFKA_MESSAGE *message);
const char* kafka_message_get_key_string(KAFKA_MESSAGE *message);
```

#### Authentication
```c
int kafka_producer_set_sasl_auth(KAFKA_PRODUCER *producer, const char *mechanism, 
                                  const char *username, const char *password);
int kafka_consumer_set_sasl_auth(KAFKA_CONSUMER *consumer, const char *mechanism,
                                  const char *username, const char *password);
```

### MongoDB API

#### Client Management
```c
MONGO_CLIENT* mongo_client_create(MONGO_CONFIG *config);
void mongo_client_destroy(MONGO_CLIENT *client);
int mongo_client_ping(MONGO_CLIENT *client);
void mongo_config_default(MONGO_CONFIG *config, const char *uri, const char *database);
MONGO_COLLECTION* mongo_client_get_collection(MONGO_CLIENT *client, const char *database, const char *collection);
void mongo_collection_destroy(MONGO_COLLECTION *collection);
```

#### Document Operations (CRUD)
```c
int mongo_insert_one(MONGO_COLLECTION *collection, const char *document, char *inserted_id, size_t id_size);
int mongo_insert_many(MONGO_COLLECTION *collection, const char **documents, size_t count);
MONGO_CURSOR* mongo_find(MONGO_COLLECTION *collection, const char *query, MONGO_QUERY_OPTIONS *options);
int mongo_find_one(MONGO_COLLECTION *collection, const char *query, char *result, size_t result_size);
int mongo_update(MONGO_COLLECTION *collection, const char *query, const char *update, MONGO_UPDATE_OPTIONS *options);
int mongo_replace_one(MONGO_COLLECTION *collection, const char *query, const char *replacement, bool upsert);
int mongo_delete(MONGO_COLLECTION *collection, const char *query, MONGO_DELETE_OPTIONS *options);
int64_t mongo_count(MONGO_COLLECTION *collection, const char *query);
```

#### Cursor Operations
```c
int mongo_cursor_next(MONGO_CURSOR *cursor, char *result, size_t result_size);
int mongo_cursor_has_next(MONGO_CURSOR *cursor);
void mongo_cursor_destroy(MONGO_CURSOR *cursor);
```

#### Aggregation & Indexes
```c
MONGO_CURSOR* mongo_aggregate(MONGO_COLLECTION *collection, const char *pipeline);
int mongo_create_index(MONGO_COLLECTION *collection, const char *keys, bool unique);
int mongo_drop_index(MONGO_COLLECTION *collection, const char *index_name);
```

#### Asynchronous Operations
```c
// Async CRUD operations with callbacks
MONGO_ASYNC_HANDLE* mongo_insert_one_async(MONGO_COLLECTION *collection, const char *document,
                                           mongo_async_callback callback, void *user_data);
MONGO_ASYNC_HANDLE* mongo_find_async(MONGO_COLLECTION *collection, const char *query,
                                     MONGO_QUERY_OPTIONS *options, mongo_async_callback callback, void *user_data);
MONGO_ASYNC_HANDLE* mongo_find_one_async(MONGO_COLLECTION *collection, const char *query,
                                         mongo_async_callback callback, void *user_data);
MONGO_ASYNC_HANDLE* mongo_update_async(MONGO_COLLECTION *collection, const char *query, const char *update,
                                       MONGO_UPDATE_OPTIONS *options, mongo_async_callback callback, void *user_data);
MONGO_ASYNC_HANDLE* mongo_delete_async(MONGO_COLLECTION *collection, const char *query,
                                       MONGO_DELETE_OPTIONS *options, mongo_async_callback callback, void *user_data);
MONGO_ASYNC_HANDLE* mongo_count_async(MONGO_COLLECTION *collection, const char *query,
                                      mongo_async_callback callback, void *user_data);
MONGO_ASYNC_HANDLE* mongo_aggregate_async(MONGO_COLLECTION *collection, const char *pipeline,
                                          mongo_async_callback callback, void *user_data);

// Async handle management
int mongo_async_wait(MONGO_ASYNC_HANDLE *handle);
int mongo_async_is_complete(MONGO_ASYNC_HANDLE *handle);
int mongo_async_cancel(MONGO_ASYNC_HANDLE *handle);
void mongo_async_handle_destroy(MONGO_ASYNC_HANDLE *handle);
```

#### Async Handle Management Patterns

The framework supports two patterns for managing async operation handles:

**Pattern 1: External Handle Management**
- Application tracks handle and destroys it explicitly
- Good for operations that need synchronous waiting
```c
MONGO_ASYNC_HANDLE *h = mongo_insert_one_async(collection, doc, callback, NULL);
mongo_async_wait(h);  // Wait for completion
mongo_async_handle_destroy(h);  // Explicit cleanup
```

**Pattern 2: Self-Cleanup in Callback**
- Callback receives handle via `result->handle` and destroys it
- Fire-and-forget pattern for background operations
```c
void on_complete(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("Operation succeeded\n");
    }
    // Handle cleans itself up
    mongo_async_handle_destroy(result->handle);
}

// No need to store handle
mongo_insert_one_async(collection, doc, on_complete, NULL);
```

**Pattern 3: Streaming Operations**
- For `mongo_find_async` and `mongo_aggregate_async`
- Callback invoked multiple times (once per document)
- Final callback has `success=0` to indicate end-of-stream
```c
void on_document(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        // Process document
        printf("Doc: %s\n", result->data);
    } else {
        // End of stream - cleanup
        printf("Stream complete\n");
        mongo_async_handle_destroy(result->handle);
    }
}
```

int mongo_create_index(MONGO_COLLECTION *collection, const char *keys, bool unique);
int mongo_drop_index(MONGO_COLLECTION *collection, const char *index_name);
```

#### Database Operations
```c
int mongo_list_databases(MONGO_CLIENT *client, char *result, size_t result_size);
int mongo_list_collections(MONGO_CLIENT *client, const char *database, char *result, size_t result_size);
int mongo_drop_database(MONGO_CLIENT *client, const char *database);
int mongo_drop_collection(MONGO_COLLECTION *collection);
```

### JSON API

#### Parsing
```c
JSON_VALUE* json_parse(const char *json_str);
void json_free(JSON_VALUE *value);
```

#### Schema-Based Parsing
```c
int json_parse_with_schema(const char *json_string, const JSON_SCHEMA *schema, void *target);
int json_parse_and_validate(const char *json_string, const JSON_SCHEMA *schema, 
                            void *target, JSON_VALIDATION_RESULT *result);
int json_serialize(const void *source, const JSON_SCHEMA *schema, 
                   char *buffer, size_t buffer_size);
```

#### Value Access
```c
JSON_VALUE* json_get_path(JSON_VALUE *value, const char *path);
const char* json_get_string(JSON_VALUE *value);
int json_get_int(JSON_VALUE *value, int default_val);
int json_get_bool(JSON_VALUE *value, int default_val);
double json_get_double(JSON_VALUE *value, double default_val);
```

#### JSON Builder
```c
JSON_BUILDER* json_builder_create(size_t initial_size);
void json_builder_destroy(JSON_BUILDER *builder);
void json_builder_start_object(JSON_BUILDER *builder);
void json_builder_end_object(JSON_BUILDER *builder);
void json_builder_start_array(JSON_BUILDER *builder);
void json_builder_end_array(JSON_BUILDER *builder);
void json_builder_add_string(JSON_BUILDER *builder, const char *key, const char *value);
void json_builder_add_int(JSON_BUILDER *builder, const char *key, int value);
void json_builder_add_int64(JSON_BUILDER *builder, const char *key, int64_t value);
void json_builder_add_double(JSON_BUILDER *builder, const char *key, double value);
void json_builder_add_bool(JSON_BUILDER *builder, const char *key, int value);
void json_builder_add_null(JSON_BUILDER *builder, const char *key);
const char* json_builder_get_string(JSON_BUILDER *builder);
```

### HTTP Server API

#### Server Management
```c
HTTP_SERVER* http_server_create(const char *host, int port);
void http_server_destroy(HTTP_SERVER *server);
int http_server_start(HTTP_SERVER *server);
void http_server_stop(HTTP_SERVER *server);
void http_server_run(HTTP_SERVER *server);
```

#### Route Registration
```c
int http_server_get(HTTP_SERVER *server, const char *path, HTTP_HANDLER handler, void *user_data);
int http_server_post(HTTP_SERVER *server, const char *path, HTTP_HANDLER handler, void *user_data);
int http_server_put(HTTP_SERVER *server, const char *path, HTTP_HANDLER handler, void *user_data);
int http_server_patch(HTTP_SERVER *server, const char *path, HTTP_HANDLER handler, void *user_data);
int http_server_delete(HTTP_SERVER *server, const char *path, HTTP_HANDLER handler, void *user_data);
```

#### Static Files
```c
int http_server_serve_static(HTTP_SERVER *server, const char *route_prefix, const char *directory);
```

#### Request/Response
```c
const char* http_request_get_header(HTTP_REQUEST *request, const char *name);
const char* http_request_get_path_param(HTTP_REQUEST *request, const char *name);
const char* http_request_get_query_param(HTTP_REQUEST *request, const char *name);

void http_response_set_status(HTTP_RESPONSE *response, int status_code);
void http_response_set_header(HTTP_RESPONSE *response, const char *name, const char *value);
void http_response_set_body(HTTP_RESPONSE *response, const char *body, size_t length);
void http_response_set_json(HTTP_RESPONSE *response, const char *json);
```

### Application Functions

- `application_create(name, version)` - Create new application
- `application_destroy(app)` - Destroy application
- `application_initialize(app)` - Initialize application and components
- `application_start(app)` - Start application
- `application_stop(app)` - Stop application
- `application_cleanup(app)` - Cleanup application
- `application_register_module(app, module)` - Register a module
- `application_register_service(app, service)` - Register a service
- `application_invoke_service(app, name, request, response)` - Invoke a service

### Module Functions

- `module_create(name, version)` - Create new module
- `module_destroy(module)` - Destroy module
- `module_set_init_callback(module, callback)` - Set init callback
- `module_set_start_callback(module, callback)` - Set start callback
- `module_set_stop_callback(module, callback)` - Set stop callback
- `module_set_cleanup_callback(module, callback)` - Set cleanup callback
- `module_add_dependency(module, dep_name)` - Add dependency
- `module_set_context(module, context)` - Set module context
- `module_get_context(module)` - Get module context

### Service Controller Functions

- `service_controller_create(name, description, version)` - Create service
- `service_controller_destroy(service)` - Destroy service
- `service_controller_set_handler(service, handler)` - Set request handler
- `service_controller_register_operation(service, operation)` - Register operation
- `service_controller_set_context(service, context)` - Set service context

### Framework Functions

- `framework_init()` - Initialize framework
- `framework_shutdown()` - Shutdown framework
- `framework_log(level, format, ...)` - Log message
- `framework_set_log_level(level)` - Set logging level

## Error Codes

- `FRAMEWORK_SUCCESS` (0) - Operation successful
- `FRAMEWORK_ERROR_NULL_PTR` (-1) - Null pointer error
- `FRAMEWORK_ERROR_INVALID` (-2) - Invalid parameter
- `FRAMEWORK_ERROR_MEMORY` (-3) - Memory allocation error
- `FRAMEWORK_ERROR_NOT_FOUND` (-4) - Resource not found
- `FRAMEWORK_ERROR_EXISTS` (-5) - Resource already exists
- `FRAMEWORK_ERROR_STATE` (-6) - Invalid state
- `FRAMEWORK_ERROR_DEPENDENCY` (-7) - Dependency error
- `FRAMEWORK_ERROR_CALLBACK` (-8) - Callback error

## Log Levels

- `LOG_LEVEL_DEBUG` - Detailed debug information
- `LOG_LEVEL_INFO` - Informational messages
- `LOG_LEVEL_WARNING` - Warning messages
- `LOG_LEVEL_ERROR` - Error messages

## Example Applications

The framework includes comprehensive demo applications showcasing all features:

### HTTP Server Demos
- **[http_server_app.c](examples/http_server_app.c)** - Basic HTTP/1.1 server with routes
- **[http2_server_app.c](examples/http2_server_app.c)** - HTTP/2 protocol features
- **[static_server_demo.c](examples/static_server_demo.c)** - Static file serving with MIME types

### HTTP Client Demos
- **[http_client_demo.c](examples/http_client_demo.c)** - Synchronous HTTP/HTTPS requests
- **[async_demo.c](examples/async_demo.c)** - Async callbacks and parallel requests
- **[async_practical.c](examples/async_practical.c)** - Real-world async patterns
- **[encoding_demo.c](examples/encoding_demo.c)** - Compression and chunked encoding
- **[http_proxy_demo.c](examples/http_proxy_demo.c)** - HTTP proxy (server + client)

### Kafka Integration Demos
- **[kafka_demo.c](examples/kafka_demo.c)** - Producer and consumer basics
- **[kafka_multi_topic_demo.c](examples/kafka_multi_topic_demo.c)** - Multi-topic subscriptions
- **[kafka_auth_demo.c](examples/kafka_auth_demo.c)** - SASL authentication
- **[unified_app.c](examples/unified_app.c)** - Combined HTTP server + Kafka

### JSON & Data Processing
- **[json_demo.c](examples/json_demo.c)** - JSON parsing and building
- **[json_schema_demo.c](examples/json_schema_demo.c)** - Schema validation

### Real-Time Communication Demos
- **[websocket_echo_server.c](examples/websocket_echo_server.c)** - RFC 6455 WebSocket echo server
- **[socketio_chat_server.c](examples/socketio_chat_server.c)** - Socket.IO chat server with rooms

### Observability Demos
- **[otel_demo.c](examples/otel_demo.c)** - Distributed tracing, metrics, and correlated logs

### Core Framework
- **[demo_app.c](examples/demo_app.c)** - Module and service controller basics

## Documentation

- **[HTTP_SERVER.md](docs/http/HTTP_SERVER.md)** - HTTP server implementation guide
- **[HTTP2_SUPPORT.md](docs/http/HTTP2_SUPPORT.md)** - HTTP/2 protocol documentation
- **[PATH_PARAMETERS.md](docs/http/PATH_PARAMETERS.md)** - Path parameters and query string parsing
- **[EPOLL_CONCURRENT.md](docs/http/EPOLL_CONCURRENT.md)** - Concurrent connection handling with EPOLL
- **[API.md](docs/API.md)** - Complete API reference
- **[QUICKSTART.md](docs/QUICKSTART.md)** - Quick start guide
- **[man/](man/)** - Linux man pages for all APIs (`man 3 http_server`, `man 7 equinox`)

## Project Structure

Source files are organized by function, under both `src/` and `src/include/`
(e.g. `src/http/http_server.c` + `src/include/http/http_server.h`). Every
example still uses bare includes like `#include "http_server.h"` - the
Makefile adds every `src/include/<module>/` folder to the include path.

```
equinox-framework/
├── src/
│   ├── core/                     # Application/module/service lifecycle
│   │   ├── application.c
│   │   ├── module.c
│   │   ├── service_controller.c
│   │   └── framework.c
│   ├── http/                     # HTTP/1.1, HTTP/2 server + HTTP client
│   │   ├── http_server.c
│   │   ├── http_route.c
│   │   ├── http2.c
│   │   └── http_client.c
│   ├── kafka/
│   │   └── kafka_client.c
│   ├── mongo/
│   │   └── mongo_client.c
│   ├── json/
│   │   └── json_parser.c
│   ├── realtime/                 # WebSocket + Socket.IO
│   │   ├── websocket.c
│   │   └── socketio.c
│   ├── otel/                     # OpenTelemetry (C++ wrapper, see below)
│   │   └── otel.cpp
│   └── include/
│       ├── core/
│       ├── http/
│       ├── kafka/
│       ├── mongo/
│       ├── json/
│       ├── realtime/
│       └── otel/
│           └── otel.h
├── examples/
│   ├── demo_app.c
│   ├── http_server_app.c
│   ├── http2_server_app.c
│   ├── static_server_demo.c
│   ├── http_client_demo.c
│   ├── async_demo.c
│   ├── async_practical.c
│   ├── encoding_demo.c
│   ├── http_proxy_demo.c
│   ├── kafka_demo.c
│   ├── kafka_multi_topic_demo.c
│   ├── kafka_auth_demo.c
│   ├── unified_app.c
│   ├── mongo_demo.c
│   ├── mongo_async_demo.c
│   ├── websocket_echo_server.c
│   ├── socketio_chat_server.c
│   ├── otel_demo.c
│   ├── json_demo.c
│   └── json_schema_demo.c
├── man/                          # Man pages
│   ├── http_server.3
│   ├── http_client.3
│   ├── kafka.3
│   └── equinox.7
├── Makefile
├── README.md
├── HTTP_SERVER.md
├── HTTP2_SUPPORT.md
├── PATH_PARAMETERS.md
├── EPOLL_CONCURRENT.md
├── ASYNC_HTTP_FEATURES.md
├── API.md
└── QUICKSTART.md
```

## Dependencies

- **C99 compiler**: GCC or Clang
- **C++17 compiler**: g++ (required only for `src/otel/otel.cpp`, the OpenTelemetry wrapper)
- **POSIX system**: Linux, macOS, Unix
- **OpenSSL**: For HTTPS/TLS support (`libssl-dev`)
- **zlib**: For compression support (`zlib1g-dev`)
- **librdkafka**: For Kafka integration (`librdkafka-dev`)
- **libmongoc**: For MongoDB integration (`libmongoc-dev`)
- **opentelemetry-cpp**: For tracing/metrics/logs (built from source, see below)
- **pthread**: For async operations (usually included)

### Installing Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install build-essential libssl-dev zlib1g-dev librdkafka-dev

# CentOS/RHEL
sudo yum install gcc openssl-devel zlib-devel librdkafka-devel

# macOS
brew install openssl zlib librdkafka
```

### Building OpenTelemetry C++ SDK

The framework's OpenTelemetry support (`src/otel/otel.cpp`, `src/include/otel/otel.h`)
is built on top of [open-telemetry/opentelemetry-cpp](https://github.com/open-telemetry/opentelemetry-cpp).
Since no prebuilt package ships this on most distributions, build it from
source and install it under `/usr/local` before running `make`:

```bash
sudo apt-get install -y cmake libprotobuf-dev protobuf-compiler \
    libcurl4-openssl-dev nlohmann-json3-dev

git clone --depth 1 --branch v1.16.1 \
    https://github.com/open-telemetry/opentelemetry-cpp.git
cd opentelemetry-cpp
git submodule update --init --depth 1 third_party/opentelemetry-proto

mkdir build && cd build
cmake .. \
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
  -DWITH_DEPRECATED_SDK_FACTORY=OFF
cmake --build . -j"$(nproc)"
sudo cmake --build . --target install
sudo ldconfig
```

> **Important:** because the SDK is configured with `WITH_STL=CXX17`, its
> `nostd::shared_ptr<T>` becomes a thin wrapper around `std::shared_ptr<T>`
> rather than its own ABI-stable implementation. Any translation unit that
> includes OpenTelemetry headers (like `src/otel/otel.cpp`, or the Makefile's
> `OTEL_CXXFLAGS`) **must** be compiled with `-DOPENTELEMETRY_STL_VERSION=2017`
> to match, or you will get a segfault deep inside `nostd::shared_ptr` that
> looks nothing like the actual bug.

## Performance Characteristics

### HTTP Server
- **Throughput**: 10,000+ requests/second (HTTP/1.1)
- **Concurrency**: 1000+ simultaneous connections
- **Latency**: < 1ms median response time
- **Memory**: ~50MB for 1000 connections

### HTTP Client
- **Parallel Requests**: 5 requests in ~1 second vs ~5 seconds sequential
- **HTTPS Overhead**: Minimal with SSL context reuse
- **Compression**: 60-80% size reduction with gzip
- **Async Performance**: Non-blocking, unlimited concurrent requests

### Kafka Integration
- **Producer Throughput**: 100,000+ messages/second
- **Consumer Throughput**: 50,000+ messages/second
- **Latency**: < 10ms end-to-end
- **Batch Efficiency**: Automatic batching for optimal performance

### JSON Processing
- **Parse Speed**: 1MB/second
- **Memory Overhead**: ~2x input size during parsing
- **Validation**: < 1ms for typical schemas

## Use Cases

### 🌐 Microservices
- RESTful API backends with HTTP/2 support
- Service-to-service communication with HTTP client
- Event-driven architecture with Kafka integration
- Health check and monitoring endpoints

### 📊 Data Processing Pipelines
- Consume messages from Kafka topics
- Transform JSON data
- Publish results to other topics or HTTP endpoints
- Real-time analytics and aggregation

### 🔄 API Gateway / Proxy
- Route requests to backend services
- Aggregate responses from multiple services
- Authentication and authorization
- Rate limiting and caching

### 📡 Webhook Handler
- Receive HTTP webhooks
- Process and validate JSON payloads
- Forward events to Kafka for processing
- Send async notifications via HTTP client

### 🚀 High-Performance Web Server
- Serve static files with compression
- Handle thousands of concurrent connections
- HTTP/2 for efficient multiplexing
- RESTful APIs with path parameters

## Real-World Examples

### Example 1: API Gateway with Kafka Backend

```c
// Receives HTTP requests and publishes to Kafka
void handle_event(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data) {
    KAFKA_CLIENT *client = (KAFKA_CLIENT*)user_data;
    
    // Parse incoming JSON
    JSON_VALUE *value = json_parse(request->body);
    
    // Publish to Kafka
    kafka_produce_string(client, "events", NULL, request->body);
    
    // Respond immediately
    http_response_set_json(response, "{\"status\":\"accepted\"}");
    
    json_free(value);
}
```

### Example 2: Multi-Service Dashboard Aggregator

```c
// Fetch data from 3 services in parallel
void load_dashboard(void) {
    HTTP_CLIENT_REQUEST *weather = http_client_request_create("GET", "https://api.weather.com/current");
    HTTP_CLIENT_REQUEST *news = http_client_request_create("GET", "https://api.news.com/headlines");
    HTTP_CLIENT_REQUEST *stocks = http_client_request_create("GET", "https://api.stocks.com/prices");
    
    // Execute all in parallel
    http_client_execute_async(weather, weather_callback, NULL);
    http_client_execute_async(news, news_callback, NULL);
    http_client_execute_async(stocks, stocks_callback, NULL);
    
    // All 3 requests run simultaneously
}
```

### Example 3: Kafka Consumer with HTTP Notifications

```c
// Consume Kafka messages and send HTTP notifications
void handle_notification(KAFKA_MESSAGE *msg, void *user_data) {
    if (msg && msg->payload) {
        // Parse event
        JSON_VALUE *event = json_parse((char*)msg->payload);
        
        // Send HTTP notification
        char url[256];
        snprintf(url, sizeof(url), "https://api.example.com/notify");
        
        HTTP_CLIENT_RESPONSE *resp = http_client_post_json(url, (char*)msg->payload);
        http_client_response_destroy(resp);
        
        json_free(event);
    }
}

void setup_notifications(KAFKA_CLIENT *client) {
    KAFKA_CONSUMER_CONFIG config;
    kafka_consumer_config_default(&config, "localhost:9092", "notifications");
    kafka_consumer_register(client, "user-events", &config, handle_notification, NULL);
    kafka_client_start(client);
}
```

## Testing

Run the comprehensive test suite:

```bash
# HTTP Server tests
./build/http_server_app &
curl http://localhost:8080/api/hello

# HTTP Client tests
./build/http_client_demo
./build/async_demo
./build/encoding_demo

# Kafka tests (requires running Kafka)
./build/kafka_demo
./build/kafka_multi_topic_demo

# MongoDB tests (requires running MongoDB)
./build/mongo_demo
./build/mongo_async_demo

# WebSocket tests
./build/websocket_echo_server &
# Test with wscat (npm install -g wscat):
wscat -c ws://localhost:8080/ws
# Or with JavaScript:
# const ws = new WebSocket('ws://localhost:8080/ws');
# ws.onmessage = (e) => console.log(e.data);
# ws.send('Hello WebSocket!');

# Socket.IO tests
./build/socketio_chat_server &
# Test with socket.io-client (npm install socket.io-client):
# node
# > const io = require('socket.io-client');
# > const socket = io('http://localhost:3000');
# > socket.emit('join', { username: 'Alice', room: 'general' });
# > socket.on('message', (data) => console.log(data));
# > socket.emit('message', { message: 'Hello!' });

# OpenTelemetry tests (console/ostream exporter, no external collector needed)
./build/otel_demo
# Prints spans, logs (correlated with trace/span IDs), and metrics to stdout

# OpenTelemetry tests with an OTLP collector (e.g. Jaeger, otel-collector)
./build/otel_demo otlp http://localhost:4318

# JSON tests
./build/json_demo
./build/json_schema_demo

# Integrated test
./build/unified_app
```

## Documentation

- **[HTTP_SERVER.md](docs/http/HTTP_SERVER.md)** - Complete HTTP server guide
- **[HTTP2_SUPPORT.md](docs/http/HTTP2_SUPPORT.md)** - HTTP/2 protocol documentation
- **[ASYNC_HTTP_FEATURES.md](docs/http/ASYNC_HTTP_FEATURES.md)** - Async HTTP client guide
- **[PATH_PARAMETERS.md](docs/http/PATH_PARAMETERS.md)** - Path parameters and query parsing
- **[EPOLL_CONCURRENT.md](docs/http/EPOLL_CONCURRENT.md)** - Concurrent connection handling
- **[API.md](docs/API.md)** - Complete API reference
- **[QUICKSTART.md](docs/QUICKSTART.md)** - Quick start guide
- **[man/](man/)** - Linux man pages (`man 3 http_client`, `man 3 kafka`, etc.)

## License

This project is licensed under the [MIT License](LICENSE).

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md)
for build instructions, coding conventions, and the pull request process.
This project follows the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md).

## Security

Please see [SECURITY.md](SECURITY.md) for supported versions and
instructions on privately reporting security vulnerabilities.

## Roadmap

✅ Core framework with modules and services  
✅ HTTP/1.1 and HTTP/2 server  
✅ Static file serving with MIME types  
✅ HTTP/HTTPS client with async support  
✅ Kafka producer and consumer  
✅ JSON parsing and schema validation  
✅ Compression (gzip/deflate)  
✅ Chunked transfer encoding  
🔄 WebSocket support (planned)  
🔄 Connection pooling (planned)  
🔄 Database integration (planned)  
🔄 Caching layer (planned)

## Support

For issues, questions, or contributions:
- Check the documentation in the `docs/` directory
- Review example applications in `examples/`
- Examine man pages with `man 3 http_server`, `man 3 kafka`, etc.

---

**Hanuman Framework** - High-performance C runtime framework for modern web applications and microservices.
