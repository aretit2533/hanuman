# Path Parameters and Query Strings

This document describes the path parameter and query string parsing features in the Hanuman Framework.

## Overview

The framework supports dynamic route matching with path parameters (similar to Express.js or Flask) and automatic query string parsing with URL decoding.

## Features

- **Path Parameters**: Extract values from URL paths using `:param` syntax
- **Query String Parsing**: Automatically parse `?key=value&key2=value2` into accessible parameters
- **URL Decoding**: Automatically decode URL-encoded characters (`%20`, `+`, etc.)
- **Multiple Parameters**: Support for multiple path and query parameters in a single route

## Path Parameters

### Defining Routes with Parameters

Use the `:` prefix to define a path parameter in your route pattern:

```c
http_server_add_route(server, HTTP_METHOD_GET, "/api/users/:id", user_handler, NULL);
http_server_add_route(server, HTTP_METHOD_GET, "/api/posts/:postId/comments/:commentId", comment_handler, NULL);
```

### Accessing Path Parameters

Use `http_request_get_path_param()` to retrieve parameter values:

```c
static void user_handler(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    const char *user_id = http_request_get_path_param(request, "id");
    
    if (user_id) {
        char body[256];
        snprintf(body, sizeof(body), "User ID: %s", user_id);
        http_response_set_text(response, body);
    }
}
```

### Example Requests

```bash
# Matches /api/users/:id
curl http://localhost:8080/api/users/123
# Extracts: id = "123"

# Matches /api/posts/:postId/comments/:commentId  
curl http://localhost:8080/api/posts/100/comments/5
# Extracts: postId = "100", commentId = "5"
```

## Query String Parameters

### Automatic Parsing

Query strings are automatically parsed when a request is received. No special route configuration is needed.

```c
http_server_add_route(server, HTTP_METHOD_GET, "/api/search", search_handler, NULL);
```

### Accessing Query Parameters

Use `http_request_get_query_param()` to retrieve query parameter values:

```c
static void search_handler(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    const char *query = http_request_get_query_param(request, "q");
    const char *page = http_request_get_query_param(request, "page");
    const char *limit = http_request_get_query_param(request, "limit");
    
    char body[512];
    snprintf(body, sizeof(body),
             "Search: %s\nPage: %s\nLimit: %s",
             query ? query : "none",
             page ? page : "1",
             limit ? limit : "10");
    
    http_response_set_text(response, body);
}
```

### Example Requests

```bash
# Single parameter
curl 'http://localhost:8080/api/search?q=test'
# Extracts: q = "test"

# Multiple parameters
curl 'http://localhost:8080/api/search?q=test&page=2&limit=10'
# Extracts: q = "test", page = "2", limit = "10"

# URL encoded spaces
curl 'http://localhost:8080/api/search?q=hello+world'
# Extracts: q = "hello world"

# URL encoded special characters
curl 'http://localhost:8080/api/search?email=user%40example.com'
# Extracts: email = "user@example.com"
```

## Combining Path and Query Parameters

Routes can use both path parameters and query strings together:

```c
static void user_handler(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    // Path parameter
    const char *user_id = http_request_get_path_param(request, "id");
    
    // Query parameters
    const char *format = http_request_get_query_param(request, "format");
    const char *include_details = http_request_get_query_param(request, "details");
    
    char body[512];
    snprintf(body, sizeof(body),
             "User ID: %s\nFormat: %s\nDetails: %s",
             user_id ? user_id : "unknown",
             format ? format : "default",
             include_details ? include_details : "false");
    
    http_response_set_text(response, body);
}
```

### Example Request

```bash
curl 'http://localhost:8080/api/users/456?format=json&details=true'
# Path parameter: id = "456"
# Query parameters: format = "json", details = "true"
```

## API Reference

### Path Parameter Functions

#### `const char* http_request_get_path_param(HTTP_REQUEST *request, const char *name)`

Retrieves a path parameter value by name.

**Parameters:**
- `request`: The HTTP request object
- `name`: The parameter name (without the `:` prefix)

**Returns:** The parameter value, or `NULL` if not found

**Example:**
```c
const char *id = http_request_get_path_param(request, "id");
```

#### `int http_request_add_path_param(HTTP_REQUEST *request, const char *name, const char *value)`

Adds a path parameter to the request (typically called internally during route matching).

**Parameters:**
- `request`: The HTTP request object
- `name`: The parameter name
- `value`: The parameter value

**Returns:** `FRAMEWORK_SUCCESS` on success, error code on failure

### Query Parameter Functions

#### `const char* http_request_get_query_param(HTTP_REQUEST *request, const char *name)`

Retrieves a query parameter value by name.

**Parameters:**
- `request`: The HTTP request object
- `name`: The parameter name

**Returns:** The parameter value (URL-decoded), or `NULL` if not found

**Example:**
```c
const char *page = http_request_get_query_param(request, "page");
```

#### `int http_request_add_query_param(HTTP_REQUEST *request, const char *name, const char *value)`

Adds a query parameter to the request (typically called internally during query string parsing).

**Parameters:**
- `request`: The HTTP request object
- `name`: The parameter name
- `value`: The parameter value

**Returns:** `FRAMEWORK_SUCCESS` on success, error code on failure

## URL Encoding Support

The framework automatically decodes the following URL encodings in query parameter values:

- **Percent encoding**: `%XX` where XX is a hexadecimal byte value
  - Example: `%20` → space, `%40` → `@`, `%21` → `!`
- **Plus encoding**: `+` → space
  - Example: `hello+world` → `hello world`

**Note:** URL encoding is only applied to query parameter values. Path parameters are not automatically decoded.

## Implementation Details

### Route Matching Algorithm

When a request is received:

1. The framework checks for exact path matches first (optimization)
2. If no exact match, it performs pattern matching with parameter extraction:
   - Path and pattern are split into segments
   - Each segment is compared
   - `:param` segments match any value
   - Literal segments must match exactly
   - Both paths must have the same number of segments

### Parameter Storage

- Path parameters: Stored in `HTTP_REQUEST.path_params` array (initial capacity: 10)
- Query parameters: Stored in `HTTP_REQUEST.query_params` array (initial capacity: 20)
- Arrays automatically resize when capacity is exceeded

### Performance Considerations

- Exact path matching is O(1) (string comparison)
- Pattern matching is O(n) where n is the number of segments
- Parameter lookup is O(m) where m is the number of parameters
- Query string parsing is O(k) where k is the length of the query string

## Complete Example

See [examples/param_demo.c](../examples/param_demo.c) for a complete working example demonstrating:

- Single path parameters (`/api/users/:id`)
- Multiple path parameters (`/api/posts/:postId/comments/:commentId`)
- Query string parsing (`/api/search?q=test&page=2`)
- Combined path and query parameters
- URL encoding/decoding

### Running the Demo

```bash
# Build the demo
make debug

# Run the parameter demo
make run-param

# Or run directly
./build/param_demo
```

### Testing the Demo

```bash
# Test path parameter
curl http://localhost:8080/api/users/123

# Test path + query parameters
curl 'http://localhost:8080/api/users/456?format=json&details=true'

# Test multiple path parameters
curl http://localhost:8080/api/posts/100/comments/5

# Test query string parsing
curl 'http://localhost:8080/api/search?q=test&page=2&limit=10'

# Test URL encoding
curl 'http://localhost:8080/api/search?q=hello+world'
```

## Limitations

- Maximum parameter name length: 127 characters
- Maximum parameter value length: 511 characters
- Maximum path length: 2047 characters
- Arrays grow dynamically but start with fixed capacity

## Future Enhancements

Potential future improvements:

- [ ] Regular expression patterns in path parameters
- [ ] Optional path segments
- [ ] Wildcard path matching (`*`)
- [ ] Parameter validation and type conversion
- [ ] Parameter middleware/preprocessing
- [ ] Query parameter arrays (`?id=1&id=2&id=3`)
