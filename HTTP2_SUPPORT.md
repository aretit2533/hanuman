# HTTP/2 Support in Hanuman Framework

**High-performance C runtime framework**

## Overview

The Hanuman Framework now supports both **HTTP/1.1** and **HTTP/2** protocols, allowing your applications to handle modern web traffic efficiently.

## Features

### HTTP/2 Protocol Implementation

✅ **Binary Protocol Support** - Full binary framing layer  
✅ **Connection Preface** - Automatic HTTP/2 connection detection  
✅ **Frame Parsing** - Support for all HTTP/2 frame types:
  - DATA frames
  - HEADERS frames
  - PRIORITY frames
  - RST_STREAM frames
  - SETTINGS frames
  - PUSH_PROMISE frames
  - PING frames
  - GOAWAY frames
  - WINDOW_UPDATE frames
  - CONTINUATION frames

✅ **Stream Management** - Multiple concurrent streams over single connection  
✅ **HPACK Compression** - Simplified header compression/decompression  
✅ **Flow Control** - Window-based flow control mechanism  
✅ **Error Handling** - Comprehensive HTTP/2 error code support

### Supported HTTP Methods

- **GET** - Retrieve resources
- **POST** - Create new resources
- **PUT** - Replace entire resources
- **PATCH** - Partial updates to resources *(NEW!)*
- **DELETE** - Remove resources
- **HEAD** - Get headers only
- **OPTIONS** - Query available methods

## Protocol Detection

The framework automatically detects whether a client is using HTTP/1.1 or HTTP/2:

```c
/* HTTP/2 Connection Preface Detection */
char peek_buffer[24];
ssize_t peek_bytes = recv(client_socket, peek_buffer, sizeof(peek_buffer), MSG_PEEK);

if (peek_bytes >= 24 && memcmp(peek_buffer, HTTP2_PREFACE, 24) == 0) {
    /* Handle as HTTP/2 connection */
    http2_handle_connection(client_socket);
} else {
    /* Handle as HTTP/1.1 connection */
    http_handle_request(client_socket);
}
```

The HTTP/2 connection preface is:
```
PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n
```

## Architecture

### HTTP/2 Components

```
src/include/http2.h      - HTTP/2 protocol structures and definitions
src/http2.c              - HTTP/2 implementation
src/http_server.c        - Unified server with protocol detection
```

### Key Structures

**HTTP2_FRAME_HEADER** - 9-byte frame header
```c
typedef struct {
    uint32_t length;        /* 24-bit payload length */
    uint8_t type;          /* Frame type */
    uint8_t flags;         /* Frame flags */
    uint32_t stream_id;    /* 31-bit stream identifier */
} HTTP2_FRAME_HEADER;
```

**HTTP2_STREAM** - Per-stream state management
```c
typedef struct {
    uint32_t stream_id;
    HTTP2_STREAM_STATE state;
    /* Request/response data */
    /* Flow control windows */
} HTTP2_STREAM;
```

**HTTP2_CONNECTION** - Connection-level state
```c
typedef struct {
    int socket;
    HTTP2_STREAM *streams;
    size_t stream_count;
    /* Settings and flow control */
} HTTP2_CONNECTION;
```

## Using HTTP/2 in Your Application

### Basic Server Setup

```c
#include "framework.h"
#include "http_server.h"

int main() {
    /* Initialize framework */
    framework_init();
    
    /* Create HTTP server - supports both HTTP/1.1 and HTTP/2 */
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    
    /* Register routes (work with both protocols) */
    http_server_get(server, "/", handle_root, NULL);
    http_server_post(server, "/api/data", handle_post, NULL);
    http_server_patch(server, "/api/users/1", handle_patch, NULL);
    
    /* Start server */
    http_server_start(server);
    http_server_run(server);
    
    /* Cleanup */
    http_server_destroy(server);
    framework_shutdown();
    
    return 0;
}
```

### Route Handlers

Route handlers remain the same for both HTTP/1.1 and HTTP/2:

```c
void handle_patch(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data) {
    /* PATCH allows partial updates */
    const char *json = 
        "{"
        "\"success\": true,"
        "\"method\": \"PATCH\","
        "\"note\": \"Partial update applied\""
        "}";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}
```

The framework automatically:
- Detects the protocol (HTTP/1.1 or HTTP/2)
- Parses the request into unified `HTTP_REQUEST` structure
- Routes to the appropriate handler
- Converts `HTTP_RESPONSE` to the correct protocol format
- Sends the response back to the client

## Testing HTTP/2

### Using curl

Test with HTTP/2:
```bash
# Requires curl with HTTP/2 support
curl --http2 http://localhost:8080/api/status

# HTTP/2 with prior knowledge (no upgrade)
curl --http2-prior-knowledge http://localhost:8080/api/status
```

Test PATCH method:
```bash
curl -X PATCH \
  -H "Content-Type: application/json" \
  -d '{"name":"updated"}' \
  http://localhost:8080/api/users/1
```

### Using nghttp

```bash
# Install nghttp2 tools
sudo apt-get install nghttp2-client  # Ubuntu/Debian
brew install nghttp2                  # macOS

# Test with nghttp
nghttp -v http://localhost:8080/
```

### Building and Running

```bash
# Build framework
make debug

# Run HTTP/2 demo server
make run-http2

# Or run directly
./build/http2_server_app
```

## Performance Benefits

HTTP/2 provides several advantages:

1. **Multiplexing** - Multiple requests over single connection (no head-of-line blocking)
2. **Header Compression** - HPACK reduces overhead
3. **Binary Protocol** - More efficient parsing than text-based HTTP/1.1
4. **Server Push** - Proactively send resources (future feature)
5. **Stream Prioritization** - Control resource loading order (future feature)

## Current Limitations

The current HTTP/2 implementation includes:

- ✅ Full frame parsing and generation
- ✅ Connection and stream management
- ✅ Basic HPACK compression
- ✅ Settings and flow control
- ⏳ Server push (planned)
- ⏳ Stream prioritization (planned)
- ⏳ TLS/ALPN negotiation (planned)

## Troubleshooting

### Connection not detected as HTTP/2

Most browsers require HTTPS for HTTP/2. For testing:
- Use `curl --http2-prior-knowledge` to force HTTP/2 over cleartext
- Or use `nghttp` which supports cleartext HTTP/2

### Build errors

Ensure you have:
```bash
gcc --version  # GCC 4.9+ recommended
make clean && make debug
```

### Check server protocol support

```bash
curl -v http://localhost:8080/ 2>&1 | grep -i server
# Should show: Server: Equinox/1.0 (HTTP/1.1, HTTP/2)
```

## Next Steps

- See [HTTP_SERVER.md](HTTP_SERVER.md) for HTTP server basics
- See [API.md](API.md) for complete API reference
- See [examples/http2_server_app.c](examples/http2_server_app.c) for working example

## References

- [RFC 7540 - HTTP/2 Specification](https://tools.ietf.org/html/rfc7540)
- [RFC 7541 - HPACK Header Compression](https://tools.ietf.org/html/rfc7541)
- [HTTP/2 Explained](https://http2-explained.haxx.se/)
