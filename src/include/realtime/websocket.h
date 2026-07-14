#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>

/* Forward declarations */
typedef struct _websocket_server_ WEBSOCKET_SERVER;
typedef struct _websocket_client_ WEBSOCKET_CLIENT;
typedef struct _websocket_connection_ WEBSOCKET_CONNECTION;

/* WebSocket opcodes */
typedef enum {
    WS_OPCODE_CONTINUATION = 0x0,
    WS_OPCODE_TEXT = 0x1,
    WS_OPCODE_BINARY = 0x2,
    WS_OPCODE_CLOSE = 0x8,
    WS_OPCODE_PING = 0x9,
    WS_OPCODE_PONG = 0xA
} WS_OPCODE;

/* WebSocket close status codes */
typedef enum {
    WS_CLOSE_NORMAL = 1000,
    WS_CLOSE_GOING_AWAY = 1001,
    WS_CLOSE_PROTOCOL_ERROR = 1002,
    WS_CLOSE_UNSUPPORTED_DATA = 1003,
    WS_CLOSE_INVALID_FRAME = 1007,
    WS_CLOSE_POLICY_VIOLATION = 1008,
    WS_CLOSE_MESSAGE_TOO_BIG = 1009,
    WS_CLOSE_INTERNAL_ERROR = 1011
} WS_CLOSE_STATUS;

/* WebSocket server configuration */
typedef struct {
    const char *host;
    int port;
    const char *path;              /* WebSocket endpoint path (e.g., "/ws") */
    int max_connections;           /* Maximum concurrent connections */
    size_t max_message_size;       /* Maximum message size in bytes */
    int ping_interval;             /* Ping interval in seconds (0 to disable) */
    int ping_timeout;              /* Ping timeout in seconds */
} WS_SERVER_CONFIG;

/* WebSocket client configuration */
typedef struct {
    const char *url;               /* ws://host:port/path or wss://host:port/path */
    const char *origin;            /* Origin header */
    const char *protocols;         /* Comma-separated subprotocols */
    int connect_timeout;           /* Connection timeout in seconds */
    bool auto_reconnect;           /* Automatically reconnect on disconnect */
    int reconnect_interval;        /* Reconnect interval in seconds */
} WS_CLIENT_CONFIG;

/* WebSocket message */
typedef struct {
    WS_OPCODE opcode;
    uint8_t *data;
    size_t length;
    bool is_final;
} WS_MESSAGE;

/* WebSocket event callbacks */
typedef void (*ws_on_connect_callback)(WEBSOCKET_CONNECTION *conn, void *user_data);
typedef void (*ws_on_message_callback)(WEBSOCKET_CONNECTION *conn, WS_MESSAGE *message, void *user_data);
typedef void (*ws_on_close_callback)(WEBSOCKET_CONNECTION *conn, WS_CLOSE_STATUS status, const char *reason, void *user_data);
typedef void (*ws_on_error_callback)(WEBSOCKET_CONNECTION *conn, const char *error, void *user_data);

typedef struct {
    ws_on_connect_callback on_connect;
    ws_on_message_callback on_message;
    ws_on_close_callback on_close;
    ws_on_error_callback on_error;
    void *user_data;
} WS_CALLBACKS;

/*
 * ============================================================================
 * WebSocket Server API
 * ============================================================================
 */

/**
 * Create a WebSocket server
 * 
 * @param config Server configuration
 * @return Server instance or NULL on error
 */
WEBSOCKET_SERVER* ws_server_create(WS_SERVER_CONFIG *config);

/**
 * Set event callbacks for the server
 * 
 * @param server Server instance
 * @param callbacks Event callbacks
 */
void ws_server_set_callbacks(WEBSOCKET_SERVER *server, WS_CALLBACKS *callbacks);

/**
 * Start the WebSocket server
 * 
 * @param server Server instance
 * @return 0 on success, -1 on error
 */
int ws_server_start(WEBSOCKET_SERVER *server);

/**
 * Stop the WebSocket server
 * 
 * @param server Server instance
 */
void ws_server_stop(WEBSOCKET_SERVER *server);

/**
 * Destroy the WebSocket server
 * 
 * @param server Server instance
 */
void ws_server_destroy(WEBSOCKET_SERVER *server);

/**
 * Broadcast message to all connected clients
 * 
 * @param server Server instance
 * @param data Message data
 * @param length Message length
 * @param opcode Message opcode (TEXT or BINARY)
 * @return Number of clients that received the message
 */
int ws_server_broadcast(WEBSOCKET_SERVER *server, const uint8_t *data, 
                       size_t length, WS_OPCODE opcode);

/**
 * Get number of active connections
 * 
 * @param server Server instance
 * @return Number of active connections
 */
int ws_server_get_connection_count(WEBSOCKET_SERVER *server);

/*
 * ============================================================================
 * WebSocket Client API
 * ============================================================================
 */

/**
 * Create a WebSocket client
 * 
 * @param config Client configuration
 * @return Client instance or NULL on error
 */
WEBSOCKET_CLIENT* ws_client_create(WS_CLIENT_CONFIG *config);

/**
 * Set event callbacks for the client
 * 
 * @param client Client instance
 * @param callbacks Event callbacks
 */
void ws_client_set_callbacks(WEBSOCKET_CLIENT *client, WS_CALLBACKS *callbacks);

/**
 * Connect to WebSocket server
 * 
 * @param client Client instance
 * @return 0 on success, -1 on error
 */
int ws_client_connect(WEBSOCKET_CLIENT *client);

/**
 * Disconnect from WebSocket server
 * 
 * @param client Client instance
 * @param status Close status code
 * @param reason Close reason (can be NULL)
 */
void ws_client_disconnect(WEBSOCKET_CLIENT *client, WS_CLOSE_STATUS status, const char *reason);

/**
 * Destroy the WebSocket client
 * 
 * @param client Client instance
 */
void ws_client_destroy(WEBSOCKET_CLIENT *client);

/**
 * Check if client is connected
 * 
 * @param client Client instance
 * @return true if connected, false otherwise
 */
bool ws_client_is_connected(WEBSOCKET_CLIENT *client);

/**
 * Get the connection associated with this client
 * 
 * @param client Client instance
 * @return Connection instance or NULL
 */
WEBSOCKET_CONNECTION* ws_client_get_connection(WEBSOCKET_CLIENT *client);

/*
 * ============================================================================
 * WebSocket Connection API (Common to Server and Client)
 * ============================================================================
 */

/**
 * Send a text message
 * 
 * @param conn Connection instance
 * @param text Text message (UTF-8)
 * @return 0 on success, -1 on error
 */
int ws_send_text(WEBSOCKET_CONNECTION *conn, const char *text);

/**
 * Send a binary message
 * 
 * @param conn Connection instance
 * @param data Binary data
 * @param length Data length
 * @return 0 on success, -1 on error
 */
int ws_send_binary(WEBSOCKET_CONNECTION *conn, const uint8_t *data, size_t length);

/**
 * Send a ping frame
 * 
 * @param conn Connection instance
 * @param data Optional ping data
 * @param length Data length
 * @return 0 on success, -1 on error
 */
int ws_send_ping(WEBSOCKET_CONNECTION *conn, const uint8_t *data, size_t length);

/**
 * Send a pong frame
 * 
 * @param conn Connection instance
 * @param data Optional pong data
 * @param length Data length
 * @return 0 on success, -1 on error
 */
int ws_send_pong(WEBSOCKET_CONNECTION *conn, const uint8_t *data, size_t length);

/**
 * Close the connection
 * 
 * @param conn Connection instance
 * @param status Close status code
 * @param reason Close reason (can be NULL)
 */
void ws_close(WEBSOCKET_CONNECTION *conn, WS_CLOSE_STATUS status, const char *reason);

/**
 * Get connection user data
 * 
 * @param conn Connection instance
 * @return User data pointer
 */
void* ws_get_user_data(WEBSOCKET_CONNECTION *conn);

/**
 * Set connection user data
 * 
 * @param conn Connection instance
 * @param user_data User data pointer
 */
void ws_set_user_data(WEBSOCKET_CONNECTION *conn, void *user_data);

/**
 * Get connection remote address
 * 
 * @param conn Connection instance
 * @param addr_buf Buffer to store address
 * @param buf_len Buffer length
 * @return 0 on success, -1 on error
 */
int ws_get_remote_addr(WEBSOCKET_CONNECTION *conn, char *addr_buf, size_t buf_len);

#endif /* WEBSOCKET_H */
