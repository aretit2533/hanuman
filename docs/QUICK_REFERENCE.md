# Equinox Framework - Quick Reference Card

## HTTP/2 & PATCH Support

### Build & Run
```bash
make debug                    # Build everything
make run-http2               # Run HTTP/2 demo server
./test_http2.sh              # Run test suite
```

### Server Setup
```c
#include "framework.h"
#include "http_server.h"

// Create server (supports both HTTP/1.1 and HTTP/2 automatically)
HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);

// Register routes (all HTTP methods)
http_server_get(server, "/", handle_root, NULL);
http_server_post(server, "/api/users", handle_create, NULL);
http_server_put(server, "/api/users/:id", handle_replace, NULL);
http_server_patch(server, "/api/users/:id", handle_update, NULL);  // NEW!
http_server_delete(server, "/api/users/:id", handle_delete, NULL);

// Start server
http_server_start(server);
http_server_run(server);  // Blocking
```

### Route Handlers
```c
// PATCH handler (partial update)
void handle_update(HTTP_REQUEST *req, HTTP_RESPONSE *res, void *ctx) {
    (void)ctx;
    
    // Parse partial data from req->body
    // Update only specified fields
    
    http_response_set_status(res, HTTP_STATUS_OK);
    http_response_set_json(res, "{\"updated\":true}");
}

// PUT handler (full replacement)
void handle_replace(HTTP_REQUEST *req, HTTP_RESPONSE *res, void *ctx) {
    (void)ctx;
    
    // Parse complete object from req->body
    // Replace entire resource
    
    http_response_set_status(res, HTTP_STATUS_OK);
    http_response_set_json(res, "{\"replaced\":true}");
}
```

### HTTP Methods Cheatsheet

| Method | Purpose | Idempotent | Safe | Body |
|--------|---------|------------|------|------|
| GET | Retrieve resource | Yes | Yes | No |
| POST | Create resource | No | No | Yes |
| PUT | Replace resource | Yes | No | Yes |
| **PATCH** | **Partial update** | **No** | **No** | **Yes** |
| DELETE | Remove resource | Yes | No | No |
| HEAD | Get headers only | Yes | Yes | No |
| OPTIONS | Query methods | Yes | Yes | No |

### PUT vs PATCH

**PUT** - Full replacement:
```bash
# Must send complete object
curl -X PUT http://localhost:8080/api/users/1 \
  -H "Content-Type: application/json" \
  -d '{"name":"John","email":"john@example.com","role":"admin"}'
```

**PATCH** - Partial update:
```bash
# Send only fields to change
curl -X PATCH http://localhost:8080/api/users/1 \
  -H "Content-Type: application/json" \
  -d '{"email":"newemail@example.com"}'
```

### Testing HTTP/2

**HTTP/1.1 (default):**
```bash
curl http://localhost:8080/api/status
```

**HTTP/2 with curl:**
```bash
curl --http2-prior-knowledge http://localhost:8080/api/status
```

**HTTP/2 with nghttp:**
```bash
nghttp -v http://localhost:8080/
```

### Response Helpers
```c
// JSON response
http_response_set_json(res, "{\"key\":\"value\"}");

// HTML response
http_response_set_html(res, "<html>...</html>");

// Plain text
http_response_set_text(res, "Hello, World!");

// Custom status
http_response_set_status(res, HTTP_STATUS_CREATED);

// Add headers
http_response_add_header(res, "X-Custom", "Value");
```

### HTTP Status Codes
```c
HTTP_STATUS_OK                    // 200
HTTP_STATUS_CREATED               // 201
HTTP_STATUS_NO_CONTENT            // 204
HTTP_STATUS_BAD_REQUEST           // 400
HTTP_STATUS_UNAUTHORIZED          // 401
HTTP_STATUS_FORBIDDEN             // 403
HTTP_STATUS_NOT_FOUND             // 404
HTTP_STATUS_METHOD_NOT_ALLOWED    // 405
HTTP_STATUS_INTERNAL_SERVER_ERROR // 500
```

### Request Data
```c
void handler(HTTP_REQUEST *req, HTTP_RESPONSE *res, void *ctx) {
    // Method
    HTTP_METHOD method = req->method;  // GET, POST, PUT, PATCH, DELETE, etc.
    
    // Path
    const char *path = req->path;  // "/api/users/1"
    
    // Query parameters
    const char *name = http_request_get_query_param(req, "name");
    
    // Headers
    const char *content_type = http_request_get_header(req, "Content-Type");
    
    // Body
    const char *body = req->body;
    size_t body_len = req->body_length;
    
    // Build response
    http_response_set_status(res, HTTP_STATUS_OK);
    http_response_set_json(res, "{\"status\":\"success\"}");
}
```

### Protocol Detection (Automatic)
```
Client connects
     ↓
Server peeks first 24 bytes
     ↓
Starts with "PRI * HTTP/2.0..."? ──Yes──→ HTTP/2 handler
     ↓                                          ↓
     No                                  Parse HTTP/2 frames
     ↓                                          ↓
HTTP/1.1 handler                        Route to handler
     ↓                                          ↓
Parse HTTP/1.1 text                    Build HTTP/2 response
     ↓                                          ↓
Route to handler ←─────────────────────────────┘
     ↓
Build HTTP/1.1 response
```

### HTTP/2 Frame Types (Handled Automatically)
- **DATA**: Request/response bodies
- **HEADERS**: HTTP headers with HPACK compression
- **SETTINGS**: Connection configuration
- **WINDOW_UPDATE**: Flow control
- **GOAWAY**: Graceful shutdown
- **PING**: Keep-alive
- **RST_STREAM**: Stream cancellation
- **PRIORITY**: Stream prioritization
- **PUSH_PROMISE**: Server push (future)
- **CONTINUATION**: Header continuation

### Logging
```c
framework_set_log_level(LOG_LEVEL_DEBUG);   // Verbose
framework_set_log_level(LOG_LEVEL_INFO);    // Normal
framework_set_log_level(LOG_LEVEL_WARNING); // Warnings only
framework_set_log_level(LOG_LEVEL_ERROR);   // Errors only

// Logs automatically show:
// [2025-12-19 14:30:40] [INFO] Message
```

### Error Handling
```c
if (http_server_start(server) != FRAMEWORK_SUCCESS) {
    fprintf(stderr, "Failed to start server\n");
    return 1;
}
```

### Complete Example
```c
#include "framework.h"
#include "http_server.h"
#include <signal.h>

static HTTP_SERVER *g_server = NULL;

void shutdown_handler(int sig) {
    (void)sig;
    if (g_server) http_server_stop(g_server);
}

void handle_hello(HTTP_REQUEST *req, HTTP_RESPONSE *res, void *ctx) {
    (void)req; (void)ctx;
    http_response_set_json(res, "{\"message\":\"Hello HTTP/2!\"}");
}

void handle_patch(HTTP_REQUEST *req, HTTP_RESPONSE *res, void *ctx) {
    (void)ctx;
    if (!req->body) {
        http_response_set_status(res, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    http_response_set_json(res, "{\"patched\":true}");
}

int main() {
    framework_init();
    
    APPLICATION *app = application_create("MyApp", 1);
    g_server = http_server_create("0.0.0.0", 8080);
    
    signal(SIGINT, shutdown_handler);
    
    http_server_get(g_server, "/hello", handle_hello, NULL);
    http_server_patch(g_server, "/users/:id", handle_patch, NULL);
    
    application_set_http_server(app, g_server);
    application_initialize(app);
    application_start(app);
    
    http_server_start(g_server);
    http_server_run(g_server);
    
    application_stop(app);
    http_server_destroy(g_server);
    application_destroy(app);
    framework_shutdown();
    
    return 0;
}
```

### Compile & Link
```bash
gcc -Wall -Wextra -Werror -std=c99 -Isrc/include \
    myapp.c -Llib -lequinox -o myapp
```

### Documentation
- **README.md** - Project overview
- **HTTP_SERVER.md** - HTTP server guide
- **HTTP2_SUPPORT.md** - HTTP/2 detailed docs
- **API.md** - Complete API reference
- **QUICKSTART.md** - Getting started
- **IMPLEMENTATION_SUMMARY.md** - What's new

---
**Equinox Framework v1.0.0**  
HTTP/1.1 + HTTP/2 | GET, POST, PUT, PATCH, DELETE
