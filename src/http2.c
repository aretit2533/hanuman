#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "http2.h"
#include "framework.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define INITIAL_STREAM_CAPACITY 10
#define INITIAL_HEADER_CAPACITY 20
#define MAX_FRAME_SIZE 16384

/* Default HTTP/2 Settings */
static const uint32_t default_settings[7] = {
    0,      /* Reserved */
    4096,   /* HEADER_TABLE_SIZE */
    1,      /* ENABLE_PUSH */
    100,    /* MAX_CONCURRENT_STREAMS */
    65535,  /* INITIAL_WINDOW_SIZE */
    16384,  /* MAX_FRAME_SIZE */
    8192    /* MAX_HEADER_LIST_SIZE */
};

HTTP2_CONNECTION* http2_connection_create(int socket)
{
    HTTP2_CONNECTION *conn = (HTTP2_CONNECTION*)calloc(1, sizeof(HTTP2_CONNECTION));
    if (!conn) return NULL;
    
    conn->socket = socket;
    conn->is_http2 = 0;
    conn->preface_received = 0;
    
    /* Initialize settings with defaults */
    memcpy(conn->local_settings, default_settings, sizeof(default_settings));
    memcpy(conn->remote_settings, default_settings, sizeof(default_settings));
    
    /* Initialize streams */
    conn->stream_capacity = INITIAL_STREAM_CAPACITY;
    conn->streams = (HTTP2_STREAM**)calloc(conn->stream_capacity, sizeof(HTTP2_STREAM*));
    if (!conn->streams) {
        free(conn);
        return NULL;
    }
    conn->stream_count = 0;
    conn->next_stream_id = 2;  /* Server uses even IDs */
    conn->last_stream_id = 0;
    
    framework_log(LOG_LEVEL_DEBUG, "HTTP/2 connection created");
    return conn;
}

void http2_connection_destroy(HTTP2_CONNECTION *conn)
{
    if (!conn) return;
    
    if (conn->streams) {
        for (size_t i = 0; i < conn->stream_count; i++) {
            http2_stream_destroy(conn->streams[i]);
        }
        free(conn->streams);
    }
    
    free(conn);
    framework_log(LOG_LEVEL_DEBUG, "HTTP/2 connection destroyed");
}

HTTP2_STREAM* http2_stream_create(uint32_t stream_id)
{
    HTTP2_STREAM *stream = (HTTP2_STREAM*)calloc(1, sizeof(HTTP2_STREAM));
    if (!stream) return NULL;
    
    stream->stream_id = stream_id;
    stream->state = HTTP2_STREAM_IDLE;
    
    stream->header_capacity = INITIAL_HEADER_CAPACITY;
    stream->header_names = (char**)calloc(stream->header_capacity, sizeof(char*));
    stream->header_values = (char**)calloc(stream->header_capacity, sizeof(char*));
    if (!stream->header_names || !stream->header_values) {
        free(stream->header_names);
        free(stream->header_values);
        free(stream);
        return NULL;
    }
    stream->header_count = 0;
    
    stream->data = NULL;
    stream->data_length = 0;
    stream->data_capacity = 0;
    
    stream->end_stream = 0;
    stream->end_headers = 0;
    
    return stream;
}

void http2_stream_destroy(HTTP2_STREAM *stream)
{
    if (!stream) return;
    
    if (stream->header_names) {
        for (size_t i = 0; i < stream->header_count; i++) {
            free(stream->header_names[i]);
            free(stream->header_values[i]);
        }
        free(stream->header_names);
        free(stream->header_values);
    }
    
    if (stream->data) {
        free(stream->data);
    }
    
    free(stream);
}

HTTP2_STREAM* http2_connection_get_stream(HTTP2_CONNECTION *conn, uint32_t stream_id)
{
    if (!conn) return NULL;
    
    for (size_t i = 0; i < conn->stream_count; i++) {
        if (conn->streams[i]->stream_id == stream_id) {
            return conn->streams[i];
        }
    }
    
    return NULL;
}

int http2_parse_frame_header(const uint8_t *data, HTTP2_FRAME_HEADER *header)
{
    if (!data || !header) return -1;
    
    /* Length (3 bytes) */
    header->length = (data[0] << 16) | (data[1] << 8) | data[2];
    
    /* Type (1 byte) */
    header->type = data[3];
    
    /* Flags (1 byte) */
    header->flags = data[4];
    
    /* Stream ID (4 bytes, ignore R bit) */
    header->stream_id = ((data[5] & 0x7F) << 24) | (data[6] << 16) | 
                        (data[7] << 8) | data[8];
    
    return 0;
}

int http2_send_frame(HTTP2_CONNECTION *conn, HTTP2_FRAME_TYPE type, uint8_t flags,
                     uint32_t stream_id, const uint8_t *payload, uint32_t length)
{
    if (!conn) return -1;
    
    uint8_t header[9];
    
    /* Length (3 bytes) */
    header[0] = (length >> 16) & 0xFF;
    header[1] = (length >> 8) & 0xFF;
    header[2] = length & 0xFF;
    
    /* Type */
    header[3] = type;
    
    /* Flags */
    header[4] = flags;
    
    /* Stream ID (ignore R bit) */
    header[5] = (stream_id >> 24) & 0x7F;
    header[6] = (stream_id >> 16) & 0xFF;
    header[7] = (stream_id >> 8) & 0xFF;
    header[8] = stream_id & 0xFF;
    
    /* Send header */
    if (send(conn->socket, header, 9, 0) != 9) {
        return -1;
    }
    
    /* Send payload if any */
    if (payload && length > 0) {
        if (send(conn->socket, payload, length, 0) != (ssize_t)length) {
            return -1;
        }
    }
    
    return 0;
}

int http2_send_settings(HTTP2_CONNECTION *conn, int ack)
{
    if (!conn) return -1;
    
    if (ack) {
        /* Send empty SETTINGS frame with ACK flag */
        return http2_send_frame(conn, HTTP2_FRAME_SETTINGS, HTTP2_FLAG_ACK, 0, NULL, 0);
    }
    
    /* Send SETTINGS frame with our settings */
    uint8_t payload[36];  /* 6 settings * 6 bytes each */
    int offset = 0;
    
    /* HEADER_TABLE_SIZE */
    payload[offset++] = 0;
    payload[offset++] = HTTP2_SETTINGS_HEADER_TABLE_SIZE;
    payload[offset++] = (conn->local_settings[1] >> 24) & 0xFF;
    payload[offset++] = (conn->local_settings[1] >> 16) & 0xFF;
    payload[offset++] = (conn->local_settings[1] >> 8) & 0xFF;
    payload[offset++] = conn->local_settings[1] & 0xFF;
    
    /* ENABLE_PUSH */
    payload[offset++] = 0;
    payload[offset++] = HTTP2_SETTINGS_ENABLE_PUSH;
    payload[offset++] = (conn->local_settings[2] >> 24) & 0xFF;
    payload[offset++] = (conn->local_settings[2] >> 16) & 0xFF;
    payload[offset++] = (conn->local_settings[2] >> 8) & 0xFF;
    payload[offset++] = conn->local_settings[2] & 0xFF;
    
    /* MAX_CONCURRENT_STREAMS */
    payload[offset++] = 0;
    payload[offset++] = HTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
    payload[offset++] = (conn->local_settings[3] >> 24) & 0xFF;
    payload[offset++] = (conn->local_settings[3] >> 16) & 0xFF;
    payload[offset++] = (conn->local_settings[3] >> 8) & 0xFF;
    payload[offset++] = conn->local_settings[3] & 0xFF;
    
    /* INITIAL_WINDOW_SIZE */
    payload[offset++] = 0;
    payload[offset++] = HTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
    payload[offset++] = (conn->local_settings[4] >> 24) & 0xFF;
    payload[offset++] = (conn->local_settings[4] >> 16) & 0xFF;
    payload[offset++] = (conn->local_settings[4] >> 8) & 0xFF;
    payload[offset++] = conn->local_settings[4] & 0xFF;
    
    /* MAX_FRAME_SIZE */
    payload[offset++] = 0;
    payload[offset++] = HTTP2_SETTINGS_MAX_FRAME_SIZE;
    payload[offset++] = (conn->local_settings[5] >> 24) & 0xFF;
    payload[offset++] = (conn->local_settings[5] >> 16) & 0xFF;
    payload[offset++] = (conn->local_settings[5] >> 8) & 0xFF;
    payload[offset++] = conn->local_settings[5] & 0xFF;
    
    /* MAX_HEADER_LIST_SIZE */
    payload[offset++] = 0;
    payload[offset++] = HTTP2_SETTINGS_MAX_HEADER_LIST_SIZE;
    payload[offset++] = (conn->local_settings[6] >> 24) & 0xFF;
    payload[offset++] = (conn->local_settings[6] >> 16) & 0xFF;
    payload[offset++] = (conn->local_settings[6] >> 8) & 0xFF;
    payload[offset++] = conn->local_settings[6] & 0xFF;
    
    return http2_send_frame(conn, HTTP2_FRAME_SETTINGS, HTTP2_FLAG_NONE, 0, payload, offset);
}

int http2_send_goaway(HTTP2_CONNECTION *conn, HTTP2_ERROR_CODE error)
{
    if (!conn) return -1;
    
    uint8_t payload[8];
    
    /* Last Stream ID (4 bytes) */
    payload[0] = (conn->last_stream_id >> 24) & 0x7F;
    payload[1] = (conn->last_stream_id >> 16) & 0xFF;
    payload[2] = (conn->last_stream_id >> 8) & 0xFF;
    payload[3] = conn->last_stream_id & 0xFF;
    
    /* Error Code (4 bytes) */
    payload[4] = (error >> 24) & 0xFF;
    payload[5] = (error >> 16) & 0xFF;
    payload[6] = (error >> 8) & 0xFF;
    payload[7] = error & 0xFF;
    
    return http2_send_frame(conn, HTTP2_FRAME_GOAWAY, HTTP2_FLAG_NONE, 0, payload, 8);
}

int http2_send_window_update(HTTP2_CONNECTION *conn, uint32_t stream_id, uint32_t increment)
{
    if (!conn) return -1;
    
    uint8_t payload[4];
    
    /* Window Size Increment (4 bytes, ignore R bit) */
    payload[0] = (increment >> 24) & 0x7F;
    payload[1] = (increment >> 16) & 0xFF;
    payload[2] = (increment >> 8) & 0xFF;
    payload[3] = increment & 0xFF;
    
    return http2_send_frame(conn, HTTP2_FRAME_WINDOW_UPDATE, HTTP2_FLAG_NONE, 
                           stream_id, payload, 4);
}

/* Simplified HPACK encoding - just literal headers without compression */
int http2_encode_headers(const char **names, const char **values, size_t count,
                         uint8_t *output, size_t *output_len)
{
    if (!names || !values || !output || !output_len) return -1;
    
    size_t offset = 0;
    
    for (size_t i = 0; i < count; i++) {
        size_t name_len = strlen(names[i]);
        size_t value_len = strlen(values[i]);
        
        /* Literal Header Field without Indexing (0x00) */
        output[offset++] = 0x00;
        
        /* Name Length */
        output[offset++] = (uint8_t)name_len;
        
        /* Name */
        memcpy(output + offset, names[i], name_len);
        offset += name_len;
        
        /* Value Length */
        output[offset++] = (uint8_t)value_len;
        
        /* Value */
        memcpy(output + offset, values[i], value_len);
        offset += value_len;
    }
    
    *output_len = offset;
    return 0;
}

/* Simplified HPACK decoding */
int http2_decode_headers(const uint8_t *input, size_t input_len,
                         char ***names, char ***values, size_t *count)
{
    if (!input || !names || !values || !count) return -1;
    
    size_t capacity = 20;
    *names = (char**)calloc(capacity, sizeof(char*));
    *values = (char**)calloc(capacity, sizeof(char*));
    *count = 0;
    
    size_t offset = 0;
    while (offset < input_len) {
        /* Skip header representation byte */
        offset++;
        
        if (offset >= input_len) break;
        
        /* Name length */
        size_t name_len = input[offset++];
        if (offset + name_len > input_len) break;
        
        /* Name */
        (*names)[*count] = strndup((char*)input + offset, name_len);
        offset += name_len;
        
        if (offset >= input_len) break;
        
        /* Value length */
        size_t value_len = input[offset++];
        if (offset + value_len > input_len) break;
        
        /* Value */
        (*values)[*count] = strndup((char*)input + offset, value_len);
        offset += value_len;
        
        (*count)++;
        
        if (*count >= capacity) {
            capacity *= 2;
            *names = (char**)realloc(*names, capacity * sizeof(char*));
            *values = (char**)realloc(*values, capacity * sizeof(char*));
        }
    }
    
    return 0;
}

int http2_handle_connection(HTTP2_CONNECTION *conn, void *server)
{
    (void)server;  /* Will be used for routing */
    
    if (!conn) return -1;
    
    uint8_t buffer[MAX_FRAME_SIZE + 9];
    
    /* Check for HTTP/2 connection preface */
    ssize_t bytes = recv(conn->socket, buffer, HTTP2_PREFACE_LEN, MSG_PEEK);
    if (bytes == HTTP2_PREFACE_LEN) {
        if (memcmp(buffer, HTTP2_PREFACE, HTTP2_PREFACE_LEN) == 0) {
            /* Consume the preface */
            recv(conn->socket, buffer, HTTP2_PREFACE_LEN, 0);
            conn->is_http2 = 1;
            conn->preface_received = 1;
            
            framework_log(LOG_LEVEL_INFO, "HTTP/2 connection established");
            
            /* Send initial SETTINGS frame */
            http2_send_settings(conn, 0);
            
            return 0;
        }
    }
    
    /* Not HTTP/2 or invalid preface */
    return -1;
}
