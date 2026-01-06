# JSON Parser - Quick Reference

## Define Schema

```c
typedef struct {
    int id;
    char name[128];
    int age;
} User;

JSON_SCHEMA_DEFINE(user_schema, User,
    JSON_SCHEMA_FIELD_INT(User, id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(User, name, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_INT(User, age, 0)
);
```

## Parse JSON → Struct

```c
const char *json = "{\"id\":1,\"name\":\"Alice\",\"age\":30}";
User user;

if (json_parse_with_schema(json, &user_schema, &user) == 0) {
    printf("Name: %s, Age: %d\n", user.name, user.age);
}
```

## Validate with Error Details

```c
User user;
JSON_VALIDATION_RESULT result;

if (json_parse_and_validate(json, &user_schema, &user, &result) == 0) {
    // Success
} else {
    printf("Error: %s in field '%s'\n", 
           result.error_message, result.error_field);
}
```

## Serialize Struct → JSON

```c
User user = {.id = 1, .name = "Alice", .age = 30};
char json[512];

json_serialize(&user, &user_schema, json, sizeof(json));
// Output: {"id":1,"name":"Alice","age":30}
```

## Build JSON Manually

```c
JSON_BUILDER *b = json_builder_create(512);
json_builder_start_object(b);
json_builder_add_int(b, "id", 1);
json_builder_add_string(b, "name", "Alice");
json_builder_add_bool(b, "active", 1);
json_builder_end_object(b);

const char *json = json_builder_get_string(b);
json_builder_destroy(b);
```

## HTTP Endpoint Example

```c
void handle_user(HTTP_REQUEST *req, HTTP_RESPONSE *res, void *data)
{
    User user;
    
    if (json_parse_with_schema(req->body, &user_schema, &user) == 0) {
        // Valid - process user
        char response[256];
        snprintf(response, sizeof(response),
                "{\"status\":\"ok\",\"user_id\":%d}", user.id);
        http_response_set_json(res, response);
    } else {
        // Invalid
        http_response_set_status(res, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(res, "{\"error\":\"Invalid JSON\"}");
    }
}
```

## Nested Objects

```c
typedef struct {
    char city[64];
    int zip;
} Address;

typedef struct {
    int id;
    char name[128];
    Address address;
} User;

JSON_SCHEMA_DEFINE(address_schema, Address,
    JSON_SCHEMA_FIELD_STRING(Address, city, 64, 0),
    JSON_SCHEMA_FIELD_INT(Address, zip, 0)
);

JSON_SCHEMA_DEFINE(user_schema, User,
    JSON_SCHEMA_FIELD_INT(User, id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(User, name, 128, 0),
    JSON_SCHEMA_FIELD_OBJECT(User, address, &address_schema, 0)
);
```

## Field Types

| Macro | C Type | JSON Type |
|-------|--------|-----------|
| `JSON_SCHEMA_FIELD_BOOL` | `int` | `true`/`false` |
| `JSON_SCHEMA_FIELD_INT` | `int` | `123` |
| `JSON_SCHEMA_FIELD_INT64` | `int64_t` | `9223372036854775807` |
| `JSON_SCHEMA_FIELD_DOUBLE` | `double` | `3.14` |
| `JSON_SCHEMA_FIELD_STRING` | `char[]` | `"text"` |
| `JSON_SCHEMA_FIELD_OBJECT` | `struct` | `{...}` |

## Flags

| Flag | Description |
|------|-------------|
| `SCHEMA_FLAG_REQUIRED` | Field must be present |
| `SCHEMA_FLAG_NULLABLE` | Field can be null |
| `0` | Optional field |

## Try the Demo

```bash
# Build
make run-json

# Browser
http://localhost:8080/api/demo

# curl test
curl -X POST http://localhost:8080/api/users \
  -H 'Content-Type: application/json' \
  -d '{"id":1,"name":"Alice","email":"alice@example.com","age":30,"is_active":true}'
```

## Full Documentation

See [JSON_PARSER.md](docs/JSON_PARSER.md) for complete API reference.
