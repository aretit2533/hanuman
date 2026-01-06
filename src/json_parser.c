#include "json_parser.h"
#include "framework.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

/* For strdup on C99 */
#ifndef _GNU_SOURCE
char* strdup(const char *s);
#endif

/* Parser state */
typedef struct {
    const char *json;
    size_t position;
    size_t length;
    char error[256];
} JSON_PARSER_STATE;

/* Forward declarations */
static JSON_VALUE* parse_value(JSON_PARSER_STATE *state);
static void skip_whitespace(JSON_PARSER_STATE *state);

/* Utility functions */
static void skip_whitespace(JSON_PARSER_STATE *state)
{
    while (state->position < state->length) {
        char c = state->json[state->position];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            state->position++;
        } else {
            break;
        }
    }
}

static char peek_char(JSON_PARSER_STATE *state)
{
    if (state->position < state->length) {
        return state->json[state->position];
    }
    return '\0';
}

static char next_char(JSON_PARSER_STATE *state)
{
    if (state->position < state->length) {
        return state->json[state->position++];
    }
    return '\0';
}

static int match_keyword(JSON_PARSER_STATE *state, const char *keyword)
{
    size_t len = strlen(keyword);
    if (state->position + len > state->length) {
        return 0;
    }
    if (strncmp(&state->json[state->position], keyword, len) == 0) {
        state->position += len;
        return 1;
    }
    return 0;
}

/* Parse string */
static char* parse_string(JSON_PARSER_STATE *state)
{
    if (next_char(state) != '"') {
        snprintf(state->error, sizeof(state->error), "Expected '\"'");
        return NULL;
    }
    
    size_t start = state->position;
    size_t length = 0;
    
    while (state->position < state->length) {
        char c = state->json[state->position];
        if (c == '"') {
            break;
        } else if (c == '\\') {
            state->position++;
            if (state->position >= state->length) {
                snprintf(state->error, sizeof(state->error), "Unterminated string");
                return NULL;
            }
            state->position++;
            length++;
        } else {
            state->position++;
            length++;
        }
    }
    
    if (peek_char(state) != '"') {
        snprintf(state->error, sizeof(state->error), "Unterminated string");
        return NULL;
    }
    
    char *str = (char*)malloc(length + 1);
    if (!str) return NULL;
    
    size_t dest_pos = 0;
    size_t src_pos = start;
    
    while (src_pos < state->position) {
        char c = state->json[src_pos];
        if (c == '\\') {
            src_pos++;
            char escape = state->json[src_pos];
            switch (escape) {
                case '"': str[dest_pos++] = '"'; break;
                case '\\': str[dest_pos++] = '\\'; break;
                case '/': str[dest_pos++] = '/'; break;
                case 'b': str[dest_pos++] = '\b'; break;
                case 'f': str[dest_pos++] = '\f'; break;
                case 'n': str[dest_pos++] = '\n'; break;
                case 'r': str[dest_pos++] = '\r'; break;
                case 't': str[dest_pos++] = '\t'; break;
                default: str[dest_pos++] = escape; break;
            }
            src_pos++;
        } else {
            str[dest_pos++] = c;
            src_pos++;
        }
    }
    
    str[dest_pos] = '\0';
    next_char(state); /* Skip closing quote */
    
    return str;
}

/* Parse number */
static JSON_VALUE* parse_number(JSON_PARSER_STATE *state)
{
    size_t start = state->position;
    int is_double = 0;
    
    if (peek_char(state) == '-') {
        next_char(state);
    }
    
    while (isdigit(peek_char(state))) {
        next_char(state);
    }
    
    if (peek_char(state) == '.') {
        is_double = 1;
        next_char(state);
        while (isdigit(peek_char(state))) {
            next_char(state);
        }
    }
    
    if (peek_char(state) == 'e' || peek_char(state) == 'E') {
        is_double = 1;
        next_char(state);
        if (peek_char(state) == '+' || peek_char(state) == '-') {
            next_char(state);
        }
        while (isdigit(peek_char(state))) {
            next_char(state);
        }
    }
    
    size_t length = state->position - start;
    char *num_str = (char*)malloc(length + 1);
    if (!num_str) return NULL;
    
    strncpy(num_str, &state->json[start], length);
    num_str[length] = '\0';
    
    JSON_VALUE *value = (JSON_VALUE*)calloc(1, sizeof(JSON_VALUE));
    if (!value) {
        free(num_str);
        return NULL;
    }
    
    if (is_double) {
        value->type = JSON_TYPE_DOUBLE;
        value->data.double_value = atof(num_str);
    } else {
        value->type = JSON_TYPE_INTEGER;
        value->data.integer_value = atoll(num_str);
    }
    
    free(num_str);
    return value;
}

/* Parse array */
static JSON_VALUE* parse_array(JSON_PARSER_STATE *state)
{
    if (next_char(state) != '[') {
        snprintf(state->error, sizeof(state->error), "Expected '['");
        return NULL;
    }
    
    JSON_VALUE *array = (JSON_VALUE*)calloc(1, sizeof(JSON_VALUE));
    if (!array) return NULL;
    
    array->type = JSON_TYPE_ARRAY;
    array->data.array_value.elements = NULL;
    array->data.array_value.count = 0;
    
    skip_whitespace(state);
    
    if (peek_char(state) == ']') {
        next_char(state);
        return array;
    }
    
    size_t capacity = 4;
    array->data.array_value.elements = (JSON_VALUE**)malloc(capacity * sizeof(JSON_VALUE*));
    if (!array->data.array_value.elements) {
        free(array);
        return NULL;
    }
    
    while (1) {
        skip_whitespace(state);
        
        JSON_VALUE *element = parse_value(state);
        if (!element) {
            json_free(array);
            return NULL;
        }
        
        if (array->data.array_value.count >= capacity) {
            capacity *= 2;
            JSON_VALUE **new_elements = (JSON_VALUE**)realloc(
                array->data.array_value.elements, capacity * sizeof(JSON_VALUE*));
            if (!new_elements) {
                json_free(element);
                json_free(array);
                return NULL;
            }
            array->data.array_value.elements = new_elements;
        }
        
        array->data.array_value.elements[array->data.array_value.count++] = element;
        
        skip_whitespace(state);
        char c = peek_char(state);
        
        if (c == ']') {
            next_char(state);
            break;
        } else if (c == ',') {
            next_char(state);
        } else {
            snprintf(state->error, sizeof(state->error), "Expected ',' or ']'");
            json_free(array);
            return NULL;
        }
    }
    
    return array;
}

/* Parse object */
static JSON_VALUE* parse_object(JSON_PARSER_STATE *state)
{
    if (next_char(state) != '{') {
        snprintf(state->error, sizeof(state->error), "Expected '{'");
        return NULL;
    }
    
    JSON_VALUE *object = (JSON_VALUE*)calloc(1, sizeof(JSON_VALUE));
    if (!object) return NULL;
    
    object->type = JSON_TYPE_OBJECT;
    object->data.object_value.keys = NULL;
    object->data.object_value.values = NULL;
    object->data.object_value.count = 0;
    
    skip_whitespace(state);
    
    if (peek_char(state) == '}') {
        next_char(state);
        return object;
    }
    
    size_t capacity = 4;
    object->data.object_value.keys = (char**)malloc(capacity * sizeof(char*));
    object->data.object_value.values = (JSON_VALUE**)malloc(capacity * sizeof(JSON_VALUE*));
    
    if (!object->data.object_value.keys || !object->data.object_value.values) {
        json_free(object);
        return NULL;
    }
    
    while (1) {
        skip_whitespace(state);
        
        char *key = parse_string(state);
        if (!key) {
            json_free(object);
            return NULL;
        }
        
        skip_whitespace(state);
        
        if (next_char(state) != ':') {
            free(key);
            json_free(object);
            snprintf(state->error, sizeof(state->error), "Expected ':'");
            return NULL;
        }
        
        skip_whitespace(state);
        
        JSON_VALUE *value = parse_value(state);
        if (!value) {
            free(key);
            json_free(object);
            return NULL;
        }
        
        if (object->data.object_value.count >= capacity) {
            capacity *= 2;
            char **new_keys = (char**)realloc(
                object->data.object_value.keys, capacity * sizeof(char*));
            JSON_VALUE **new_values = (JSON_VALUE**)realloc(
                object->data.object_value.values, capacity * sizeof(JSON_VALUE*));
            
            if (!new_keys || !new_values) {
                free(key);
                json_free(value);
                json_free(object);
                return NULL;
            }
            
            object->data.object_value.keys = new_keys;
            object->data.object_value.values = new_values;
        }
        
        object->data.object_value.keys[object->data.object_value.count] = key;
        object->data.object_value.values[object->data.object_value.count] = value;
        object->data.object_value.count++;
        
        skip_whitespace(state);
        char c = peek_char(state);
        
        if (c == '}') {
            next_char(state);
            break;
        } else if (c == ',') {
            next_char(state);
        } else {
            snprintf(state->error, sizeof(state->error), "Expected ',' or '}'");
            json_free(object);
            return NULL;
        }
    }
    
    return object;
}

/* Parse value */
static JSON_VALUE* parse_value(JSON_PARSER_STATE *state)
{
    skip_whitespace(state);
    
    char c = peek_char(state);
    
    if (c == '{') {
        return parse_object(state);
    } else if (c == '[') {
        return parse_array(state);
    } else if (c == '"') {
        char *str = parse_string(state);
        if (!str) return NULL;
        
        JSON_VALUE *value = (JSON_VALUE*)calloc(1, sizeof(JSON_VALUE));
        if (!value) {
            free(str);
            return NULL;
        }
        value->type = JSON_TYPE_STRING;
        value->data.string_value = str;
        return value;
    } else if (match_keyword(state, "true")) {
        JSON_VALUE *value = (JSON_VALUE*)calloc(1, sizeof(JSON_VALUE));
        if (!value) return NULL;
        value->type = JSON_TYPE_BOOLEAN;
        value->data.boolean_value = 1;
        return value;
    } else if (match_keyword(state, "false")) {
        JSON_VALUE *value = (JSON_VALUE*)calloc(1, sizeof(JSON_VALUE));
        if (!value) return NULL;
        value->type = JSON_TYPE_BOOLEAN;
        value->data.boolean_value = 0;
        return value;
    } else if (match_keyword(state, "null")) {
        JSON_VALUE *value = (JSON_VALUE*)calloc(1, sizeof(JSON_VALUE));
        if (!value) return NULL;
        value->type = JSON_TYPE_NULL;
        return value;
    } else if (c == '-' || isdigit(c)) {
        return parse_number(state);
    } else {
        snprintf(state->error, sizeof(state->error), "Unexpected character: '%c'", c);
        return NULL;
    }
}

/* Public API */

JSON_VALUE* json_parse(const char *json_string)
{
    if (!json_string) return NULL;
    
    JSON_PARSER_STATE state = {
        .json = json_string,
        .position = 0,
        .length = strlen(json_string),
        .error = {0}
    };
    
    JSON_VALUE *value = parse_value(&state);
    
    if (!value && state.error[0]) {
        framework_log(LOG_LEVEL_ERROR, "JSON parse error: %s", state.error);
    }
    
    return value;
}

/* Find value in object by key */
static JSON_VALUE* find_object_value(JSON_VALUE *object, const char *key)
{
    if (!object || object->type != JSON_TYPE_OBJECT) {
        return NULL;
    }
    
    for (size_t i = 0; i < object->data.object_value.count; i++) {
        if (strcmp(object->data.object_value.keys[i], key) == 0) {
            return object->data.object_value.values[i];
        }
    }
    
    return NULL;
}

JSON_VALUE* json_get_path(JSON_VALUE *value, const char *path)
{
    if (!value || !path) return NULL;
    
    /* Duplicate path for tokenization */
    size_t path_len = strlen(path);
    char *path_copy = (char*)malloc(path_len + 1);
    if (!path_copy) return NULL;
    strcpy(path_copy, path);
    
    JSON_VALUE *current = value;
    char *token = strtok(path_copy, ".");
    
    while (token && current) {
        current = find_object_value(current, token);
        token = strtok(NULL, ".");
    }
    
    free(path_copy);
    return current;
}

const char* json_get_string(JSON_VALUE *value)
{
    if (value && value->type == JSON_TYPE_STRING) {
        return value->data.string_value;
    }
    return NULL;
}

int json_get_int(JSON_VALUE *value, int default_val)
{
    if (!value) return default_val;
    
    if (value->type == JSON_TYPE_INTEGER) {
        return (int)value->data.integer_value;
    } else if (value->type == JSON_TYPE_DOUBLE) {
        return (int)value->data.double_value;
    }
    
    return default_val;
}

int json_get_bool(JSON_VALUE *value, int default_val)
{
    if (value && value->type == JSON_TYPE_BOOLEAN) {
        return value->data.boolean_value;
    }
    return default_val;
}

double json_get_double(JSON_VALUE *value, double default_val)
{
    if (!value) return default_val;
    
    if (value->type == JSON_TYPE_DOUBLE) {
        return value->data.double_value;
    } else if (value->type == JSON_TYPE_INTEGER) {
        return (double)value->data.integer_value;
    }
    
    return default_val;
}

void json_free(JSON_VALUE *value)
{
    if (!value) return;
    
    switch (value->type) {
        case JSON_TYPE_STRING:
            free(value->data.string_value);
            break;
            
        case JSON_TYPE_ARRAY:
            for (size_t i = 0; i < value->data.array_value.count; i++) {
                json_free(value->data.array_value.elements[i]);
            }
            free(value->data.array_value.elements);
            break;
            
        case JSON_TYPE_OBJECT:
            for (size_t i = 0; i < value->data.object_value.count; i++) {
                free(value->data.object_value.keys[i]);
                json_free(value->data.object_value.values[i]);
            }
            free(value->data.object_value.keys);
            free(value->data.object_value.values);
            break;
            
        default:
            break;
    }
    
    free(value);
}

/* Forward declaration for schema parsing */
static int json_parse_with_schema_value(JSON_VALUE *json_value, const JSON_SCHEMA *schema, void *target);

/* Schema-based parsing */
static int apply_schema_field(JSON_VALUE *json_obj, const JSON_SCHEMA_FIELD *field, 
                             void *target, JSON_VALIDATION_RESULT *result)
{
    JSON_VALUE *field_value = find_object_value(json_obj, field->name);
    
    if (!field_value) {
        if (field->flags & SCHEMA_FLAG_REQUIRED) {
            if (result) {
                snprintf(result->error_message, sizeof(result->error_message),
                        "Required field '%s' is missing", field->name);
                result->error_field = field->name;
                result->valid = 0;
            }
            return -1;
        }
        return 0;
    }
    
    void *field_ptr = (char*)target + field->offset;
    
    switch (field->type) {
        case SCHEMA_TYPE_BOOL:
            *(int*)field_ptr = json_get_bool(field_value, 0);
            break;
            
        case SCHEMA_TYPE_INT:
            *(int*)field_ptr = json_get_int(field_value, 0);
            break;
            
        case SCHEMA_TYPE_INT64:
            if (field_value->type == JSON_TYPE_INTEGER) {
                *(int64_t*)field_ptr = field_value->data.integer_value;
            }
            break;
            
        case SCHEMA_TYPE_DOUBLE:
            *(double*)field_ptr = json_get_double(field_value, 0.0);
            break;
            
        case SCHEMA_TYPE_STRING: {
            const char *str = json_get_string(field_value);
            if (str) {
                strncpy((char*)field_ptr, str, field->max_length - 1);
                ((char*)field_ptr)[field->max_length - 1] = '\0';
            }
            break;
        }
            
        case SCHEMA_TYPE_OBJECT:
            if (field->nested) {
                return json_parse_with_schema_value(field_value, field->nested, field_ptr);
            }
            break;
            
        default:
            break;
    }
    
    if (field->validator) {
        if (!field->validator(field_ptr)) {
            if (result) {
                snprintf(result->error_message, sizeof(result->error_message),
                        "Validation failed for field '%s'", field->name);
                result->error_field = field->name;
                result->valid = 0;
            }
            return -1;
        }
    }
    
    return 0;
}

static int json_parse_with_schema_value(JSON_VALUE *json_value, const JSON_SCHEMA *schema, void *target)
{
    if (!json_value || !schema || !target) {
        return -1;
    }
    
    if (json_value->type != JSON_TYPE_OBJECT) {
        return -1;
    }
    
    memset(target, 0, schema->struct_size);
    
    for (size_t i = 0; i < schema->field_count; i++) {
        if (apply_schema_field(json_value, &schema->fields[i], target, NULL) != 0) {
            return -1;
        }
    }
    
    return 0;
}

int json_parse_with_schema(const char *json_string, const JSON_SCHEMA *schema, void *target)
{
    JSON_VALUE *json = json_parse(json_string);
    if (!json) return -1;
    
    int result = json_parse_with_schema_value(json, schema, target);
    json_free(json);
    
    return result;
}

int json_parse_and_validate(const char *json_string, const JSON_SCHEMA *schema,
                           void *target, JSON_VALIDATION_RESULT *result)
{
    if (result) {
        result->valid = 1;
        result->error_message[0] = '\0';
        result->error_field = NULL;
    }
    
    JSON_VALUE *json = json_parse(json_string);
    if (!json) {
        if (result) {
            snprintf(result->error_message, sizeof(result->error_message),
                    "Failed to parse JSON");
            result->valid = 0;
        }
        return -1;
    }
    
    if (json->type != JSON_TYPE_OBJECT) {
        if (result) {
            snprintf(result->error_message, sizeof(result->error_message),
                    "Expected JSON object");
            result->valid = 0;
        }
        json_free(json);
        return -1;
    }
    
    memset(target, 0, schema->struct_size);
    
    for (size_t i = 0; i < schema->field_count; i++) {
        if (apply_schema_field(json, &schema->fields[i], target, result) != 0) {
            json_free(json);
            return -1;
        }
    }
    
    json_free(json);
    return 0;
}

/* JSON serialization */
int json_serialize(const void *source, const JSON_SCHEMA *schema,
                  char *buffer, size_t buffer_size)
{
    if (!source || !schema || !buffer || buffer_size == 0) {
        return -1;
    }
    
    size_t pos = 0;
    
    pos += snprintf(buffer + pos, buffer_size - pos, "{");
    
    for (size_t i = 0; i < schema->field_count; i++) {
        const JSON_SCHEMA_FIELD *field = &schema->fields[i];
        const void *field_ptr = (const char*)source + field->offset;
        
        if (i > 0) {
            pos += snprintf(buffer + pos, buffer_size - pos, ",");
        }
        
        pos += snprintf(buffer + pos, buffer_size - pos, "\"%s\":", field->name);
        
        switch (field->type) {
            case SCHEMA_TYPE_BOOL:
                pos += snprintf(buffer + pos, buffer_size - pos, "%s",
                              *(int*)field_ptr ? "true" : "false");
                break;
                
            case SCHEMA_TYPE_INT:
                pos += snprintf(buffer + pos, buffer_size - pos, "%d",
                              *(int*)field_ptr);
                break;
                
            case SCHEMA_TYPE_INT64:
                pos += snprintf(buffer + pos, buffer_size - pos, "%lld",
                              (long long)*(int64_t*)field_ptr);
                break;
                
            case SCHEMA_TYPE_DOUBLE:
                pos += snprintf(buffer + pos, buffer_size - pos, "%.10g",
                              *(double*)field_ptr);
                break;
                
            case SCHEMA_TYPE_STRING:
                pos += snprintf(buffer + pos, buffer_size - pos, "\"%s\"",
                              (char*)field_ptr);
                break;
                
            default:
                pos += snprintf(buffer + pos, buffer_size - pos, "null");
                break;
        }
        
        if (pos >= buffer_size) {
            return -1;
        }
    }
    
    pos += snprintf(buffer + pos, buffer_size - pos, "}");
    
    return (int)pos;
}

/* JSON Builder */
JSON_BUILDER* json_builder_create(size_t initial_size)
{
    JSON_BUILDER *builder = (JSON_BUILDER*)calloc(1, sizeof(JSON_BUILDER));
    if (!builder) return NULL;
    
    builder->buffer = (char*)malloc(initial_size);
    if (!builder->buffer) {
        free(builder);
        return NULL;
    }
    
    builder->size = initial_size;
    builder->position = 0;
    builder->error = 0;
    
    return builder;
}

void json_builder_destroy(JSON_BUILDER *builder)
{
    if (builder) {
        free(builder->buffer);
        free(builder);
    }
}

static void builder_append(JSON_BUILDER *builder, const char *str)
{
    if (builder->error) return;
    
    size_t len = strlen(str);
    
    if (builder->position + len >= builder->size) {
        size_t new_size = builder->size * 2;
        while (builder->position + len >= new_size) {
            new_size *= 2;
        }
        
        char *new_buffer = (char*)realloc(builder->buffer, new_size);
        if (!new_buffer) {
            builder->error = 1;
            return;
        }
        
        builder->buffer = new_buffer;
        builder->size = new_size;
    }
    
    memcpy(builder->buffer + builder->position, str, len);
    builder->position += len;
    builder->buffer[builder->position] = '\0';
}

void json_builder_start_object(JSON_BUILDER *builder)
{
    builder_append(builder, "{");
}

void json_builder_end_object(JSON_BUILDER *builder)
{
    if (builder->position > 0 && builder->buffer[builder->position - 1] == ',') {
        builder->position--;
    }
    builder_append(builder, "}");
}

void json_builder_start_array(JSON_BUILDER *builder)
{
    builder_append(builder, "[");
}

void json_builder_end_array(JSON_BUILDER *builder)
{
    if (builder->position > 0 && builder->buffer[builder->position - 1] == ',') {
        builder->position--;
    }
    builder_append(builder, "]");
}

void json_builder_add_string(JSON_BUILDER *builder, const char *key, const char *value)
{
    char temp[2048];
    if (key) {
        snprintf(temp, sizeof(temp), "\"%s\":\"%s\",", key, value ? value : "");
    } else {
        snprintf(temp, sizeof(temp), "\"%s\",", value ? value : "");
    }
    builder_append(builder, temp);
}

void json_builder_add_int(JSON_BUILDER *builder, const char *key, int value)
{
    char temp[256];
    if (key) {
        snprintf(temp, sizeof(temp), "\"%s\":%d,", key, value);
    } else {
        snprintf(temp, sizeof(temp), "%d,", value);
    }
    builder_append(builder, temp);
}

void json_builder_add_int64(JSON_BUILDER *builder, const char *key, int64_t value)
{
    char temp[256];
    if (key) {
        snprintf(temp, sizeof(temp), "\"%s\":%lld,", key, (long long)value);
    } else {
        snprintf(temp, sizeof(temp), "%lld,", (long long)value);
    }
    builder_append(builder, temp);
}

void json_builder_add_double(JSON_BUILDER *builder, const char *key, double value)
{
    char temp[256];
    if (key) {
        snprintf(temp, sizeof(temp), "\"%s\":%.10g,", key, value);
    } else {
        snprintf(temp, sizeof(temp), "%.10g,", value);
    }
    builder_append(builder, temp);
}

void json_builder_add_bool(JSON_BUILDER *builder, const char *key, int value)
{
    char temp[256];
    if (key) {
        snprintf(temp, sizeof(temp), "\"%s\":%s,", key, value ? "true" : "false");
    } else {
        snprintf(temp, sizeof(temp), "%s,", value ? "true" : "false");
    }
    builder_append(builder, temp);
}

void json_builder_add_null(JSON_BUILDER *builder, const char *key)
{
    char temp[256];
    if (key) {
        snprintf(temp, sizeof(temp), "\"%s\":null,", key);
    } else {
        snprintf(temp, sizeof(temp), "null,");
    }
    builder_append(builder, temp);
}

const char* json_builder_get_string(JSON_BUILDER *builder)
{
    if (builder && !builder->error) {
        return builder->buffer;
    }
    return NULL;
}
