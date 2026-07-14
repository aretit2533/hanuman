#ifndef SOCKETIO_H
#define SOCKETIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct _socketio_server_ SOCKETIO_SERVER;
typedef struct _socketio_client_ SOCKETIO_CLIENT;
typedef struct _socketio_socket_ SOCKETIO_SOCKET;

/* Socket.IO packet types */
typedef enum {
    SIO_PACKET_CONNECT = 0,
    SIO_PACKET_DISCONNECT = 1,
    SIO_PACKET_EVENT = 2,
    SIO_PACKET_ACK = 3,
    SIO_PACKET_CONNECT_ERROR = 4,
    SIO_PACKET_BINARY_EVENT = 5,
    SIO_PACKET_BINARY_ACK = 6
} SIO_PACKET_TYPE;

/* Socket.IO server configuration */
typedef struct {
    const char *host;
    int port;
    const char *path;                 /* Socket.IO endpoint (default: "/socket.io") */
    int max_connections;
    int ping_interval;                /* Ping interval in ms (default: 25000) */
    int ping_timeout;                 /* Ping timeout in ms (default: 20000) */
    bool allow_upgrades;              /* Allow transport upgrades (default: true) */
    const char *cors_origin;          /* CORS allowed origin (NULL for no CORS) */
} SIO_SERVER_CONFIG;

/* Socket.IO client configuration */
typedef struct {
    const char *url;                  /* Server URL (e.g., "http://localhost:3000") */
    const char *path;                 /* Socket.IO path (default: "/socket.io") */
    const char *namespace_path;       /* Namespace (default: "/") */
    bool auto_connect;                /* Auto-connect on creation */
    bool reconnection;                /* Enable auto-reconnection */
    int reconnection_attempts;        /* Max reconnection attempts (0 = infinite) */
    int reconnection_delay;           /* Initial reconnection delay in ms */
    int reconnection_delay_max;       /* Max reconnection delay in ms */
    const char *auth_token;           /* Authentication token */
} SIO_CLIENT_CONFIG;

/* Socket.IO event data */
typedef struct {
    char *event_name;
    char *data;                       /* JSON string or plain text */
    uint8_t *binary_data;             /* Binary data if any */
    size_t binary_length;
    int ack_id;                       /* Acknowledgement ID (-1 if none) */
} SIO_EVENT;

/* Socket.IO event callbacks */
typedef void (*sio_on_connect_callback)(SOCKETIO_SOCKET *socket, void *user_data);
typedef void (*sio_on_disconnect_callback)(SOCKETIO_SOCKET *socket, const char *reason, void *user_data);
typedef void (*sio_on_event_callback)(SOCKETIO_SOCKET *socket, SIO_EVENT *event, void *user_data);
typedef void (*sio_on_error_callback)(SOCKETIO_SOCKET *socket, const char *error, void *user_data);

typedef struct {
    sio_on_connect_callback on_connect;
    sio_on_disconnect_callback on_disconnect;
    sio_on_event_callback on_event;
    sio_on_error_callback on_error;
    void *user_data;
} SIO_CALLBACKS;

/*
 * ============================================================================
 * Socket.IO Server API
 * ============================================================================
 */

/**
 * Create a Socket.IO server
 * 
 * @param config Server configuration
 * @return Server instance or NULL on error
 */
SOCKETIO_SERVER* sio_server_create(SIO_SERVER_CONFIG *config);

/**
 * Set global event callbacks for the server
 * 
 * @param server Server instance
 * @param callbacks Event callbacks
 */
void sio_server_set_callbacks(SOCKETIO_SERVER *server, SIO_CALLBACKS *callbacks);

/**
 * Register an event handler
 * 
 * @param server Server instance
 * @param event_name Event name to handle
 * @param callback Callback for this specific event
 * @param user_data User data for the callback
 * @return 0 on success, -1 on error
 */
int sio_server_on(SOCKETIO_SERVER *server, const char *event_name,
                 sio_on_event_callback callback, void *user_data);

/**
 * Start the Socket.IO server
 * 
 * @param server Server instance
 * @return 0 on success, -1 on error
 */
int sio_server_start(SOCKETIO_SERVER *server);

/**
 * Stop the Socket.IO server
 * 
 * @param server Server instance
 */
void sio_server_stop(SOCKETIO_SERVER *server);

/**
 * Destroy the Socket.IO server
 * 
 * @param server Server instance
 */
void sio_server_destroy(SOCKETIO_SERVER *server);

/**
 * Broadcast event to all connected clients
 * 
 * @param server Server instance
 * @param event_name Event name
 * @param data Event data (JSON string)
 * @return Number of clients that received the event
 */
int sio_server_emit(SOCKETIO_SERVER *server, const char *event_name, const char *data);

/**
 * Broadcast to all clients in a room
 * 
 * @param server Server instance
 * @param room Room name
 * @param event_name Event name
 * @param data Event data (JSON string)
 * @return Number of clients that received the event
 */
int sio_server_to(SOCKETIO_SERVER *server, const char *room, 
                 const char *event_name, const char *data);

/**
 * Get number of connected clients
 * 
 * @param server Server instance
 * @return Number of connected clients
 */
int sio_server_get_client_count(SOCKETIO_SERVER *server);

/*
 * ============================================================================
 * Socket.IO Client API
 * ============================================================================
 */

/**
 * Create a Socket.IO client
 * 
 * @param config Client configuration
 * @return Client instance or NULL on error
 */
SOCKETIO_CLIENT* sio_client_create(SIO_CLIENT_CONFIG *config);

/**
 * Set event callbacks for the client
 * 
 * @param client Client instance
 * @param callbacks Event callbacks
 */
void sio_client_set_callbacks(SOCKETIO_CLIENT *client, SIO_CALLBACKS *callbacks);

/**
 * Register an event handler
 * 
 * @param client Client instance
 * @param event_name Event name to handle
 * @param callback Callback for this specific event
 * @param user_data User data for the callback
 * @return 0 on success, -1 on error
 */
int sio_client_on(SOCKETIO_CLIENT *client, const char *event_name,
                 sio_on_event_callback callback, void *user_data);

/**
 * Connect to Socket.IO server
 * 
 * @param client Client instance
 * @return 0 on success, -1 on error
 */
int sio_client_connect(SOCKETIO_CLIENT *client);

/**
 * Disconnect from Socket.IO server
 * 
 * @param client Client instance
 */
void sio_client_disconnect(SOCKETIO_CLIENT *client);

/**
 * Destroy the Socket.IO client
 * 
 * @param client Client instance
 */
void sio_client_destroy(SOCKETIO_CLIENT *client);

/**
 * Check if client is connected
 * 
 * @param client Client instance
 * @return true if connected, false otherwise
 */
bool sio_client_is_connected(SOCKETIO_CLIENT *client);

/**
 * Get the socket associated with this client
 * 
 * @param client Client instance
 * @return Socket instance or NULL
 */
SOCKETIO_SOCKET* sio_client_get_socket(SOCKETIO_CLIENT *client);

/*
 * ============================================================================
 * Socket.IO Socket API (Common to Server and Client)
 * ============================================================================
 */

/**
 * Emit an event
 * 
 * @param socket Socket instance
 * @param event_name Event name
 * @param data Event data (JSON string)
 * @return 0 on success, -1 on error
 */
int sio_emit(SOCKETIO_SOCKET *socket, const char *event_name, const char *data);

/**
 * Emit an event with acknowledgement
 * 
 * @param socket Socket instance
 * @param event_name Event name
 * @param data Event data (JSON string)
 * @param callback Callback for acknowledgement
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int sio_emit_with_ack(SOCKETIO_SOCKET *socket, const char *event_name, const char *data,
                     sio_on_event_callback callback, void *user_data);

/**
 * Emit a binary event
 * 
 * @param socket Socket instance
 * @param event_name Event name
 * @param data Binary data
 * @param length Data length
 * @return 0 on success, -1 on error
 */
int sio_emit_binary(SOCKETIO_SOCKET *socket, const char *event_name, 
                   const uint8_t *data, size_t length);

/**
 * Join a room (server-side only)
 * 
 * @param socket Socket instance
 * @param room Room name
 * @return 0 on success, -1 on error
 */
int sio_join(SOCKETIO_SOCKET *socket, const char *room);

/**
 * Leave a room (server-side only)
 * 
 * @param socket Socket instance
 * @param room Room name
 * @return 0 on success, -1 on error
 */
int sio_leave(SOCKETIO_SOCKET *socket, const char *room);

/**
 * Broadcast to all sockets in the same rooms except sender
 * 
 * @param socket Socket instance
 * @param event_name Event name
 * @param data Event data (JSON string)
 * @return Number of sockets that received the event
 */
int sio_broadcast(SOCKETIO_SOCKET *socket, const char *event_name, const char *data);

/**
 * Get socket ID
 * 
 * @param socket Socket instance
 * @return Socket ID string
 */
const char* sio_get_id(SOCKETIO_SOCKET *socket);

/**
 * Get socket user data
 * 
 * @param socket Socket instance
 * @return User data pointer
 */
void* sio_get_user_data(SOCKETIO_SOCKET *socket);

/**
 * Set socket user data
 * 
 * @param socket Socket instance
 * @param user_data User data pointer
 */
void sio_set_user_data(SOCKETIO_SOCKET *socket, void *user_data);

/**
 * Disconnect the socket
 * 
 * @param socket Socket instance
 * @param close_transport Whether to close the underlying transport
 */
void sio_disconnect(SOCKETIO_SOCKET *socket, bool close_transport);

#endif /* SOCKETIO_H */
