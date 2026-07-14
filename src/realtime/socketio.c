/*
 * Socket.IO Implementation
 * Built on top of WebSocket with Engine.IO protocol support
 */

#define _POSIX_C_SOURCE 200809L

#include "socketio.h"
#include "websocket.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define SIO_PROTOCOL_VERSION 4
#define SIO_DEFAULT_PING_INTERVAL 25000
#define SIO_DEFAULT_PING_TIMEOUT 20000

/* Logging */
#define log_error(fmt, ...) fprintf(stderr, "[SIO ERROR] " fmt "\n", ##__VA_ARGS__)
#define log_info(fmt, ...) printf("[SIO INFO] " fmt "\n", ##__VA_ARGS__)
#define log_debug(fmt, ...) printf("[SIO DEBUG] " fmt "\n", ##__VA_ARGS__)

/* Socket state */
typedef enum {
    SIO_STATE_DISCONNECTED,
    SIO_STATE_CONNECTING,
    SIO_STATE_CONNECTED
} sio_state_t;

/* Event handler */
typedef struct sio_event_handler_ {
    char *event_name;
    sio_on_event_callback callback;
    void *user_data;
    struct sio_event_handler_ *next;
} sio_event_handler_t;

/* Room membership */
typedef struct sio_room_ {
    char *name;
    struct sio_room_ *next;
} sio_room_t;

/* Socket structure */
struct _socketio_socket_ {
    char id[64];
    WEBSOCKET_CONNECTION *ws_conn;
    sio_state_t state;
    bool is_server;
    
    /* Event handlers */
    sio_event_handler_t *handlers;
    pthread_mutex_t handlers_mutex;
    
    /* Rooms (server-side) */
    sio_room_t *rooms;
    pthread_mutex_t rooms_mutex;
    
    /* User data */
    void *user_data;
    
    /* Acknowledgements */
    int next_ack_id;
    pthread_mutex_t ack_mutex;
};

/* Server structure */
struct _socketio_server_ {
    WEBSOCKET_SERVER *ws_server;
    SIO_SERVER_CONFIG config;
    SIO_CALLBACKS callbacks;
    
    /* Sockets */
    SOCKETIO_SOCKET **sockets;
    int socket_count;
    int max_sockets;
    pthread_mutex_t sockets_mutex;
    
    /* Event handlers */
    sio_event_handler_t *handlers;
    pthread_mutex_t handlers_mutex;
};

/* Client structure */
struct _socketio_client_ {
    WEBSOCKET_CLIENT *ws_client;
    SIO_CLIENT_CONFIG config;
    SIO_CALLBACKS callbacks;
    SOCKETIO_SOCKET *socket;
    pthread_mutex_t mutex;
};

/* Generate unique socket ID */
static void sio_generate_id(char *id_buf, size_t buf_len) {
    snprintf(id_buf, buf_len, "socket_%ld_%d", time(NULL), rand());
}

/* Create Socket.IO packet */
static char* sio_create_packet(SIO_PACKET_TYPE type, const char *namespace_path,
                               const char *event_name, const char *data, int ack_id) {
    /* Format: <type>[namespace][ack_id]<data> */
    char *packet = (char*)malloc(4096);
    if (!packet) return NULL;
    
    int offset = 0;
    
    /* Packet type */
    offset += snprintf(packet + offset, 4096 - offset, "%d", type);
    
    /* Namespace */
    if (namespace_path && strcmp(namespace_path, "/") != 0) {
        offset += snprintf(packet + offset, 4096 - offset, "%s,", namespace_path);
    }
    
    /* Acknowledgement ID */
    if (ack_id >= 0) {
        offset += snprintf(packet + offset, 4096 - offset, "%d", ack_id);
    }
    
    /* Data */
    if (type == SIO_PACKET_EVENT && event_name) {
        /* Format: ["event_name", data] */
        if (data && data[0] == '[') {
            /* Data is already an array, insert event name */
            offset += snprintf(packet + offset, 4096 - offset, "[\"%s\",", event_name);
            /* Remove leading '[' from data */
            const char *data_content = data + 1;
            offset += snprintf(packet + offset, 4096 - offset, "%s", data_content);
        } else if (data) {
            offset += snprintf(packet + offset, 4096 - offset, "[\"%s\",%s]", event_name, data);
        } else {
            offset += snprintf(packet + offset, 4096 - offset, "[\"%s\"]", event_name);
        }
    } else if (data) {
        offset += snprintf(packet + offset, 4096 - offset, "%s", data);
    }
    
    return packet;
}

/* Parse Socket.IO packet */
static int sio_parse_packet(const char *packet_str, SIO_EVENT *event) {
    if (!packet_str || !event) return -1;
    
    memset(event, 0, sizeof(SIO_EVENT));
    event->ack_id = -1;
    
    /* Parse packet type */
    int type = packet_str[0] - '0';
    if (type < 0 || type > 6) return -1;
    
    const char *data_start = packet_str + 1;
    
    /* Skip namespace if present */
    if (*data_start == '/') {
        const char *comma = strchr(data_start, ',');
        if (comma) {
            data_start = comma + 1;
        }
    }
    
    /* Parse ack ID if present */
    if (*data_start >= '0' && *data_start <= '9') {
        event->ack_id = atoi(data_start);
        while (*data_start >= '0' && *data_start <= '9') data_start++;
    }
    
    /* Parse event data */
    if (type == SIO_PACKET_EVENT || type == SIO_PACKET_ACK) {
        /* Data should be JSON array: ["event_name", ...args] */
        if (*data_start == '[') {
            const char *event_start = strchr(data_start, '"');
            if (event_start) {
                event_start++;
                const char *event_end = strchr(event_start, '"');
                if (event_end) {
                    size_t event_len = event_end - event_start;
                    event->event_name = (char*)malloc(event_len + 1);
                    memcpy(event->event_name, event_start, event_len);
                    event->event_name[event_len] = '\0';
                    
                    /* Get event data (rest of array) */
                    const char *args_start = event_end + 1;
                    while (*args_start == ' ' || *args_start == ',') args_start++;
                    
                    if (*args_start != ']') {
                        /* Find matching closing bracket */
                        const char *args_end = strrchr(args_start, ']');
                        if (args_end) {
                            size_t args_len = args_end - args_start;
                            event->data = (char*)malloc(args_len + 1);
                            memcpy(event->data, args_start, args_len);
                            event->data[args_len] = '\0';
                        }
                    }
                }
            }
        }
    } else if (type == SIO_PACKET_CONNECT) {
        event->event_name = strdup("connect");
    } else if (type == SIO_PACKET_DISCONNECT) {
        event->event_name = strdup("disconnect");
    }
    
    return type;
}

/* Free event data */
static void sio_free_event(SIO_EVENT *event) {
    if (!event) return;
    free(event->event_name);
    free(event->data);
    free(event->binary_data);
    memset(event, 0, sizeof(SIO_EVENT));
}

/* Create a socket */
static SOCKETIO_SOCKET* sio_socket_create(WEBSOCKET_CONNECTION *ws_conn, bool is_server) {
    SOCKETIO_SOCKET *socket = (SOCKETIO_SOCKET*)calloc(1, sizeof(SOCKETIO_SOCKET));
    if (!socket) return NULL;
    
    sio_generate_id(socket->id, sizeof(socket->id));
    socket->ws_conn = ws_conn;
    socket->is_server = is_server;
    socket->state = SIO_STATE_CONNECTED;
    socket->next_ack_id = 0;
    
    pthread_mutex_init(&socket->handlers_mutex, NULL);
    pthread_mutex_init(&socket->rooms_mutex, NULL);
    pthread_mutex_init(&socket->ack_mutex, NULL);
    
    return socket;
}

/* Destroy socket */
static void sio_socket_destroy(SOCKETIO_SOCKET *socket) {
    if (!socket) return;
    
    /* Free event handlers */
    pthread_mutex_lock(&socket->handlers_mutex);
    sio_event_handler_t *handler = socket->handlers;
    while (handler) {
        sio_event_handler_t *next = handler->next;
        free(handler->event_name);
        free(handler);
        handler = next;
    }
    pthread_mutex_unlock(&socket->handlers_mutex);
    
    /* Free rooms */
    pthread_mutex_lock(&socket->rooms_mutex);
    sio_room_t *room = socket->rooms;
    while (room) {
        sio_room_t *next = room->next;
        free(room->name);
        free(room);
        room = next;
    }
    pthread_mutex_unlock(&socket->rooms_mutex);
    
    pthread_mutex_destroy(&socket->handlers_mutex);
    pthread_mutex_destroy(&socket->rooms_mutex);
    pthread_mutex_destroy(&socket->ack_mutex);
    
    free(socket);
}

/* Add event handler */
__attribute__((unused))
static int sio_add_handler(SOCKETIO_SOCKET *socket, const char *event_name,
                          sio_on_event_callback callback, void *user_data) {
    if (!socket || !event_name || !callback) return -1;
    
    sio_event_handler_t *handler = (sio_event_handler_t*)malloc(sizeof(sio_event_handler_t));
    handler->event_name = strdup(event_name);
    handler->callback = callback;
    handler->user_data = user_data;
    handler->next = NULL;
    
    pthread_mutex_lock(&socket->handlers_mutex);
    
    if (!socket->handlers) {
        socket->handlers = handler;
    } else {
        sio_event_handler_t *last = socket->handlers;
        while (last->next) last = last->next;
        last->next = handler;
    }
    
    pthread_mutex_unlock(&socket->handlers_mutex);
    return 0;
}

/* Find and invoke event handler */
static void sio_invoke_handler(SOCKETIO_SOCKET *socket, SIO_EVENT *event) {
    if (!socket || !event || !event->event_name) return;
    
    pthread_mutex_lock(&socket->handlers_mutex);
    
    sio_event_handler_t *handler = socket->handlers;
    while (handler) {
        if (strcmp(handler->event_name, event->event_name) == 0) {
            handler->callback(socket, event, handler->user_data);
            break;
        }
        handler = handler->next;
    }
    
    pthread_mutex_unlock(&socket->handlers_mutex);
}

/* WebSocket message handler for Socket.IO */
static void sio_on_ws_message(WEBSOCKET_CONNECTION *conn, WS_MESSAGE *msg, void *user_data) {
    SOCKETIO_SOCKET *socket = (SOCKETIO_SOCKET*)ws_get_user_data(conn);
    if (!socket) return;
    
    if (msg->opcode != WS_OPCODE_TEXT) return;
    
    /* Parse Socket.IO packet */
    char *packet_str = (char*)malloc(msg->length + 1);
    memcpy(packet_str, msg->data, msg->length);
    packet_str[msg->length] = '\0';
    
    SIO_EVENT event;
    int packet_type = sio_parse_packet(packet_str, &event);
    
    if (packet_type == SIO_PACKET_EVENT) {
        /* Invoke handler */
        sio_invoke_handler(socket, &event);
    } else if (packet_type == SIO_PACKET_CONNECT) {
        socket->state = SIO_STATE_CONNECTED;
    } else if (packet_type == SIO_PACKET_DISCONNECT) {
        socket->state = SIO_STATE_DISCONNECTED;
    }
    
    sio_free_event(&event);
    free(packet_str);
    
    (void)user_data;
}

/* WebSocket connect handler */
static void sio_on_ws_connect(WEBSOCKET_CONNECTION *conn, void *user_data) {
    SOCKETIO_SERVER *server = (SOCKETIO_SERVER*)user_data;
    if (!server) return;
    
    /* Create Socket.IO socket */
    SOCKETIO_SOCKET *socket = sio_socket_create(conn, true);
    if (!socket) return;
    
    /* Store socket in connection */
    ws_set_user_data(conn, socket);
    
    /* Add to server sockets */
    pthread_mutex_lock(&server->sockets_mutex);
    if (server->socket_count < server->max_sockets) {
        server->sockets[server->socket_count++] = socket;
    }
    pthread_mutex_unlock(&server->sockets_mutex);
    
    /* Send connect packet */
    char *packet = sio_create_packet(SIO_PACKET_CONNECT, "/", NULL, 
                                    "{\"sid\":\"socket_id\"}", -1);
    ws_send_text(conn, packet);
    free(packet);
    
    /* Call user callback */
    if (server->callbacks.on_connect) {
        server->callbacks.on_connect(socket, server->callbacks.user_data);
    }
}

/* WebSocket close handler */
static void sio_on_ws_close(WEBSOCKET_CONNECTION *conn, WS_CLOSE_STATUS status, 
                           const char *reason, void *user_data) {
    SOCKETIO_SOCKET *socket = (SOCKETIO_SOCKET*)ws_get_user_data(conn);
    if (!socket) return;
    
    socket->state = SIO_STATE_DISCONNECTED;
    
    SOCKETIO_SERVER *server = (SOCKETIO_SERVER*)user_data;
    if (server && server->callbacks.on_disconnect) {
        server->callbacks.on_disconnect(socket, reason ? reason : "connection closed", 
                                       server->callbacks.user_data);
    }
    
    /* Remove from server */
    if (server) {
        pthread_mutex_lock(&server->sockets_mutex);
        for (int i = 0; i < server->socket_count; i++) {
            if (server->sockets[i] == socket) {
                memmove(&server->sockets[i], &server->sockets[i+1],
                       (server->socket_count - i - 1) * sizeof(SOCKETIO_SOCKET*));
                server->socket_count--;
                break;
            }
        }
        pthread_mutex_unlock(&server->sockets_mutex);
    }
    
    sio_socket_destroy(socket);
    (void)status;
}

/*
 * ============================================================================
 * Public API Implementation
 * ============================================================================
 */

/* Create server */
SOCKETIO_SERVER* sio_server_create(SIO_SERVER_CONFIG *config) {
    if (!config) return NULL;
    
    SOCKETIO_SERVER *server = (SOCKETIO_SERVER*)calloc(1, sizeof(SOCKETIO_SERVER));
    if (!server) return NULL;
    
    server->config = *config;
    server->max_sockets = config->max_connections > 0 ? config->max_connections : 100;
    server->sockets = (SOCKETIO_SOCKET**)calloc(server->max_sockets, sizeof(SOCKETIO_SOCKET*));
    
    pthread_mutex_init(&server->sockets_mutex, NULL);
    pthread_mutex_init(&server->handlers_mutex, NULL);
    
    /* Create WebSocket server */
    WS_SERVER_CONFIG ws_config = {
        .host = config->host,
        .port = config->port,
        .path = config->path ? config->path : "/socket.io",
        .max_connections = config->max_connections,
        .max_message_size = 10 * 1024 * 1024,
        .ping_interval = config->ping_interval / 1000,
        .ping_timeout = config->ping_timeout / 1000
    };
    
    server->ws_server = ws_server_create(&ws_config);
    if (!server->ws_server) {
        free(server->sockets);
        free(server);
        return NULL;
    }
    
    /* Set WebSocket callbacks */
    WS_CALLBACKS ws_callbacks = {
        .on_connect = sio_on_ws_connect,
        .on_message = sio_on_ws_message,
        .on_close = sio_on_ws_close,
        .on_error = NULL,
        .user_data = server
    };
    ws_server_set_callbacks(server->ws_server, &ws_callbacks);
    
    return server;
}

/* Set callbacks */
void sio_server_set_callbacks(SOCKETIO_SERVER *server, SIO_CALLBACKS *callbacks) {
    if (server && callbacks) {
        server->callbacks = *callbacks;
    }
}

/* Register event handler */
int sio_server_on(SOCKETIO_SERVER *server, const char *event_name,
                 sio_on_event_callback callback, void *user_data) {
    if (!server || !event_name || !callback) return -1;
    
    sio_event_handler_t *handler = (sio_event_handler_t*)malloc(sizeof(sio_event_handler_t));
    handler->event_name = strdup(event_name);
    handler->callback = callback;
    handler->user_data = user_data;
    handler->next = NULL;
    
    pthread_mutex_lock(&server->handlers_mutex);
    
    if (!server->handlers) {
        server->handlers = handler;
    } else {
        sio_event_handler_t *last = server->handlers;
        while (last->next) last = last->next;
        last->next = handler;
    }
    
    pthread_mutex_unlock(&server->handlers_mutex);
    return 0;
}

/* Start server */
int sio_server_start(SOCKETIO_SERVER *server) {
    if (!server) return -1;
    
    int ret = ws_server_start(server->ws_server);
    if (ret == 0) {
        log_info("Socket.IO server started on %s:%d%s",
                server->config.host ? server->config.host : "0.0.0.0",
                server->config.port,
                server->config.path ? server->config.path : "/socket.io");
    }
    return ret;
}

/* Stop server */
void sio_server_stop(SOCKETIO_SERVER *server) {
    if (!server) return;
    ws_server_stop(server->ws_server);
}

/* Destroy server */
void sio_server_destroy(SOCKETIO_SERVER *server) {
    if (!server) return;
    
    sio_server_stop(server);
    
    /* Free event handlers */
    pthread_mutex_lock(&server->handlers_mutex);
    sio_event_handler_t *handler = server->handlers;
    while (handler) {
        sio_event_handler_t *next = handler->next;
        free(handler->event_name);
        free(handler);
        handler = next;
    }
    pthread_mutex_unlock(&server->handlers_mutex);
    
    ws_server_destroy(server->ws_server);
    free(server->sockets);
    pthread_mutex_destroy(&server->sockets_mutex);
    pthread_mutex_destroy(&server->handlers_mutex);
    free(server);
}

/* Emit to all clients */
int sio_server_emit(SOCKETIO_SERVER *server, const char *event_name, const char *data) {
    if (!server || !event_name) return 0;
    
    char *packet = sio_create_packet(SIO_PACKET_EVENT, "/", event_name, data, -1);
    int count = ws_server_broadcast(server->ws_server, (const uint8_t*)packet, 
                                    strlen(packet), WS_OPCODE_TEXT);
    free(packet);
    
    return count;
}

/* Emit to room */
int sio_server_to(SOCKETIO_SERVER *server, const char *room,
                 const char *event_name, const char *data) {
    if (!server || !room || !event_name) return 0;
    
    char *packet = sio_create_packet(SIO_PACKET_EVENT, "/", event_name, data, -1);
    int count = 0;
    
    pthread_mutex_lock(&server->sockets_mutex);
    
    for (int i = 0; i < server->socket_count; i++) {
        SOCKETIO_SOCKET *socket = server->sockets[i];
        
        /* Check if socket is in room */
        pthread_mutex_lock(&socket->rooms_mutex);
        sio_room_t *room_entry = socket->rooms;
        bool in_room = false;
        while (room_entry) {
            if (strcmp(room_entry->name, room) == 0) {
                in_room = true;
                break;
            }
            room_entry = room_entry->next;
        }
        pthread_mutex_unlock(&socket->rooms_mutex);
        
        if (in_room) {
            ws_send_text(socket->ws_conn, packet);
            count++;
        }
    }
    
    pthread_mutex_unlock(&server->sockets_mutex);
    free(packet);
    
    return count;
}

/* Get client count */
int sio_server_get_client_count(SOCKETIO_SERVER *server) {
    if (!server) return 0;
    
    pthread_mutex_lock(&server->sockets_mutex);
    int count = server->socket_count;
    pthread_mutex_unlock(&server->sockets_mutex);
    
    return count;
}

/* Socket API */
int sio_emit(SOCKETIO_SOCKET *socket, const char *event_name, const char *data) {
    if (!socket || !event_name) return -1;
    
    char *packet = sio_create_packet(SIO_PACKET_EVENT, "/", event_name, data, -1);
    int ret = ws_send_text(socket->ws_conn, packet);
    free(packet);
    
    return ret;
}

int sio_emit_with_ack(SOCKETIO_SOCKET *socket, const char *event_name, const char *data,
                     sio_on_event_callback callback, void *user_data) {
    if (!socket || !event_name) return -1;
    
    pthread_mutex_lock(&socket->ack_mutex);
    int ack_id = socket->next_ack_id++;
    pthread_mutex_unlock(&socket->ack_mutex);
    
    /* TODO: Store callback for when ack is received */
    
    char *packet = sio_create_packet(SIO_PACKET_EVENT, "/", event_name, data, ack_id);
    int ret = ws_send_text(socket->ws_conn, packet);
    free(packet);
    
    (void)callback;
    (void)user_data;
    
    return ret;
}

int sio_emit_binary(SOCKETIO_SOCKET *socket, const char *event_name,
                   const uint8_t *data, size_t length) {
    if (!socket || !event_name) return -1;
    
    /* For binary, we send a BINARY_EVENT packet */
    char *packet = sio_create_packet(SIO_PACKET_BINARY_EVENT, "/", event_name, NULL, -1);
    ws_send_text(socket->ws_conn, packet);
    free(packet);
    
    /* Then send the binary data */
    return ws_send_binary(socket->ws_conn, data, length);
}

int sio_join(SOCKETIO_SOCKET *socket, const char *room) {
    if (!socket || !room) return -1;
    
    pthread_mutex_lock(&socket->rooms_mutex);
    
    /* Check if already in room */
    sio_room_t *entry = socket->rooms;
    while (entry) {
        if (strcmp(entry->name, room) == 0) {
            pthread_mutex_unlock(&socket->rooms_mutex);
            return 0;
        }
        entry = entry->next;
    }
    
    /* Add to room */
    sio_room_t *new_room = (sio_room_t*)malloc(sizeof(sio_room_t));
    new_room->name = strdup(room);
    new_room->next = socket->rooms;
    socket->rooms = new_room;
    
    pthread_mutex_unlock(&socket->rooms_mutex);
    return 0;
}

int sio_leave(SOCKETIO_SOCKET *socket, const char *room) {
    if (!socket || !room) return -1;
    
    pthread_mutex_lock(&socket->rooms_mutex);
    
    sio_room_t **entry_ptr = &socket->rooms;
    while (*entry_ptr) {
        if (strcmp((*entry_ptr)->name, room) == 0) {
            sio_room_t *to_free = *entry_ptr;
            *entry_ptr = to_free->next;
            free(to_free->name);
            free(to_free);
            pthread_mutex_unlock(&socket->rooms_mutex);
            return 0;
        }
        entry_ptr = &(*entry_ptr)->next;
    }
    
    pthread_mutex_unlock(&socket->rooms_mutex);
    return -1;
}

int sio_broadcast(SOCKETIO_SOCKET *socket, const char *event_name, const char *data) {
    /* TODO: Implement broadcast to all sockets in same rooms except sender */
    (void)socket;
    (void)event_name;
    (void)data;
    return 0;
}

const char* sio_get_id(SOCKETIO_SOCKET *socket) {
    return socket ? socket->id : NULL;
}

void* sio_get_user_data(SOCKETIO_SOCKET *socket) {
    return socket ? socket->user_data : NULL;
}

void sio_set_user_data(SOCKETIO_SOCKET *socket, void *user_data) {
    if (socket) socket->user_data = user_data;
}

void sio_disconnect(SOCKETIO_SOCKET *socket, bool close_transport) {
    if (!socket) return;
    
    socket->state = SIO_STATE_DISCONNECTED;
    
    if (close_transport) {
        ws_close(socket->ws_conn, WS_CLOSE_NORMAL, "Socket.IO disconnect");
    }
}

/* Client stubs */
SOCKETIO_CLIENT* sio_client_create(SIO_CLIENT_CONFIG *config) {
    log_info("Socket.IO client support is basic - use server for full features");
    (void)config;
    return NULL;
}

void sio_client_set_callbacks(SOCKETIO_CLIENT *client, SIO_CALLBACKS *callbacks) {
    (void)client; (void)callbacks;
}

int sio_client_on(SOCKETIO_CLIENT *client, const char *event_name,
                 sio_on_event_callback callback, void *user_data) {
    (void)client; (void)event_name; (void)callback; (void)user_data;
    return -1;
}

int sio_client_connect(SOCKETIO_CLIENT *client) {
    (void)client;
    return -1;
}

void sio_client_disconnect(SOCKETIO_CLIENT *client) {
    (void)client;
}

void sio_client_destroy(SOCKETIO_CLIENT *client) {
    (void)client;
}

bool sio_client_is_connected(SOCKETIO_CLIENT *client) {
    (void)client;
    return false;
}

SOCKETIO_SOCKET* sio_client_get_socket(SOCKETIO_CLIENT *client) {
    (void)client;
    return NULL;
}
