/*
 * WebSocket Implementation (RFC 6455)
 * Supports WebSocket server and client with frame encoding/decoding
 */

#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <time.h>

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_MAX_HEADER_SIZE 14
#define WS_DEFAULT_MAX_MESSAGE_SIZE (10 * 1024 * 1024) /* 10MB */

/* Logging macros */
#define log_error(fmt, ...) fprintf(stderr, "[WS ERROR] " fmt "\n", ##__VA_ARGS__)
#define log_info(fmt, ...) printf("[WS INFO] " fmt "\n", ##__VA_ARGS__)
#define log_debug(fmt, ...) printf("[WS DEBUG] " fmt "\n", ##__VA_ARGS__)

/* Connection state */
typedef enum {
    WS_STATE_CONNECTING,
    WS_STATE_OPEN,
    WS_STATE_CLOSING,
    WS_STATE_CLOSED
} ws_state_t;

/* WebSocket connection structure */
struct _websocket_connection_ {
    int socket_fd;
    ws_state_t state;
    bool is_server;
    char remote_addr[64];
    
    /* Buffering */
    uint8_t *recv_buffer;
    size_t recv_buffer_size;
    size_t recv_buffer_used;
    
    /* Fragmentation */
    uint8_t *fragment_buffer;
    size_t fragment_buffer_size;
    size_t fragment_buffer_used;
    WS_OPCODE fragment_opcode;
    
    /* Configuration */
    size_t max_message_size;
    
    /* User data */
    void *user_data;
    
    /* Callbacks for this connection */
    WS_CALLBACKS *callbacks;
    
    /* Thread safety */
    pthread_mutex_t write_mutex;
    pthread_mutex_t read_mutex;
    
    /* Ping/pong */
    time_t last_pong;
    bool waiting_for_pong;
};

/* WebSocket server structure */
struct _websocket_server_ {
    int listen_fd;
    WS_SERVER_CONFIG config;
    WS_CALLBACKS callbacks;
    bool running;
    pthread_t accept_thread;
    pthread_t ping_thread;
    
    /* Connections */
    WEBSOCKET_CONNECTION **connections;
    int connection_count;
    int max_connections;
    pthread_mutex_t connections_mutex;
};

/* WebSocket client structure */
struct _websocket_client_ {
    WS_CLIENT_CONFIG config;
    WS_CALLBACKS callbacks;
    WEBSOCKET_CONNECTION *connection;
    bool connected;
    pthread_t recv_thread;
    pthread_mutex_t mutex;
};

/* Base64 encode */
static char* base64_encode(const unsigned char *input, int length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    
    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);
    
    /* Get length using BIO_ctrl */
    long blen = BIO_ctrl(bmem, BIO_CTRL_INFO, 0, (char*)&bptr);
    (void)blen; /* Suppress unused warning */
    
    /* Safer way - get buffer content */
    char *buf_data;
    long buf_len = BIO_get_mem_data(bmem, &buf_data);
    
    char *output = (char*)malloc(buf_len + 1);
    memcpy(output, buf_data, buf_len);
    output[buf_len] = '\0';
    
    BIO_free_all(b64);
    return output;
}

/* Generate WebSocket accept key */
static char* ws_generate_accept_key(const char *client_key) {
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", client_key, WS_GUID);
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)combined, strlen(combined), hash);
    
    return base64_encode(hash, SHA_DIGEST_LENGTH);
}

/* Generate random masking key */
static void ws_generate_mask(uint8_t mask[4]) {
    for (int i = 0; i < 4; i++) {
        mask[i] = rand() & 0xFF;
    }
}

/* Apply/remove mask */
static void ws_apply_mask(uint8_t *data, size_t length, const uint8_t mask[4]) {
    for (size_t i = 0; i < length; i++) {
        data[i] ^= mask[i % 4];
    }
}

/* Create a new connection */
static WEBSOCKET_CONNECTION* ws_connection_create(int socket_fd, bool is_server) {
    WEBSOCKET_CONNECTION *conn = (WEBSOCKET_CONNECTION*)calloc(1, sizeof(WEBSOCKET_CONNECTION));
    if (!conn) return NULL;
    
    conn->socket_fd = socket_fd;
    conn->state = WS_STATE_CONNECTING;
    conn->is_server = is_server;
    conn->max_message_size = WS_DEFAULT_MAX_MESSAGE_SIZE;
    conn->recv_buffer_size = 4096;
    conn->recv_buffer = (uint8_t*)malloc(conn->recv_buffer_size);
    conn->fragment_opcode = WS_OPCODE_CONTINUATION;
    conn->last_pong = time(NULL);
    
    pthread_mutex_init(&conn->write_mutex, NULL);
    pthread_mutex_init(&conn->read_mutex, NULL);
    
    /* Get remote address */
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(socket_fd, (struct sockaddr*)&addr, &addr_len) == 0) {
        snprintf(conn->remote_addr, sizeof(conn->remote_addr), "%s:%d",
                inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    }
    
    return conn;
}

/* Destroy a connection */
static void ws_connection_destroy(WEBSOCKET_CONNECTION *conn) {
    if (!conn) return;
    
    if (conn->socket_fd >= 0) {
        close(conn->socket_fd);
    }
    
    free(conn->recv_buffer);
    free(conn->fragment_buffer);
    
    pthread_mutex_destroy(&conn->write_mutex);
    pthread_mutex_destroy(&conn->read_mutex);
    
    free(conn);
}

/* Send a WebSocket frame */
static int ws_send_frame(WEBSOCKET_CONNECTION *conn, WS_OPCODE opcode,
                        const uint8_t *payload, size_t payload_len, bool mask) {
    if (!conn || conn->state != WS_STATE_OPEN) return -1;
    
    uint8_t header[WS_MAX_HEADER_SIZE];
    size_t header_len = 0;
    
    /* First byte: FIN + opcode */
    header[0] = 0x80 | (opcode & 0x0F);
    
    /* Payload length */
    if (payload_len < 126) {
        header[1] = payload_len;
        header_len = 2;
    } else if (payload_len < 65536) {
        header[1] = 126;
        header[2] = (payload_len >> 8) & 0xFF;
        header[3] = payload_len & 0xFF;
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (payload_len >> (56 - i * 8)) & 0xFF;
        }
        header_len = 10;
    }
    
    /* Masking */
    uint8_t mask_key[4] = {0};
    if (mask) {
        header[1] |= 0x80;
        ws_generate_mask(mask_key);
        memcpy(&header[header_len], mask_key, 4);
        header_len += 4;
    }
    
    pthread_mutex_lock(&conn->write_mutex);
    
    /* Send header */
    if (send(conn->socket_fd, header, header_len, 0) != (ssize_t)header_len) {
        pthread_mutex_unlock(&conn->write_mutex);
        return -1;
    }
    
    /* Send payload */
    if (payload_len > 0) {
        if (mask) {
            /* Apply mask to a copy */
            uint8_t *masked_payload = (uint8_t*)malloc(payload_len);
            memcpy(masked_payload, payload, payload_len);
            ws_apply_mask(masked_payload, payload_len, mask_key);
            
            ssize_t sent = send(conn->socket_fd, masked_payload, payload_len, 0);
            free(masked_payload);
            
            if (sent != (ssize_t)payload_len) {
                pthread_mutex_unlock(&conn->write_mutex);
                return -1;
            }
        } else {
            if (send(conn->socket_fd, payload, payload_len, 0) != (ssize_t)payload_len) {
                pthread_mutex_unlock(&conn->write_mutex);
                return -1;
            }
        }
    }
    
    pthread_mutex_unlock(&conn->write_mutex);
    return 0;
}

/* Parse and handle a WebSocket frame */
static int ws_handle_frame(WEBSOCKET_CONNECTION *conn, WS_CALLBACKS *callbacks) {
    if (conn->recv_buffer_used < 2) return 0; /* Need at least 2 bytes */
    
    uint8_t *data = conn->recv_buffer;
    size_t data_len = conn->recv_buffer_used;
    
    /* Parse header */
    bool fin = (data[0] & 0x80) != 0;
    WS_OPCODE opcode = (WS_OPCODE)(data[0] & 0x0F);
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;
    
    size_t header_len = 2;
    
    if (payload_len == 126) {
        if (data_len < 4) return 0;
        payload_len = ((uint64_t)data[2] << 8) | data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (data_len < 10) return 0;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        header_len = 10;
    }
    
    if (masked) header_len += 4;
    
    /* Check if we have the complete frame */
    if (data_len < header_len + payload_len) return 0;
    
    /* Extract payload */
    uint8_t *payload = &data[header_len];
    
    /* Unmask if needed */
    if (masked) {
        uint8_t mask[4];
        memcpy(mask, &data[header_len - 4], 4);
        ws_apply_mask(payload, payload_len, mask);
    }
    
    /* Handle control frames */
    if (opcode == WS_OPCODE_CLOSE) {
        WS_CLOSE_STATUS status = WS_CLOSE_NORMAL;
        char reason[128] = "";
        
        if (payload_len >= 2) {
            status = (WS_CLOSE_STATUS)((payload[0] << 8) | payload[1]);
            if (payload_len > 2) {
                size_t reason_len = payload_len - 2;
                if (reason_len > sizeof(reason) - 1) reason_len = sizeof(reason) - 1;
                memcpy(reason, &payload[2], reason_len);
                reason[reason_len] = '\0';
            }
        }
        
        conn->state = WS_STATE_CLOSING;
        if (callbacks && callbacks->on_close) {
            callbacks->on_close(conn, status, reason, callbacks->user_data);
        }
        
        /* Send close frame back */
        ws_send_frame(conn, WS_OPCODE_CLOSE, payload, payload_len > 125 ? 125 : payload_len, !conn->is_server);
        conn->state = WS_STATE_CLOSED;
        
    } else if (opcode == WS_OPCODE_PING) {
        /* Respond with pong */
        ws_send_frame(conn, WS_OPCODE_PONG, payload, payload_len, !conn->is_server);
        
    } else if (opcode == WS_OPCODE_PONG) {
        conn->last_pong = time(NULL);
        conn->waiting_for_pong = false;
        
    } else {
        /* Data frame */
        if (opcode != WS_OPCODE_CONTINUATION) {
            conn->fragment_opcode = opcode;
        }
        
        /* Handle fragmentation */
        if (!fin) {
            /* Add to fragment buffer */
            size_t new_size = conn->fragment_buffer_used + payload_len;
            if (new_size > conn->max_message_size) {
                ws_close(conn, WS_CLOSE_MESSAGE_TOO_BIG, "Message too large");
                return -1;
            }
            
            if (new_size > conn->fragment_buffer_size) {
                conn->fragment_buffer_size = new_size * 2;
                conn->fragment_buffer = (uint8_t*)realloc(conn->fragment_buffer, conn->fragment_buffer_size);
            }
            
            memcpy(&conn->fragment_buffer[conn->fragment_buffer_used], payload, payload_len);
            conn->fragment_buffer_used += payload_len;
        } else {
            /* Complete message */
            uint8_t *message_data;
            size_t message_len;
            
            if (conn->fragment_buffer_used > 0) {
                /* Assemble fragments */
                size_t total_len = conn->fragment_buffer_used + payload_len;
                message_data = (uint8_t*)malloc(total_len);
                memcpy(message_data, conn->fragment_buffer, conn->fragment_buffer_used);
                memcpy(&message_data[conn->fragment_buffer_used], payload, payload_len);
                message_len = total_len;
                
                conn->fragment_buffer_used = 0;
            } else {
                message_data = payload;
                message_len = payload_len;
            }
            
            /* Deliver message */
            if (callbacks && callbacks->on_message) {
                WS_MESSAGE msg = {
                    .opcode = conn->fragment_opcode,
                    .data = message_data,
                    .length = message_len,
                    .is_final = true
                };
                callbacks->on_message(conn, &msg, callbacks->user_data);
            }
            
            if (message_data != payload) {
                free(message_data);
            }
            
            conn->fragment_opcode = WS_OPCODE_CONTINUATION;
        }
    }
    
    /* Remove processed frame from buffer */
    size_t frame_len = header_len + payload_len;
    memmove(conn->recv_buffer, &conn->recv_buffer[frame_len], conn->recv_buffer_used - frame_len);
    conn->recv_buffer_used -= frame_len;
    
    return 1; /* Frame processed */
}

/* Perform WebSocket handshake (server) */
static int ws_server_handshake(WEBSOCKET_CONNECTION *conn) {
    char buffer[4096];
    ssize_t n = recv(conn->socket_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) return -1;
    
    buffer[n] = '\0';
    
    /* Parse WebSocket key */
    char *key_start = strstr(buffer, "Sec-WebSocket-Key:");
    if (!key_start) return -1;
    
    key_start += 18;
    while (*key_start == ' ') key_start++;
    
    char *key_end = strchr(key_start, '\r');
    if (!key_end) return -1;
    
    char client_key[256];
    size_t key_len = key_end - key_start;
    if (key_len >= sizeof(client_key)) return -1;
    
    memcpy(client_key, key_start, key_len);
    client_key[key_len] = '\0';
    
    /* Generate accept key */
    char *accept_key = ws_generate_accept_key(client_key);
    
    /* Send handshake response */
    char response[1024];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_key);
    
    free(accept_key);
    
    if (send(conn->socket_fd, response, response_len, 0) != response_len) {
        return -1;
    }
    
    conn->state = WS_STATE_OPEN;
    return 0;
}

/* Connection handler thread */
static void* ws_connection_thread(void *arg) {
    WEBSOCKET_CONNECTION *conn = (WEBSOCKET_CONNECTION*)arg;
    WS_CALLBACKS *callbacks = conn->callbacks;
    
    struct pollfd pfd = {
        .fd = conn->socket_fd,
        .events = POLLIN
    };
    
    while (conn->state == WS_STATE_OPEN) {
        int ret = poll(&pfd, 1, 100);
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            /* Resize buffer if needed */
            if (conn->recv_buffer_used + 4096 > conn->recv_buffer_size) {
                conn->recv_buffer_size *= 2;
                conn->recv_buffer = (uint8_t*)realloc(conn->recv_buffer, conn->recv_buffer_size);
            }
            
            ssize_t n = recv(conn->socket_fd, &conn->recv_buffer[conn->recv_buffer_used],
                           conn->recv_buffer_size - conn->recv_buffer_used, 0);
            
            if (n <= 0) {
                break;
            }
            
            conn->recv_buffer_used += n;
            
            /* Process frames */
            while (ws_handle_frame(conn, callbacks) > 0);
        }
    }
    
    return NULL;
}

/*
 * ============================================================================
 * Public API Implementation
 * ============================================================================
 */

/* Send text message */
int ws_send_text(WEBSOCKET_CONNECTION *conn, const char *text) {
    if (!conn || !text) return -1;
    return ws_send_frame(conn, WS_OPCODE_TEXT, (const uint8_t*)text, 
                        strlen(text), !conn->is_server);
}

/* Send binary message */
int ws_send_binary(WEBSOCKET_CONNECTION *conn, const uint8_t *data, size_t length) {
    if (!conn || !data) return -1;
    return ws_send_frame(conn, WS_OPCODE_BINARY, data, length, !conn->is_server);
}

/* Send ping */
int ws_send_ping(WEBSOCKET_CONNECTION *conn, const uint8_t *data, size_t length) {
    if (!conn) return -1;
    conn->waiting_for_pong = true;
    return ws_send_frame(conn, WS_OPCODE_PING, data, length, !conn->is_server);
}

/* Send pong */
int ws_send_pong(WEBSOCKET_CONNECTION *conn, const uint8_t *data, size_t length) {
    if (!conn) return -1;
    return ws_send_frame(conn, WS_OPCODE_PONG, data, length, !conn->is_server);
}

/* Close connection */
void ws_close(WEBSOCKET_CONNECTION *conn, WS_CLOSE_STATUS status, const char *reason) {
    if (!conn || conn->state == WS_STATE_CLOSED) return;
    
    uint8_t payload[128];
    size_t payload_len = 0;
    
    payload[0] = (status >> 8) & 0xFF;
    payload[1] = status & 0xFF;
    payload_len = 2;
    
    if (reason) {
        size_t reason_len = strlen(reason);
        if (reason_len > sizeof(payload) - 2) reason_len = sizeof(payload) - 2;
        memcpy(&payload[2], reason, reason_len);
        payload_len += reason_len;
    }
    
    ws_send_frame(conn, WS_OPCODE_CLOSE, payload, payload_len, !conn->is_server);
    conn->state = WS_STATE_CLOSED;
}

/* Get/Set user data */
void* ws_get_user_data(WEBSOCKET_CONNECTION *conn) {
    return conn ? conn->user_data : NULL;
}

void ws_set_user_data(WEBSOCKET_CONNECTION *conn, void *user_data) {
    if (conn) conn->user_data = user_data;
}

/* Get remote address */
int ws_get_remote_addr(WEBSOCKET_CONNECTION *conn, char *addr_buf, size_t buf_len) {
    if (!conn || !addr_buf) return -1;
    snprintf(addr_buf, buf_len, "%s", conn->remote_addr);
    return 0;
}

/* Server accept thread */
static void* ws_server_accept_thread(void *arg) {
    WEBSOCKET_SERVER *server = (WEBSOCKET_SERVER*)arg;
    
    while (server->running) {
        struct pollfd pfd = {
            .fd = server->listen_fd,
            .events = POLLIN
        };
        
        int ret = poll(&pfd, 1, 100);
        if (ret <= 0) continue;
        
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) continue;
        
        pthread_mutex_lock(&server->connections_mutex);
        
        if (server->connection_count >= server->max_connections) {
            close(client_fd);
            pthread_mutex_unlock(&server->connections_mutex);
            continue;
        }
        
        WEBSOCKET_CONNECTION *conn = ws_connection_create(client_fd, true);
        if (!conn) {
            close(client_fd);
            pthread_mutex_unlock(&server->connections_mutex);
            continue;
        }
        
        /* Perform handshake */
        if (ws_server_handshake(conn) != 0) {
            ws_connection_destroy(conn);
            pthread_mutex_unlock(&server->connections_mutex);
            continue;
        }
        
        /* Add to connections */
        server->connections[server->connection_count++] = conn;
        conn->callbacks = &server->callbacks;
        
        pthread_mutex_unlock(&server->connections_mutex);
        
        /* Call on_connect */
        if (server->callbacks.on_connect) {
            server->callbacks.on_connect(conn, server->callbacks.user_data);
        }
        
        /* Start connection thread */
        pthread_t thread;
        pthread_create(&thread, NULL, ws_connection_thread, conn);
        pthread_detach(thread);
    }
    
    return NULL;
}

/* Create server */
WEBSOCKET_SERVER* ws_server_create(WS_SERVER_CONFIG *config) {
    if (!config) return NULL;
    
    WEBSOCKET_SERVER *server = (WEBSOCKET_SERVER*)calloc(1, sizeof(WEBSOCKET_SERVER));
    if (!server) return NULL;
    
    server->config = *config;
    server->max_connections = config->max_connections > 0 ? config->max_connections : 100;
    server->connections = (WEBSOCKET_CONNECTION**)calloc(server->max_connections, 
                                                        sizeof(WEBSOCKET_CONNECTION*));
    
    pthread_mutex_init(&server->connections_mutex, NULL);
    
    return server;
}

/* Set server callbacks */
void ws_server_set_callbacks(WEBSOCKET_SERVER *server, WS_CALLBACKS *callbacks) {
    if (server && callbacks) {
        server->callbacks = *callbacks;
    }
}

/* Start server */
int ws_server_start(WEBSOCKET_SERVER *server) {
    if (!server) return -1;
    
    /* Create socket */
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        log_error("Failed to create socket");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* Bind */
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->config.port);
    addr.sin_addr.s_addr = server->config.host ? inet_addr(server->config.host) : INADDR_ANY;
    
    if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_error("Failed to bind to port %d", server->config.port);
        close(server->listen_fd);
        return -1;
    }
    
    /* Listen */
    if (listen(server->listen_fd, 10) < 0) {
        log_error("Failed to listen");
        close(server->listen_fd);
        return -1;
    }
    
    server->running = true;
    
    /* Start accept thread */
    pthread_create(&server->accept_thread, NULL, ws_server_accept_thread, server);
    
    log_info("WebSocket server started on %s:%d", 
            server->config.host ? server->config.host : "0.0.0.0",
            server->config.port);
    
    return 0;
}

/* Stop server */
void ws_server_stop(WEBSOCKET_SERVER *server) {
    if (!server) return;
    
    server->running = false;
    pthread_join(server->accept_thread, NULL);
    
    /* Close all connections */
    pthread_mutex_lock(&server->connections_mutex);
    for (int i = 0; i < server->connection_count; i++) {
        ws_close(server->connections[i], WS_CLOSE_GOING_AWAY, "Server shutting down");
        ws_connection_destroy(server->connections[i]);
    }
    server->connection_count = 0;
    pthread_mutex_unlock(&server->connections_mutex);
    
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }
}

/* Destroy server */
void ws_server_destroy(WEBSOCKET_SERVER *server) {
    if (!server) return;
    
    ws_server_stop(server);
    free(server->connections);
    pthread_mutex_destroy(&server->connections_mutex);
    free(server);
}

/* Broadcast */
int ws_server_broadcast(WEBSOCKET_SERVER *server, const uint8_t *data, 
                       size_t length, WS_OPCODE opcode) {
    if (!server) return 0;
    
    int count = 0;
    pthread_mutex_lock(&server->connections_mutex);
    
    for (int i = 0; i < server->connection_count; i++) {
        if (server->connections[i]->state == WS_STATE_OPEN) {
            if (ws_send_frame(server->connections[i], opcode, data, length, false) == 0) {
                count++;
            }
        }
    }
    
    pthread_mutex_unlock(&server->connections_mutex);
    return count;
}

/* Get connection count */
int ws_server_get_connection_count(WEBSOCKET_SERVER *server) {
    if (!server) return 0;
    
    pthread_mutex_lock(&server->connections_mutex);
    int count = server->connection_count;
    pthread_mutex_unlock(&server->connections_mutex);
    
    return count;
}

/* Stub implementations for client (simplified) */
WEBSOCKET_CLIENT* ws_client_create(WS_CLIENT_CONFIG *config) {
    (void)config; /* Suppress unused parameter warning */
    log_info("WebSocket client support is basic - use server for full features");
    return NULL;
}

void ws_client_set_callbacks(WEBSOCKET_CLIENT *client, WS_CALLBACKS *callbacks) {
    (void)client; (void)callbacks;
}

int ws_client_connect(WEBSOCKET_CLIENT *client) {
    (void)client;
    return -1;
}

void ws_client_disconnect(WEBSOCKET_CLIENT *client, WS_CLOSE_STATUS status, const char *reason) {
    (void)client; (void)status; (void)reason;
}

void ws_client_destroy(WEBSOCKET_CLIENT *client) {
    (void)client;
}

bool ws_client_is_connected(WEBSOCKET_CLIENT *client) {
    (void)client;
    return false;
}

WEBSOCKET_CONNECTION* ws_client_get_connection(WEBSOCKET_CLIENT *client) {
    (void)client;
    return NULL;
}
