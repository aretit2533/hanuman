# Path Parameter and Query String Support - Implementation Summary

## Overview

Added comprehensive support for path parameters and query string parsing to the Equinox Framework HTTP server, enabling RESTful API design patterns similar to Express.js, Flask, and other modern web frameworks.

## Features Implemented

### 1. Path Parameters

Dynamic route matching using `:param` syntax:

```c
// Define routes with parameters
http_server_add_route(server, HTTP_METHOD_GET, "/api/users/:id", user_handler, NULL);
http_server_add_route(server, HTTP_METHOD_GET, "/api/posts/:postId/comments/:commentId", comment_handler, NULL);

// Access parameters in handler
const char *user_id = http_request_get_path_param(request, "id");
```

**Key Features:**
- Pattern-based route matching
- Multiple parameters per route
- Parameter extraction and storage
- Dynamic array allocation with auto-resize

### 2. Query String Parsing

Automatic parsing of URL query strings:

```c
// Query string: ?format=json&page=2&limit=10

const char *format = http_request_get_query_param(request, "format");  // "json"
const char *page = http_request_get_query_param(request, "page");      // "2"
const char *limit = http_request_get_query_param(request, "limit");    // "10"
```

**Key Features:**
- Automatic parsing on request reception
- URL decoding (percent encoding and plus encoding)
- Key-value pair extraction
- Dynamic storage

### 3. URL Encoding Support

Full URL encoding/decoding:

- **Percent encoding**: `%20` → space, `%40` → `@`, `%21` → `!`
- **Plus encoding**: `hello+world` → `hello world`

Example:
```bash
curl 'http://localhost:8080/api/search?q=hello+world&email=user%40example.com'
# Extracts: q = "hello world", email = "user@example.com"
```

## Code Changes

### Data Structures

**New Type: HTTP_PARAM** (http_server.h)
```c
typedef struct {
    char name[128];
    char value[512];
} HTTP_PARAM;
```

**Updated HTTP_REQUEST** (http_server.h)
```c
typedef struct {
    // ... existing fields ...
    
    // Path parameters
    HTTP_PARAM *path_params;
    size_t path_param_count;
    size_t path_param_capacity;
    
    // Query parameters
    HTTP_PARAM *query_params;
    size_t query_param_count;
    size_t query_param_capacity;
} HTTP_REQUEST;
```

### New Functions

**Path Parameter Functions** (http_server.c)
- `const char* http_request_get_path_param(HTTP_REQUEST *request, const char *name)`
- `int http_request_add_path_param(HTTP_REQUEST *request, const char *name, const char *value)`

**Query Parameter Functions** (http_server.c)
- `const char* http_request_get_query_param(HTTP_REQUEST *request, const char *name)`
- `int http_request_add_query_param(HTTP_REQUEST *request, const char *name, const char *value)`

**Route Matching** (http_route.c)
- `int http_route_extract_params(HTTP_ROUTE *route, const char *path, HTTP_REQUEST *request)`

**Helper Functions** (http_server.c)
- `static void url_decode(char *dst, const char *src, size_t dst_size)`
- `static void parse_query_string(HTTP_REQUEST *request)`

### Modified Functions

**http_route_matches()** (http_route.c)
- Rewrote pattern matching algorithm
- Added support for `:param` segments
- Segment-by-segment comparison
- Handles leading slashes correctly

**http_request_create()** (http_server.c)
- Allocates path_params array (capacity 10)
- Allocates query_params array (capacity 20)

**http_request_destroy()** (http_server.c)
- Frees path_params array
- Frees query_params array

**parse_http_request()** (http_server.c)
- Calls parse_query_string() after extracting query string

**process_http_request()** (http_server.c)
- Calls http_route_extract_params() when route matches

## Files Modified

1. **src/include/http_server.h**
   - Added HTTP_PARAM structure
   - Added parameter arrays to HTTP_REQUEST
   - Added function declarations

2. **src/http_server.c**
   - Implemented parameter getter/setter functions
   - Added URL decoding function
   - Added query string parser
   - Updated request lifecycle functions

3. **src/include/http_route.h**
   - Added http_route_extract_params() declaration

4. **src/http_route.c**
   - Reimplemented http_route_matches() with pattern support
   - Implemented http_route_extract_params()

5. **Makefile**
   - Added PARAM_DEMO target
   - Added run-param target

## New Files

1. **examples/param_demo.c**
   - Complete demonstration application
   - Shows all parameter features
   - Multiple route patterns
   - URL encoding examples

2. **PATH_PARAMETERS.md**
   - Comprehensive documentation
   - API reference
   - Examples and usage patterns
   - Implementation details

3. **PARAM_FEATURE_SUMMARY.md** (this file)
   - Implementation summary
   - Technical details

## Demo Application

Created a comprehensive demo (`examples/param_demo.c`) with:

1. **Root endpoint** (`/`)
   - Help text with examples

2. **User endpoint** (`/api/users/:id`)
   - Single path parameter
   - Query parameter support

3. **Comment endpoint** (`/api/posts/:postId/comments/:commentId`)
   - Multiple path parameters

4. **Search endpoint** (`/api/search`)
   - Query string parsing
   - URL decoding demonstration

### Running the Demo

```bash
# Build
make debug

# Run
make run-param

# Test
curl http://localhost:8080/
curl http://localhost:8080/api/users/123
curl 'http://localhost:8080/api/users/456?format=json&details=true'
curl http://localhost:8080/api/posts/100/comments/5
curl 'http://localhost:8080/api/search?q=test&page=2&limit=10'
curl 'http://localhost:8080/api/search?q=hello+world'
```

## Testing Results

✅ **All tests passed:**

1. ✅ Path parameter extraction: `/api/users/123` → `id = "123"`
2. ✅ Query parameter parsing: `?format=json&details=true`
3. ✅ Multiple path parameters: `/api/posts/100/comments/5`
4. ✅ Multiple query parameters: `?q=test&page=2&limit=10`
5. ✅ URL encoding (plus): `hello+world` → `"hello world"`
6. ✅ URL encoding (percent): `%40` → `"@"`
7. ✅ Combined path + query: `/api/users/456?format=json`

## Performance Characteristics

- **Route Matching**: O(n) where n = number of path segments
- **Parameter Lookup**: O(m) where m = number of parameters (typically < 10)
- **Query Parsing**: O(k) where k = query string length
- **Memory**: Dynamic arrays with realloc on capacity overflow
- **Initial Capacity**: 10 path params, 20 query params

## Memory Management

- Parameters stored in dynamically allocated arrays
- Arrays grow automatically when capacity exceeded
- Properly freed in http_request_destroy()
- No memory leaks (all allocations have corresponding frees)

## Limitations

- Max parameter name: 127 characters
- Max parameter value: 511 characters
- Max path length: 2047 characters
- No regex pattern support (yet)
- No optional segments (yet)
- No wildcard matching (yet)

## Future Enhancements

Possible additions:

- [ ] Regular expression patterns
- [ ] Optional path segments
- [ ] Wildcard matching (`*`)
- [ ] Parameter validation
- [ ] Type conversion helpers
- [ ] Query parameter arrays
- [ ] Parameter middleware

## Compatibility

- **C Standard**: C99
- **POSIX**: Uses strtok_r (reentrant tokenization)
- **Dependencies**: None (standard library only)
- **Platforms**: Linux (tested), other POSIX systems (should work)

## Documentation

- **User Guide**: [PATH_PARAMETERS.md](PATH_PARAMETERS.md)
- **Demo Code**: [examples/param_demo.c](examples/param_demo.c)
- **API Reference**: Included in PATH_PARAMETERS.md
- **Main README**: Updated with feature listing

## Build Status

✅ **Clean build** - No warnings, no errors
- Compiled with `-Wall -Wextra -Werror`
- All existing tests still pass
- New demo builds and runs successfully

## Summary

Successfully implemented comprehensive path parameter and query string support for the Equinox Framework HTTP server. The implementation is clean, efficient, and follows the existing code style and patterns. All features are fully documented and demonstrated in a working example application.

The framework now supports modern RESTful API patterns with dynamic route matching, making it suitable for building sophisticated web APIs in C.
