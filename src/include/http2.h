#ifndef HTTP2_H
#define HTTP2_H

#include <stdint.h>
#include <stddef.h>

/* HTTP/2 Protocol Constants */
#define HTTP2_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define HTTP2_PREFACE_LEN 24

/* HTTP/2 Frame Types */
typedef enum {
    HTTP2_FRAME_DATA = 0x0,
    HTTP2_FRAME_HEADERS = 0x1,
    HTTP2_FRAME_PRIORITY = 0x2,
    HTTP2_FRAME_RST_STREAM = 0x3,
    HTTP2_FRAME_SETTINGS = 0x4,
    HTTP2_FRAME_PUSH_PROMISE = 0x5,
    HTTP2_FRAME_PING = 0x6,
    HTTP2_FRAME_GOAWAY = 0x7,
    HTTP2_FRAME_WINDOW_UPDATE = 0x8,
    HTTP2_FRAME_CONTINUATION = 0x9
} HTTP2_FRAME_TYPE;

/* HTTP/2 Frame Flags */
#define HTTP2_FLAG_NONE         0x0
#define HTTP2_FLAG_ACK          0x1
#define HTTP2_FLAG_END_STREAM   0x1
#define HTTP2_FLAG_END_HEADERS  0x4
#define HTTP2_FLAG_PADDED       0x8
#define HTTP2_FLAG_PRIORITY     0x20

/* HTTP/2 Error Codes */
typedef enum {
    HTTP2_NO_ERROR = 0x0,
    HTTP2_PROTOCOL_ERROR = 0x1,
    HTTP2_INTERNAL_ERROR = 0x2,
    HTTP2_FLOW_CONTROL_ERROR = 0x3,
    HTTP2_SETTINGS_TIMEOUT = 0x4,
    HTTP2_STREAM_CLOSED_ERROR = 0x5,  /* Renamed to avoid conflict */
    HTTP2_FRAME_SIZE_ERROR = 0x6,
    HTTP2_REFUSED_STREAM = 0x7,
    HTTP2_CANCEL = 0x8,
    HTTP2_COMPRESSION_ERROR = 0x9,
    HTTP2_CONNECT_ERROR = 0xa,
    HTTP2_ENHANCE_YOUR_CALM = 0xb,
    HTTP2_INADEQUATE_SECURITY = 0xc,
    HTTP2_HTTP_1_1_REQUIRED = 0xd
} HTTP2_ERROR_CODE;

/* HTTP/2 Settings */
typedef enum {
    HTTP2_SETTINGS_HEADER_TABLE_SIZE = 0x1,
    HTTP2_SETTINGS_ENABLE_PUSH = 0x2,
    HTTP2_SETTINGS_MAX_CONCURRENT_STREAMS = 0x3,
    HTTP2_SETTINGS_INITIAL_WINDOW_SIZE = 0x4,
    HTTP2_SETTINGS_MAX_FRAME_SIZE = 0x5,
    HTTP2_SETTINGS_MAX_HEADER_LIST_SIZE = 0x6
} HTTP2_SETTING_ID;

/* HTTP/2 Frame Header (9 bytes) */
typedef struct {
    uint32_t length;        /* 24-bit length */
    uint8_t type;          /* Frame type */
    uint8_t flags;         /* Frame flags */
    uint32_t stream_id;    /* 31-bit stream identifier */
} HTTP2_FRAME_HEADER;

/* HTTP/2 Settings Frame Entry */
typedef struct {
    uint16_t identifier;
    uint32_t value;
} HTTP2_SETTING;

/* HTTP/2 Stream State */
typedef enum {
    HTTP2_STREAM_IDLE = 0,
    HTTP2_STREAM_OPEN,
    HTTP2_STREAM_RESERVED_LOCAL,
    HTTP2_STREAM_RESERVED_REMOTE,
    HTTP2_STREAM_HALF_CLOSED_LOCAL,
    HTTP2_STREAM_HALF_CLOSED_REMOTE,
    HTTP2_STREAM_CLOSED
} HTTP2_STREAM_STATE;

/* HTTP/2 Stream */
typedef struct _http2_stream_ {
    uint32_t stream_id;
    HTTP2_STREAM_STATE state;
    
    /* Headers */
    char **header_names;
    char **header_values;
    size_t header_count;
    size_t header_capacity;
    
    /* Data */
    uint8_t *data;
    size_t data_length;
    size_t data_capacity;
    
    int end_stream;
    int end_headers;
} HTTP2_STREAM;

/* HTTP/2 Connection */
typedef struct _http2_connection_ {
    int socket;
    int is_http2;
    int preface_received;
    
    /* Settings */
    uint32_t local_settings[7];
    uint32_t remote_settings[7];
    
    /* Streams */
    HTTP2_STREAM **streams;
    size_t stream_count;
    size_t stream_capacity;
    
    uint32_t next_stream_id;
    uint32_t last_stream_id;
} HTTP2_CONNECTION;

/* HTTP/2 Functions */
HTTP2_CONNECTION* http2_connection_create(int socket);
void http2_connection_destroy(HTTP2_CONNECTION *conn);

int http2_handle_connection(HTTP2_CONNECTION *conn, void *server);
int http2_parse_frame_header(const uint8_t *data, HTTP2_FRAME_HEADER *header);
int http2_send_frame(HTTP2_CONNECTION *conn, HTTP2_FRAME_TYPE type, uint8_t flags,
                     uint32_t stream_id, const uint8_t *payload, uint32_t length);

int http2_send_settings(HTTP2_CONNECTION *conn, int ack);
int http2_send_goaway(HTTP2_CONNECTION *conn, HTTP2_ERROR_CODE error);
int http2_send_window_update(HTTP2_CONNECTION *conn, uint32_t stream_id, uint32_t increment);

HTTP2_STREAM* http2_stream_create(uint32_t stream_id);
void http2_stream_destroy(HTTP2_STREAM *stream);
HTTP2_STREAM* http2_connection_get_stream(HTTP2_CONNECTION *conn, uint32_t stream_id);

/* HPACK (Header Compression) - Simplified */
int http2_encode_headers(const char **names, const char **values, size_t count,
                         uint8_t *output, size_t *output_len);
int http2_decode_headers(const uint8_t *input, size_t input_len,
                         char ***names, char ***values, size_t *count);

#endif /* HTTP2_H */
