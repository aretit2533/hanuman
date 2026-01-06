# JSON Parser with Schema Support

The Equinox framework provides a powerful JSON parser with schema-based validation and type-safe struct mapping.

## Overview

Instead of manually parsing JSON strings, you define a schema that maps JSON fields directly to C struct members. The framework handles:
- **Type validation** - Ensures JSON types match expected types
- **Required field checking** - Validates required fields are present
- **Automatic conversion** - Converts JSON to native C types
- **Nested objects** - Supports complex nested structures
- **Serialization** - Convert structs back to JSON

## Quick Example

```c
#include "json_parser.h"

/* Define your struct */
typedef struct {
    int id;
    char name[128];
    char email[128];
    int age;
    int is_active;
} User;

/* Define schema using macros */
JSON_SCHEMA_DEFINE(user_schema, User,
    JSON_SCHEMA_FIELD_INT(User, id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(User, name, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(User, email, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_INT(User, age, 0),
    JSON_SCHEMA_FIELD_BOOL(User, is_active, 0)
);

/* Parse JSON string */
const char *json = "{\"id\":1,\"name\":\"Alice\",\"email\":\"alice@example.com\",\"age\":30,\"is_active\":true}";

User user;
if (json_parse_with_schema(json, &user_schema, &user) == 0) {
    printf("User: %s (%s), age %d\n", user.name, user.email, user.age);
}
```

## Schema Definition

### Basic Types

```c
JSON_SCHEMA_FIELD_BOOL(struct_type, field_name, flags)      // Boolean
JSON_SCHEMA_FIELD_INT(struct_type, field_name, flags)       // Integer
JSON_SCHEMA_FIELD_INT64(struct_type, field_name, flags)     // 64-bit integer
JSON_SCHEMA_FIELD_DOUBLE(struct_type, field_name, flags)    // Double
JSON_SCHEMA_FIELD_STRING(struct_type, field_name, max_len, flags)  // String
```

### Schema Flags

```c
SCHEMA_FLAG_REQUIRED    // Field must be present
SCHEMA_FLAG_NULLABLE    // Field can be null
SCHEMA_FLAG_READONLY    // Field is read-only (skip on serialize)
```

### Example with All Types

```c
typedef struct {
    int id;                 // Integer
    int64_t timestamp;      // Large integer
    char name[128];         // String with max length
    double price;           // Floating point
    int is_active;          // Boolean
} Product;

JSON_SCHEMA_DEFINE(product_schema, Product,
    JSON_SCHEMA_FIELD_INT(Product, id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_INT64(Product, timestamp, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(Product, name, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_DOUBLE(Product, price, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_BOOL(Product, is_active, 0)
);
```

## Nested Objects

You can nest schemas for complex structures:

```c
typedef struct {
    char street[128];
    char city[64];
    int zip_code;
} Address;

typedef struct {
    int id;
    char name[128];
    Address address;  // Nested struct
} User;

/* Define address schema */
JSON_SCHEMA_DEFINE(address_schema, Address,
    JSON_SCHEMA_FIELD_STRING(Address, street, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(Address, city, 64, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_INT(Address, zip_code, 0)
);

/* Define user schema with nested address */
JSON_SCHEMA_DEFINE(user_schema, User,
    JSON_SCHEMA_FIELD_INT(User, id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(User, name, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_OBJECT(User, address, &address_schema, SCHEMA_FLAG_REQUIRED)
);

/* Parse nested JSON */
const char *json = "{\"id\":1,\"name\":\"Alice\",\"address\":{\"street\":\"123 Main St\",\"city\":\"SF\",\"zip_code\":94102}}";
User user;
json_parse_with_schema(json, &user_schema, &user);
```

## Validation

### Basic Validation

```c
User user;
if (json_parse_with_schema(json, &user_schema, &user) == 0) {
    // Success - user struct is populated
} else {
    // Parse failed
}
```

### Detailed Validation

```c
User user;
JSON_VALIDATION_RESULT result;

if (json_parse_and_validate(json, &user_schema, &user, &result) == 0) {
    printf("✓ Valid user: %s\n", user.name);
} else {
    printf("✗ Validation failed:\n");
    printf("  Error: %s\n", result.error_message);
    printf("  Field: %s\n", result.error_field);
}
```

## HTTP Integration

Perfect for handling JSON in HTTP endpoints:

```c
void handle_create_user(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    User user;
    JSON_VALIDATION_RESULT result;
    
    /* Parse and validate request body */
    if (json_parse_and_validate(request->body, &user_schema, &user, &result) == 0) {
        /* Valid user data */
        printf("Created user: %s\n", user.name);
        
        /* Build success response */
        JSON_BUILDER *builder = json_builder_create(1024);
        json_builder_start_object(builder);
        json_builder_add_string(builder, "status", "success");
        json_builder_add_int(builder, "user_id", user.id);
        json_builder_end_object(builder);
        
        http_response_set_status(response, HTTP_STATUS_CREATED);
        http_response_set_json(response, json_builder_get_string(builder));
        
        json_builder_destroy(builder);
    } else {
        /* Validation failed */
        char error_json[512];
        snprintf(error_json, sizeof(error_json),
                "{\"error\":\"%s\",\"field\":\"%s\"}",
                result.error_message, result.error_field);
        
        http_response_set_status(response, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(response, error_json);
    }
}
```

## JSON Serialization

Convert structs back to JSON:

```c
User user = {
    .id = 1,
    .name = "Alice",
    .email = "alice@example.com",
    .age = 30,
    .is_active = 1
};

char json_buffer[1024];
json_serialize(&user, &user_schema, json_buffer, sizeof(json_buffer));
printf("%s\n", json_buffer);
// Output: {"id":1,"name":"Alice","email":"alice@example.com","age":30,"is_active":true}
```

## JSON Builder (Manual Construction)

For dynamic JSON creation:

```c
JSON_BUILDER *builder = json_builder_create(1024);

json_builder_start_object(builder);
json_builder_add_string(builder, "status", "success");
json_builder_add_int(builder, "code", 200);
json_builder_add_bool(builder, "authenticated", 1);
json_builder_add_double(builder, "balance", 1234.56);

json_builder_start_object(builder); // Nested object
json_builder_add_string(builder, "name", "Alice");
json_builder_add_string(builder, "role", "admin");
json_builder_end_object(builder);

json_builder_end_object(builder);

const char *json = json_builder_get_string(builder);
printf("%s\n", json);

json_builder_destroy(builder);
```

## Low-Level API (Without Schema)

For cases where schemas aren't suitable:

```c
/* Parse to JSON tree */
JSON_VALUE *root = json_parse(json_string);

/* Navigate tree */
JSON_VALUE *name = json_get_path(root, "user.name");
const char *name_str = json_get_string(name);

JSON_VALUE *age = json_get_path(root, "user.age");
int age_val = json_get_int(age, 0);

/* Free tree */
json_free(root);
```

### Path Access

```c
JSON_VALUE *root = json_parse("{\"user\":{\"profile\":{\"name\":\"Alice\"}}}");

/* Get nested value */
JSON_VALUE *name = json_get_path(root, "user.profile.name");
printf("Name: %s\n", json_get_string(name));

json_free(root);
```

## Complete Example: REST API

```c
#include "application.h"
#include "http_server.h"
#include "json_parser.h"

typedef struct {
    int id;
    char product[128];
    double price;
    int quantity;
} Order;

JSON_SCHEMA_DEFINE(order_schema, Order,
    JSON_SCHEMA_FIELD_INT(Order, id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(Order, product, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_DOUBLE(Order, price, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_INT(Order, quantity, SCHEMA_FLAG_REQUIRED)
);

void handle_create_order(HTTP_REQUEST *req, HTTP_RESPONSE *res, void *data)
{
    (void)data;
    
    Order order;
    JSON_VALIDATION_RESULT result;
    
    if (json_parse_and_validate(req->body, &order_schema, &order, &result) == 0) {
        /* Process order */
        double total = order.price * order.quantity;
        
        /* Build response */
        JSON_BUILDER *b = json_builder_create(512);
        json_builder_start_object(b);
        json_builder_add_int(b, "order_id", order.id);
        json_builder_add_string(b, "product", order.product);
        json_builder_add_double(b, "total", total);
        json_builder_add_string(b, "status", "confirmed");
        json_builder_end_object(b);
        
        http_response_set_status(res, HTTP_STATUS_CREATED);
        http_response_set_json(res, json_builder_get_string(b));
        
        json_builder_destroy(b);
    } else {
        /* Return error */
        char err[512];
        snprintf(err, sizeof(err), "{\"error\":\"%s\"}", result.error_message);
        http_response_set_status(res, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(res, err);
    }
}

int main(void)
{
    framework_init();
    APPLICATION *app = application_create("Order API", 1);
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    
    http_server_add_route(server, HTTP_METHOD_POST, "/api/orders", 
                         handle_create_order, NULL);
    
    application_set_http_server(app, server);
    application_run(app);
    
    http_server_destroy(server);
    application_destroy(app);
    framework_shutdown();
    return 0;
}
```

## Testing

```bash
# Build
make debug

# Run demo
./build/json_schema_demo

# Test with curl
curl -X POST http://localhost:8080/api/users \
  -H 'Content-Type: application/json' \
  -d '{
    "id": 1,
    "name": "Alice Johnson",
    "email": "alice@example.com",
    "age": 30,
    "is_active": true,
    "balance": 1250.75,
    "address": {
      "street": "123 Main St",
      "city": "San Francisco",
      "state": "CA",
      "zip_code": 94102
    }
  }'

# Test validation error (missing required field)
curl -X POST http://localhost:8080/api/users \
  -H 'Content-Type: application/json' \
  -d '{"name": "Invalid User"}'
```

## Run Demo

```bash
# Build and run
make run-json

# Or directly
./build/json_schema_demo

# Open browser to
http://localhost:8080/api/demo
```

## API Reference

### Parsing Functions

| Function | Description |
|----------|-------------|
| `json_parse(json_string)` | Parse to JSON tree |
| `json_parse_with_schema(json, schema, target)` | Parse to struct |
| `json_parse_and_validate(json, schema, target, result)` | Parse with detailed validation |

### Access Functions

| Function | Description |
|----------|-------------|
| `json_get_path(value, "path.to.field")` | Get nested value |
| `json_get_string(value)` | Extract string |
| `json_get_int(value, default)` | Extract integer |
| `json_get_bool(value, default)` | Extract boolean |
| `json_get_double(value, default)` | Extract double |

### Builder Functions

| Function | Description |
|----------|-------------|
| `json_builder_create(size)` | Create builder |
| `json_builder_start_object()` | Start object `{` |
| `json_builder_end_object()` | End object `}` |
| `json_builder_add_string(key, val)` | Add string field |
| `json_builder_add_int(key, val)` | Add integer field |
| `json_builder_add_bool(key, val)` | Add boolean field |
| `json_builder_get_string()` | Get result |
| `json_builder_destroy()` | Free builder |

### Memory Management

| Function | Description |
|----------|-------------|
| `json_free(value)` | Free JSON tree |
| `json_builder_destroy(builder)` | Free builder |

## Benefits

✅ **Type Safety** - Compile-time struct validation  
✅ **Less Boilerplate** - No manual parsing code  
✅ **Validation** - Automatic type and requirement checking  
✅ **Nested Support** - Complex object hierarchies  
✅ **Bidirectional** - Parse and serialize  
✅ **HTTP Integration** - Works seamlessly with HTTP server  

## See Also

- [HTTP Server Documentation](../README.md)
- [JSON Schema Demo](../examples/json_schema_demo.c)
- [API Header](../src/include/json_parser.h)
