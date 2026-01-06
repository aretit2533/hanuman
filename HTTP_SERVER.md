# Equinox HTTP Server Framework

The Hanuman Framework has been enhanced with full HTTP server capabilities! You can now build web applications and REST APIs with ease.

## Features

✅ **HTTP/1.1 Server** - Full HTTP server implementation
✅ **Route Registration** - Register routes with handler functions
✅ **Multiple HTTP Methods** - GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
✅ **Request/Response Structures** - Easy-to-use HTTP abstractions
✅ **Automatic Response Building** - Framework constructs and sends responses
✅ **JSON Support** - Built-in JSON response helpers
✅ **Query String Parsing** - Automatic query parameter extraction
✅ **Header Management** - Add/read HTTP headers
✅ **Application Integration** - Seamless integration with existing framework

## Quick Start

### 1. Create an HTTP Server

```c
#include "framework.h"
#include "http_server.h"

int main() {
    // Initialize framework
    framework_init();
    
    // Create application
    APPLICATION *app = application_create("MyAPI", 1);
    
    // Create HTTP server
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    
    // Register routes (see below)
    // ...
    
    // Start server
    application_set_http_server(app, server);
    application_initialize(app);
    application_start(app);
    http_server_start(server);
    
    // Run server (blocking)
    http_server_run(server);
    
    // Cleanup
    http_server_destroy(server);
    application_destroy(app);
    framework_shutdown();
    
    return 0;
}
```

### 2. Register Routes

Routes are registered with handler functions that receive HTTP requests and construct responses:

```c
void my_handler(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    // Set status
    http_response_set_status(response, HTTP_STATUS_OK);
    
    // Set JSON body
    http_response_set_json(response, "{\"message\": \"Hello, World!\"}");
}

// Register route
http_server_get(server, "/api/hello", my_handler, NULL);
```

## Route Registration Examples

### GET Route

```c
void handle_users(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *json = 
        "{"
        "\"users\": ["
        "{\"id\": 1, \"name\": \"Alice\"},"
        "{\"id\": 2, \"name\": \"Bob\"}"
        "]"
        "}";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

http_server_get(server, "/api/users", handle_users, NULL);
```

### POST Route with Body

```c
void handle_create_user(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    // Check if body exists
    if (!request->body || request->body_length == 0) {
        http_response_set_status(response, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(response, "{\"error\": \"No body provided\"}");
        return;
    }
    
    // Process the body (request->body contains the data)
    // In real app: parse JSON, validate, save to database
    
    char json[256];
    snprintf(json, sizeof(json),
            "{\"success\": true, \"message\": \"User created\"}");
    
    http_response_set_status(response, HTTP_STATUS_CREATED);
    http_response_set_json(response, json);
}

http_server_post(server, "/api/users", handle_create_user, NULL);
```

### Query Parameters

```c
void handle_search(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    // Access query string: /api/search?q=test&limit=10
    // request->query_string contains "q=test&limit=10"
    
    char *query = request->query_string;
    
    char json[512];
    snprintf(json, sizeof(json),
            "{\"query\": \"%s\", \"results\": []}", query);
    
    http_response_set_json(response, json);
}

http_server_get(server, "/api/search", handle_search, NULL);
```

### Headers

```c
void handle_with_headers(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    // Read request header
    const char *auth = http_request_get_header(request, "Authorization");
    if (!auth) {
        http_response_set_status(response, HTTP_STATUS_UNAUTHORIZED);
        http_response_set_json(response, "{\"error\": \"No auth header\"}");
        return;
    }
    
    // Add custom response header
    http_response_add_header(response, "X-Custom-Header", "MyValue");
    
    http_response_set_json(response, "{\"authenticated\": true}");
}

http_server_get(server, "/api/protected", handle_with_headers, NULL);
```

### HTML Response

```c
void handle_html(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *html = 
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>My Page</title></head>"
        "<body><h1>Hello from Equinox!</h1></body>"
        "</html>";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/html");
    http_response_set_body(response, html, strlen(html));
}

http_server_get(server, "/", handle_html, NULL);
```

## HTTP Response Functions

### Set Status
```c
http_response_set_status(response, HTTP_STATUS_OK);          // 200
http_response_set_status(response, HTTP_STATUS_CREATED);     // 201
http_response_set_status(response, HTTP_STATUS_BAD_REQUEST); // 400
http_response_set_status(response, HTTP_STATUS_NOT_FOUND);   // 404
```

### Set Body
```c
// JSON (sets Content-Type: application/json)
http_response_set_json(response, "{\"key\": \"value\"}");

// Plain text (sets Content-Type: text/plain)
http_response_set_text(response, "Hello, World!");

// Raw body
http_response_set_body(response, data, length);
```

### Add Headers
```c
http_response_add_header(response, "Content-Type", "application/json");
http_response_add_header(response, "Cache-Control", "no-cache");
http_response_add_header(response, "X-API-Version", "1.0");
```

## HTTP Request Structure

```c
typedef struct _http_request_ {
    HTTP_METHOD method;          // GET, POST, PUT, DELETE, etc.
    char path[512];             // Request path: "/api/users"
    char query_string[512];     // Query params: "id=1&name=test"
    char http_version[16];      // "HTTP/1.1"
    
    HTTP_HEADER *headers;       // Array of headers
    size_t header_count;
    
    char *body;                 // Request body
    size_t body_length;
    
    void *user_data;           // Custom data
} HTTP_REQUEST;
```

## HTTP Response Structure

```c
typedef struct _http_response_ {
    HTTP_STATUS status;         // 200, 404, 500, etc.
    char status_message[128];   // "OK", "Not Found", etc.
    
    HTTP_HEADER *headers;       // Array of response headers
    size_t header_count;
    
    char *body;                 // Response body
    size_t body_length;
} HTTP_RESPONSE;
```

## Available HTTP Methods

```c
http_server_get(server, path, handler, user_data);
http_server_post(server, path, handler, user_data);
http_server_put(server, path, handler, user_data);
http_server_delete(server, path, handler, user_data);

// Or use generic:
http_server_add_route(server, HTTP_METHOD_GET, path, handler, user_data);
```

## HTTP Status Codes

```c
HTTP_STATUS_OK = 200
HTTP_STATUS_CREATED = 201
HTTP_STATUS_ACCEPTED = 202
HTTP_STATUS_NO_CONTENT = 204
HTTP_STATUS_BAD_REQUEST = 400
HTTP_STATUS_UNAUTHORIZED = 401
HTTP_STATUS_FORBIDDEN = 403
HTTP_STATUS_NOT_FOUND = 404
HTTP_STATUS_METHOD_NOT_ALLOWED = 405
HTTP_STATUS_INTERNAL_ERROR = 500
HTTP_STATUS_NOT_IMPLEMENTED = 501
HTTP_STATUS_SERVICE_UNAVAILABLE = 503
```

## Building and Running

```bash
# Build the framework
make debug

# Run the HTTP server example
make run-server

# Or run directly
./build/http_server_app
```

The server will start on `http://localhost:8080`

## Testing with curl

```bash
# Test GET request
curl http://localhost:8080/api/status

# Test POST request
curl -X POST http://localhost:8080/api/echo \
  -H "Content-Type: application/json" \
  -d '{"message": "Hello"}'

# Test with query parameters
curl "http://localhost:8080/api/hello?name=John"

# List users
curl http://localhost:8080/api/users

# Create user
curl -X POST http://localhost:8080/api/users \
  -H "Content-Type: application/json" \
  -d '{"name": "NewUser", "email": "user@example.com"}'
```

## Complete Example

See [examples/http_server_app.c](examples/http_server_app.c) for a complete working HTTP server with multiple routes demonstrating:

- GET routes with HTML and JSON responses
- POST routes with body parsing
- Query parameter handling
- Header management
- Status codes
- Error handling

## Architecture

The HTTP server integrates seamlessly with the Hanuman framework:

1. **HTTP Server** creates socket and listens for connections
2. **Routes** are registered with handler functions
3. **Requests** are parsed automatically from raw HTTP
4. **Handler functions** receive request and response structures
5. **Framework** constructs the HTTP response and sends it
6. **Application lifecycle** manages server start/stop

## Best Practices

1. **Always check request body** before accessing it
2. **Set appropriate status codes** for different scenarios
3. **Add Content-Type headers** for JSON/HTML responses
4. **Validate input** in POST/PUT handlers
5. **Use meaningful error messages** in error responses
6. **Log important events** for debugging
7. **Handle signals** for graceful shutdown

## Future Enhancements

- Path parameters: `/users/:id`
- Middleware support
- HTTPS/TLS support
- WebSocket support
- File upload handling
- Session management
- CORS support
- Rate limiting

---

**The framework automatically handles:**
- HTTP parsing
- Response construction
- Socket management
- Connection handling
- Memory management
- Error responses (404, 500, etc.)
