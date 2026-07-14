#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>
#include <stdint.h>

/* JSON value types */
typedef enum {
    JSON_TYPE_NULL,
    JSON_TYPE_BOOLEAN,
    JSON_TYPE_INTEGER,
    JSON_TYPE_DOUBLE,
    JSON_TYPE_STRING,
    JSON_TYPE_ARRAY,
    JSON_TYPE_OBJECT
} JSON_TYPE;

/* JSON value structure */
typedef struct _json_value_ {
    JSON_TYPE type;
    union {
        int boolean_value;
        int64_t integer_value;
        double double_value;
        char *string_value;
        struct {
            struct _json_value_ **elements;
            size_t count;
        } array_value;
        struct {
            char **keys;
            struct _json_value_ **values;
            size_t count;
        } object_value;
    } data;
} JSON_VALUE;

/* Schema field types */
typedef enum {
    SCHEMA_TYPE_BOOL,
    SCHEMA_TYPE_INT,
    SCHEMA_TYPE_INT64,
    SCHEMA_TYPE_DOUBLE,
    SCHEMA_TYPE_STRING,
    SCHEMA_TYPE_ARRAY,
    SCHEMA_TYPE_OBJECT,
    SCHEMA_TYPE_CUSTOM
} SCHEMA_TYPE;

/* Schema field flags */
#define SCHEMA_FLAG_REQUIRED    0x01
#define SCHEMA_FLAG_NULLABLE    0x02
#define SCHEMA_FLAG_READONLY    0x04

/* Schema field definition */
typedef struct _json_schema_field_ {
    const char *name;              /* Field name in JSON */
    SCHEMA_TYPE type;              /* Data type */
    size_t offset;                 /* Offset in target struct */
    size_t max_length;             /* Max length for strings/arrays */
    int flags;                     /* Schema flags */
    void *default_value;           /* Default value if not present */
    struct _json_schema_ *nested;  /* Nested schema for objects */
    int (*validator)(void *value); /* Custom validator function */
} JSON_SCHEMA_FIELD;

/* Schema definition */
typedef struct _json_schema_ {
    const char *name;
    JSON_SCHEMA_FIELD *fields;
    size_t field_count;
    size_t struct_size;
} JSON_SCHEMA;

/* Schema validation result */
typedef struct {
    int valid;
    char error_message[512];
    const char *error_field;
} JSON_VALIDATION_RESULT;

/* Helper macros for schema definition */
#define JSON_SCHEMA_FIELD_BOOL(struct_type, field_name, flags) \
    { #field_name, SCHEMA_TYPE_BOOL, offsetof(struct_type, field_name), 0, flags, NULL, NULL, NULL }

#define JSON_SCHEMA_FIELD_INT(struct_type, field_name, flags) \
    { #field_name, SCHEMA_TYPE_INT, offsetof(struct_type, field_name), 0, flags, NULL, NULL, NULL }

#define JSON_SCHEMA_FIELD_INT64(struct_type, field_name, flags) \
    { #field_name, SCHEMA_TYPE_INT64, offsetof(struct_type, field_name), 0, flags, NULL, NULL, NULL }

#define JSON_SCHEMA_FIELD_DOUBLE(struct_type, field_name, flags) \
    { #field_name, SCHEMA_TYPE_DOUBLE, offsetof(struct_type, field_name), 0, flags, NULL, NULL, NULL }

#define JSON_SCHEMA_FIELD_STRING(struct_type, field_name, max_len, flags) \
    { #field_name, SCHEMA_TYPE_STRING, offsetof(struct_type, field_name), max_len, flags, NULL, NULL, NULL }

#define JSON_SCHEMA_FIELD_OBJECT(struct_type, field_name, nested_schema, flags) \
    { #field_name, SCHEMA_TYPE_OBJECT, offsetof(struct_type, field_name), 0, flags, NULL, nested_schema, NULL }

#define JSON_SCHEMA_DEFINE(schema_name, struct_type, ...) \
    static JSON_SCHEMA_FIELD schema_name##_fields[] = { __VA_ARGS__ }; \
    static JSON_SCHEMA schema_name = { \
        #schema_name, \
        schema_name##_fields, \
        sizeof(schema_name##_fields) / sizeof(JSON_SCHEMA_FIELD), \
        sizeof(struct_type) \
    }

/* JSON Parser API */

/**
 * Parse JSON string into JSON_VALUE tree
 * @param json_string JSON string to parse
 * @return JSON_VALUE pointer or NULL on error
 */
JSON_VALUE* json_parse(const char *json_string);

/**
 * Parse JSON string directly into struct using schema
 * @param json_string JSON string to parse
 * @param schema Schema definition
 * @param target Target struct to populate
 * @return 0 on success, error code on failure
 */
int json_parse_with_schema(const char *json_string, const JSON_SCHEMA *schema, void *target);

/**
 * Parse JSON string with validation
 * @param json_string JSON string to parse
 * @param schema Schema definition
 * @param target Target struct to populate
 * @param result Validation result
 * @return 0 on success, error code on failure
 */
int json_parse_and_validate(const char *json_string, const JSON_SCHEMA *schema, 
                            void *target, JSON_VALIDATION_RESULT *result);

/**
 * Serialize struct to JSON string using schema
 * @param source Source struct
 * @param schema Schema definition
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
int json_serialize(const void *source, const JSON_SCHEMA *schema, 
                   char *buffer, size_t buffer_size);

/**
 * Get value from JSON object by key path (e.g., "user.address.city")
 * @param value JSON object
 * @param path Dot-separated path
 * @return JSON_VALUE pointer or NULL if not found
 */
JSON_VALUE* json_get_path(JSON_VALUE *value, const char *path);

/**
 * Get string value from JSON object
 * @param value JSON value
 * @return String value or NULL
 */
const char* json_get_string(JSON_VALUE *value);

/**
 * Get integer value from JSON object
 * @param value JSON value
 * @param default_val Default value if not integer
 * @return Integer value
 */
int json_get_int(JSON_VALUE *value, int default_val);

/**
 * Get boolean value from JSON object
 * @param value JSON value
 * @param default_val Default value if not boolean
 * @return Boolean value
 */
int json_get_bool(JSON_VALUE *value, int default_val);

/**
 * Get double value from JSON object
 * @param value JSON value
 * @param default_val Default value if not double
 * @return Double value
 */
double json_get_double(JSON_VALUE *value, double default_val);

/**
 * Free JSON value tree
 * @param value JSON value to free
 */
void json_free(JSON_VALUE *value);

/**
 * Create JSON string builder
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return 0 on success
 */
typedef struct {
    char *buffer;
    size_t size;
    size_t position;
    int error;
} JSON_BUILDER;

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

#endif /* JSON_PARSER_H */
