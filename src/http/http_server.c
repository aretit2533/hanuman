#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "http_server.h"
#include "http_route.h"
#include "http2.h"
#include "application.h"
#include "framework.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define INITIAL_ROUTE_CAPACITY 20
#define INITIAL_HEADER_CAPACITY 10
#define INITIAL_BODY_CAPACITY 4096
#define MAX_REQUEST_SIZE 65536
#define MAX_EPOLL_EVENTS 64
#define EPOLL_TIMEOUT_MS 1000

/* Forward declarations */
static int serve_static_file(HTTP_SERVER *server, const char *url_path, HTTP_RESPONSE *response);

/* Connection state for tracking each client */
typedef struct _connection_state_ {
    int socket;
    char buffer[MAX_REQUEST_SIZE];
    size_t buffer_used;
    int is_http2;
    HTTP2_CONNECTION *http2_conn;
    time_t last_activity;
} CONNECTION_STATE;

/* HTTP Server Creation */
HTTP_SERVER* http_server_create(const char *host, int port)
{
    HTTP_SERVER *server = (HTTP_SERVER*)calloc(1, sizeof(HTTP_SERVER));
    if (!server) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate HTTP server");
        return NULL;
    }
    
    if (host) {
        strncpy(server->host, host, sizeof(server->host) - 1);
    } else {
        strcpy(server->host, "0.0.0.0");
    }
    
    server->port = port;
    server->server_socket = -1;
    server->running = 0;
    server->app = NULL;
    server->context = NULL;
    
    server->route_capacity = INITIAL_ROUTE_CAPACITY;
    server->routes = (HTTP_ROUTE**)calloc(server->route_capacity, sizeof(HTTP_ROUTE*));
    if (!server->routes) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate routes array");
        free(server);
        return NULL;
    }
    server->route_count = 0;
    
    /* Initialize epoll for concurrent connections */
    server->epoll_fd = -1;
    server->max_connections = 1000;
    server->connection_states = NULL;
    server->connection_count = 0;
    
    /* Initialize static file serving */
    server->static_enabled = 0;
    strcpy(server->default_file, "index.html");
    server->static_url_path[0] = '\0';
    server->static_directory[0] = '\0';
    
    framework_log(LOG_LEVEL_INFO, "HTTP server created on %s:%d", server->host, server->port);
    framework_log(LOG_LEVEL_INFO, "EPOLL-based concurrent connection handling enabled");
    return server;
}

void http_server_destroy(HTTP_SERVER *server)
{
    if (!server) return;
    
    if (server->running) {
        http_server_stop(server);
    }
    
    if (server->routes) {
        for (size_t i = 0; i < server->route_count; i++) {
            http_route_destroy(server->routes[i]);
        }
        free(server->routes);
    }
    
    /* Clean up connection states */
    if (server->connection_states) {
        free(server->connection_states);
    }
    
    free(server);
    framework_log(LOG_LEVEL_INFO, "HTTP server destroyed");
}

/* Set socket to non-blocking mode */
static int set_nonblocking(int socket_fd)
{
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
}

/* Find connection state by socket */
static CONNECTION_STATE* find_connection(HTTP_SERVER *server, int socket_fd)
{
    for (size_t i = 0; i < server->connection_count; i++) {
        if (server->connection_states[i].socket == socket_fd) {
            return &server->connection_states[i];
        }
    }
    return NULL;
}

/* Add new connection */
static CONNECTION_STATE* add_connection(HTTP_SERVER *server, int socket_fd)
{
    if (server->connection_count >= server->max_connections) {
        framework_log(LOG_LEVEL_WARNING, "Max connections reached, rejecting new connection");
        return NULL;
    }
    
    if (!server->connection_states) {
        server->connection_states = (CONNECTION_STATE*)calloc(server->max_connections, 
                                                              sizeof(CONNECTION_STATE));
        if (!server->connection_states) {
            return NULL;
        }
    }
    
    CONNECTION_STATE *conn = &server->connection_states[server->connection_count++];
    conn->socket = socket_fd;
    conn->buffer_used = 0;
    conn->is_http2 = 0;
    conn->http2_conn = NULL;
    conn->last_activity = time(NULL);
    
    return conn;
}

/* Remove connection */
static void remove_connection(HTTP_SERVER *server, int socket_fd)
{
    for (size_t i = 0; i < server->connection_count; i++) {
        if (server->connection_states[i].socket == socket_fd) {
            /* Clean up HTTP/2 connection if exists */
            if (server->connection_states[i].http2_conn) {
                http2_connection_destroy(server->connection_states[i].http2_conn);
            }
            
            close(socket_fd);
            
            /* Move last connection to this slot */
            if (i < server->connection_count - 1) {
                server->connection_states[i] = server->connection_states[server->connection_count - 1];
            }
            server->connection_count--;
            return;
        }
    }
}

int http_server_start(HTTP_SERVER *server)
{
    if (!server) return FRAMEWORK_ERROR_NULL_PTR;
    if (server->running) return FRAMEWORK_ERROR_STATE;
    
    /* Create socket */
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        framework_log(LOG_LEVEL_ERROR, "Failed to create socket: %s", strerror(errno));
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        framework_log(LOG_LEVEL_WARNING, "Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    /* Bind socket */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(server->port);
    
    if (strcmp(server->host, "0.0.0.0") == 0) {
        address.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, server->host, &address.sin_addr) <= 0) {
            framework_log(LOG_LEVEL_ERROR, "Invalid host address: %s", server->host);
            close(server->server_socket);
            return FRAMEWORK_ERROR_INVALID;
        }
    }
    
    if (bind(server->server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        framework_log(LOG_LEVEL_ERROR, "Failed to bind socket: %s", strerror(errno));
        close(server->server_socket);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Listen */
    if (listen(server->server_socket, 128) < 0) {
        framework_log(LOG_LEVEL_ERROR, "Failed to listen on socket: %s", strerror(errno));
        close(server->server_socket);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Set server socket to non-blocking */
    if (set_nonblocking(server->server_socket) < 0) {
        framework_log(LOG_LEVEL_ERROR, "Failed to set non-blocking mode: %s", strerror(errno));
        close(server->server_socket);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Create epoll instance */
    server->epoll_fd = epoll_create1(0);
    if (server->epoll_fd < 0) {
        framework_log(LOG_LEVEL_ERROR, "Failed to create epoll: %s", strerror(errno));
        close(server->server_socket);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Add server socket to epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server->server_socket;
    if (epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->server_socket, &ev) < 0) {
        framework_log(LOG_LEVEL_ERROR, "Failed to add server socket to epoll: %s", strerror(errno));
        close(server->epoll_fd);
        close(server->server_socket);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    server->running = 1;
    framework_log(LOG_LEVEL_INFO, "HTTP server listening on %s:%d", server->host, server->port);
    framework_log(LOG_LEVEL_INFO, "Registered %zu routes", server->route_count);
    framework_log(LOG_LEVEL_INFO, "Using EPOLL for concurrent connections (max: %zu)", server->max_connections);
    
    return FRAMEWORK_SUCCESS;
}

int http_server_stop(HTTP_SERVER *server)
{
    if (!server) return FRAMEWORK_ERROR_NULL_PTR;
    if (!server->running) return FRAMEWORK_ERROR_STATE;
    
    server->running = 0;
    
    /* Close all client connections */
    if (server->connection_states) {
        for (size_t i = 0; i < server->connection_count; i++) {
            if (server->connection_states[i].http2_conn) {
                http2_connection_destroy(server->connection_states[i].http2_conn);
            }
            close(server->connection_states[i].socket);
        }
        server->connection_count = 0;
    }
    
    /* Close epoll */
    if (server->epoll_fd >= 0) {
        close(server->epoll_fd);
        server->epoll_fd = -1;
    }
    
    /* Close server socket */
    if (server->server_socket >= 0) {
        close(server->server_socket);
        server->server_socket = -1;
    }
    
    framework_log(LOG_LEVEL_INFO, "HTTP server stopped");
    return FRAMEWORK_SUCCESS;
}

/* URL decode a string */
static void url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/* Parse query string into parameters */
static void parse_query_string(HTTP_REQUEST *request)
{
    if (!request || strlen(request->query_string) == 0) {
        return;
    }
    
    char query_copy[2048];
    strncpy(query_copy, request->query_string, sizeof(query_copy) - 1);
    
    char *saveptr;
    char *pair = strtok_r(query_copy, "&", &saveptr);
    
    while (pair) {
        char *equals = strchr(pair, '=');
        if (equals) {
            *equals = '\0';
            char *key = pair;
            char *value = equals + 1;
            
            /* URL decode both key and value */
            char decoded_key[128];
            char decoded_value[512];
            url_decode(decoded_key, key, sizeof(decoded_key));
            url_decode(decoded_value, value, sizeof(decoded_value));
            
            http_request_add_query_param(request, decoded_key, decoded_value);
        }
        pair = strtok_r(NULL, "&", &saveptr);
    }
}

/* Parse HTTP request from buffer */
static int parse_http_request(const char *buffer, size_t buffer_len, HTTP_REQUEST *request)
{
    char *buf_copy = strndup(buffer, buffer_len);
    if (!buf_copy) return FRAMEWORK_ERROR_MEMORY;
    
    char *line = strtok(buf_copy, "\r\n");
    if (!line) {
        free(buf_copy);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Parse request line: METHOD PATH HTTP/VERSION */
    char method_str[16], path[512], version[16];
    if (sscanf(line, "%15s %511s %15s", method_str, path, version) != 3) {
        free(buf_copy);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    request->method = http_method_from_string(method_str);
    strncpy(request->path, path, sizeof(request->path) - 1);
    strncpy(request->http_version, version, sizeof(request->http_version) - 1);
    
    /* Parse query string if present */
    char *query = strchr(request->path, '?');
    if (query) {
        *query = '\0';
        query++;
        strncpy(request->query_string, query, sizeof(request->query_string) - 1);
        parse_query_string(request);
    }
    
    /* Parse headers */
    while ((line = strtok(NULL, "\r\n")) != NULL) {
        if (strlen(line) == 0) break;  /* End of headers */
        
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *value = colon + 1;
            while (*value == ' ') value++;  /* Skip leading spaces */
            http_request_add_header(request, line, value);
        }
    }
    
    /* Body is whatever remains after headers */
    char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        size_t body_len = buffer_len - (body_start - buffer);
        if (body_len > 0) {
            request->body = (char*)malloc(body_len + 1);
            if (request->body) {
                memcpy(request->body, body_start, body_len);
                request->body[body_len] = '\0';
                request->body_length = body_len;
            }
        }
    }
    
    free(buf_copy);
    return FRAMEWORK_SUCCESS;
}

/* Build HTTP response string */
static char* build_http_response(HTTP_RESPONSE *response, size_t *response_len)
{
    size_t capacity = 1024;
    char *buffer = (char*)malloc(capacity);
    if (!buffer) return NULL;
    
    size_t offset = 0;
    
    /* Status line */
    int n = snprintf(buffer + offset, capacity - offset, "HTTP/1.1 %d %s\r\n",
                    response->status, 
                    strlen(response->status_message) > 0 ? 
                        response->status_message : http_status_to_string(response->status));
    offset += n;
    
    /* Headers */
    for (size_t i = 0; i < response->header_count; i++) {
        n = snprintf(buffer + offset, capacity - offset, "%s: %s\r\n",
                    response->headers[i].name, response->headers[i].value);
        offset += n;
        
        if (offset >= capacity - 512) {
            capacity *= 2;
            char *new_buf = (char*)realloc(buffer, capacity);
            if (!new_buf) {
                free(buffer);
                return NULL;
            }
            buffer = new_buf;
        }
    }
    
    /* Content-Length if body present */
    if (response->body && response->body_length > 0) {
        n = snprintf(buffer + offset, capacity - offset, "Content-Length: %zu\r\n", 
                    response->body_length);
        offset += n;
    }
    
    /* End of headers */
    n = snprintf(buffer + offset, capacity - offset, "\r\n");
    offset += n;
    
    /* Body */
    if (response->body && response->body_length > 0) {
        while (offset + response->body_length >= capacity) {
            capacity *= 2;
            char *new_buf = (char*)realloc(buffer, capacity);
            if (!new_buf) {
                free(buffer);
                return NULL;
            }
            buffer = new_buf;
        }
        memcpy(buffer + offset, response->body, response->body_length);
        offset += response->body_length;
    }
    
    *response_len = offset;
    return buffer;
}

/* Process HTTP/1.1 request from connection buffer */
static void process_http_request(HTTP_SERVER *server, CONNECTION_STATE *conn)
{
    /* Check if we have a complete HTTP request (ends with \r\n\r\n) */
    if (conn->buffer_used < 4) return;
    
    char *end_of_headers = strstr(conn->buffer, "\r\n\r\n");
    if (!end_of_headers) {
        /* Request not complete yet */
        if (conn->buffer_used >= MAX_REQUEST_SIZE - 1) {
            /* Request too large */
            framework_log(LOG_LEVEL_ERROR, "Request too large, closing connection");
            remove_connection(server, conn->socket);
        }
        return;
    }
    
    size_t request_len = conn->buffer_used;
    
    /* Parse request */
    HTTP_REQUEST *request = http_request_create();
    if (!request) {
        remove_connection(server, conn->socket);
        return;
    }
    
    if (parse_http_request(conn->buffer, request_len, request) != FRAMEWORK_SUCCESS) {
        framework_log(LOG_LEVEL_ERROR, "Failed to parse HTTP request");
        http_request_destroy(request);
        remove_connection(server, conn->socket);
        return;
    }
    
    /* Start timing */
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    /* Create response */
    HTTP_RESPONSE *response = http_response_create();
    if (!response) {
        http_request_destroy(request);
        remove_connection(server, conn->socket);
        return;
    }
    
    /* Try route handlers first */
    HTTP_ROUTE *matched_route = NULL;
    for (size_t i = 0; i < server->route_count; i++) {
        if (http_route_matches(server->routes[i], request->method, request->path)) {
            matched_route = server->routes[i];
            /* Extract path parameters from the matched route */
            http_route_extract_params(server->routes[i], request->path, request);
            break;
        }
    }
    
    if (matched_route && matched_route->handler) {
        /* Call route handler */
        matched_route->handler(request, response, matched_route->user_data);
    } else if (serve_static_file(server, request->path, response) < 0) {
        /* Not a route or static file - 404 Not Found */
        http_response_set_status(response, HTTP_STATUS_NOT_FOUND);
        http_response_set_text(response, "404 Not Found");
    }
    
    /* Build and send response */
    size_t response_len;
    char *response_str = build_http_response(response, &response_len);
    if (response_str) {
        send(conn->socket, response_str, response_len, 0);
        free(response_str);
    }
    
    /* Calculate processing time */
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
    
    /* Log completion with status code and timing */
    framework_log(LOG_LEVEL_INFO, "%s %s - %d (%.2fms)",
                 http_method_to_string(request->method), 
                 request->path,
                 response->status,
                 elapsed_ms);
    
    http_request_destroy(request);
    http_response_destroy(response);
    
    /* Close connection after response (HTTP/1.1 without keep-alive) */
    remove_connection(server, conn->socket);
}

/* Handle data received on client connection */
static void handle_client_data(HTTP_SERVER *server, int client_socket)
{
    CONNECTION_STATE *conn = find_connection(server, client_socket);
    if (!conn) return;
    
    /* Read available data */
    ssize_t bytes_read = recv(client_socket, 
                             conn->buffer + conn->buffer_used,
                             MAX_REQUEST_SIZE - conn->buffer_used - 1, 
                             0);
    
    if (bytes_read <= 0) {
        /* Connection closed or error */
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;  /* Would block, try again later */
        }
        remove_connection(server, client_socket);
        return;
    }
    
    conn->buffer_used += bytes_read;
    conn->buffer[conn->buffer_used] = '\0';
    conn->last_activity = time(NULL);
    
    /* Check for HTTP/2 connection preface on first data */
    if (conn->buffer_used >= HTTP2_PREFACE_LEN && !conn->is_http2 && 
        memcmp(conn->buffer, HTTP2_PREFACE, HTTP2_PREFACE_LEN) == 0) {
        framework_log(LOG_LEVEL_INFO, "HTTP/2 connection detected");
        conn->is_http2 = 1;
        conn->http2_conn = http2_connection_create(client_socket);
        if (conn->http2_conn) {
            http2_handle_connection(conn->http2_conn, server);
        }
        remove_connection(server, client_socket);
        return;
    }
    
    /* Process HTTP/1.1 request */
    if (!conn->is_http2) {
        process_http_request(server, conn);
    }
}

/* Accept new client connection */
static void accept_new_connection(HTTP_SERVER *server)
{
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server->server_socket, 
                                   (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* No more connections to accept */
                break;
            }
            framework_log(LOG_LEVEL_ERROR, "Failed to accept connection: %s", 
                        strerror(errno));
            break;
        }
        
        /* Set client socket to non-blocking */
        if (set_nonblocking(client_socket) < 0) {
            framework_log(LOG_LEVEL_ERROR, "Failed to set client non-blocking: %s", 
                        strerror(errno));
            close(client_socket);
            continue;
        }
        
        /* Add connection to tracking */
        CONNECTION_STATE *conn = add_connection(server, client_socket);
        if (!conn) {
            close(client_socket);
            continue;
        }
        
        /* Add to epoll */
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;  /* Edge-triggered */
        ev.data.fd = client_socket;
        if (epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, client_socket, &ev) < 0) {
            framework_log(LOG_LEVEL_ERROR, "Failed to add client to epoll: %s", 
                        strerror(errno));
            remove_connection(server, client_socket);
            continue;
        }
        
        framework_log(LOG_LEVEL_DEBUG, "Accepted new connection (socket %d, total: %zu)",
                     client_socket, server->connection_count);
    }
}

int http_server_run(HTTP_SERVER *server)
{
    if (!server || !server->running) {
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "HTTP server running with EPOLL, press Ctrl+C to stop...");
    framework_log(LOG_LEVEL_INFO, "Handling multiple concurrent connections...");
    
    struct epoll_event events[MAX_EPOLL_EVENTS];
    
    while (server->running) {
        /* Wait for events */
        int nfds = epoll_wait(server->epoll_fd, events, MAX_EPOLL_EVENTS, EPOLL_TIMEOUT_MS);
        
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted by signal */
            }
            framework_log(LOG_LEVEL_ERROR, "epoll_wait failed: %s", strerror(errno));
            break;
        }
        
        /* Process events */
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            if (fd == server->server_socket) {
                /* New connection */
                accept_new_connection(server);
            } else {
                /* Data on client socket */
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    /* Error or hangup */
                    remove_connection(server, fd);
                } else if (events[i].events & EPOLLIN) {
                    /* Data available to read */
                    handle_client_data(server, fd);
                }
            }
        }
        
        /* Optional: Clean up idle connections (timeout after 60 seconds) */
        time_t now = time(NULL);
        for (size_t i = 0; i < server->connection_count; ) {
            if (now - server->connection_states[i].last_activity > 60) {
                framework_log(LOG_LEVEL_DEBUG, "Closing idle connection (socket %d)",
                            server->connection_states[i].socket);
                int socket = server->connection_states[i].socket;
                remove_connection(server, socket);
                /* Don't increment i, as remove_connection shifts the array */
            } else {
                i++;
            }
        }
    }
    
    framework_log(LOG_LEVEL_INFO, "HTTP server event loop terminated");
    return FRAMEWORK_SUCCESS;
}

/* Route Management */
int http_server_add_route(HTTP_SERVER *server, HTTP_METHOD method, const char *path,
                         http_route_handler_fn handler, void *user_data)
{
    if (!server || !path || !handler) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    /* Resize array if needed */
    if (server->route_count >= server->route_capacity) {
        size_t new_capacity = server->route_capacity * 2;
        HTTP_ROUTE **new_routes = (HTTP_ROUTE**)realloc(server->routes, 
                                                         new_capacity * sizeof(HTTP_ROUTE*));
        if (!new_routes) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        server->routes = new_routes;
        server->route_capacity = new_capacity;
    }
    
    HTTP_ROUTE *route = http_route_create(method, path, handler, user_data);
    if (!route) {
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    server->routes[server->route_count++] = route;
    
    framework_log(LOG_LEVEL_DEBUG, "Route registered: %s %s", 
                 http_method_to_string(method), path);
    
    return FRAMEWORK_SUCCESS;
}

int http_server_get(HTTP_SERVER *server, const char *path, 
                   http_route_handler_fn handler, void *user_data)
{
    return http_server_add_route(server, HTTP_METHOD_GET, path, handler, user_data);
}

int http_server_post(HTTP_SERVER *server, const char *path,
                    http_route_handler_fn handler, void *user_data)
{
    return http_server_add_route(server, HTTP_METHOD_POST, path, handler, user_data);
}

int http_server_put(HTTP_SERVER *server, const char *path,
                   http_route_handler_fn handler, void *user_data)
{
    return http_server_add_route(server, HTTP_METHOD_PUT, path, handler, user_data);
}

int http_server_delete(HTTP_SERVER *server, const char *path,
                      http_route_handler_fn handler, void *user_data)
{
    return http_server_add_route(server, HTTP_METHOD_DELETE, path, handler, user_data);
}

int http_server_patch(HTTP_SERVER *server, const char *path,
                     http_route_handler_fn handler, void *user_data)
{
    return http_server_add_route(server, HTTP_METHOD_PATCH, path, handler, user_data);
}

/* HTTP Request Functions */
HTTP_REQUEST* http_request_create(void)
{
    HTTP_REQUEST *request = (HTTP_REQUEST*)calloc(1, sizeof(HTTP_REQUEST));
    if (!request) return NULL;
    
    request->method = HTTP_METHOD_UNKNOWN;
    request->header_capacity = INITIAL_HEADER_CAPACITY;
    request->headers = (HTTP_HEADER*)calloc(request->header_capacity, sizeof(HTTP_HEADER));
    if (!request->headers) {
        free(request);
        return NULL;
    }
    request->header_count = 0;
    request->body = NULL;
    request->body_length = 0;
    request->user_data = NULL;
    
    /* Initialize path parameters */
    request->path_param_capacity = 10;
    request->path_params = (HTTP_PARAM*)calloc(request->path_param_capacity, sizeof(HTTP_PARAM));
    if (!request->path_params) {
        free(request->headers);
        free(request);
        return NULL;
    }
    request->path_param_count = 0;
    
    /* Initialize query parameters */
    request->query_param_capacity = 20;
    request->query_params = (HTTP_PARAM*)calloc(request->query_param_capacity, sizeof(HTTP_PARAM));
    if (!request->query_params) {
        free(request->path_params);
        free(request->headers);
        free(request);
        return NULL;
    }
    request->query_param_count = 0;
    
    return request;
}

void http_request_destroy(HTTP_REQUEST *request)
{
    if (!request) return;
    
    if (request->headers) {
        free(request->headers);
    }
    if (request->body) {
        free(request->body);
    }
    if (request->path_params) {
        free(request->path_params);
    }
    if (request->query_params) {
        free(request->query_params);
    }
    free(request);
}

int http_request_add_header(HTTP_REQUEST *request, const char *name, const char *value)
{
    if (!request || !name || !value) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (request->header_count >= request->header_capacity) {
        size_t new_capacity = request->header_capacity * 2;
        HTTP_HEADER *new_headers = (HTTP_HEADER*)realloc(request->headers,
                                                          new_capacity * sizeof(HTTP_HEADER));
        if (!new_headers) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        request->headers = new_headers;
        request->header_capacity = new_capacity;
    }
    
    strncpy(request->headers[request->header_count].name, name, 
            sizeof(request->headers[0].name) - 1);
    strncpy(request->headers[request->header_count].value, value,
            sizeof(request->headers[0].value) - 1);
    request->header_count++;
    
    return FRAMEWORK_SUCCESS;
}

const char* http_request_get_header(HTTP_REQUEST *request, const char *name)
{
    if (!request || !name) return NULL;
    
    for (size_t i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i].name, name) == 0) {
            return request->headers[i].value;
        }
    }
    
    return NULL;
}

/* Path Parameter Functions */
const char* http_request_get_path_param(HTTP_REQUEST *request, const char *name)
{
    if (!request || !name) return NULL;
    
    for (size_t i = 0; i < request->path_param_count; i++) {
        if (strcmp(request->path_params[i].name, name) == 0) {
            return request->path_params[i].value;
        }
    }
    
    return NULL;
}

int http_request_add_path_param(HTTP_REQUEST *request, const char *name, const char *value)
{
    if (!request || !name || !value) {
        return FRAMEWORK_ERROR_INVALID;
    }
    
    // Resize if needed
    if (request->path_param_count >= request->path_param_capacity) {
        size_t new_capacity = request->path_param_capacity * 2;
        HTTP_PARAM *new_params = (HTTP_PARAM*)realloc(request->path_params,
                                                       new_capacity * sizeof(HTTP_PARAM));
        if (!new_params) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        request->path_params = new_params;
        request->path_param_capacity = new_capacity;
    }
    
    strncpy(request->path_params[request->path_param_count].name, name, 
            sizeof(request->path_params[0].name) - 1);
    strncpy(request->path_params[request->path_param_count].value, value,
            sizeof(request->path_params[0].value) - 1);
    request->path_param_count++;
    
    return FRAMEWORK_SUCCESS;
}

/* Query Parameter Functions */
const char* http_request_get_query_param(HTTP_REQUEST *request, const char *name)
{
    if (!request || !name) return NULL;
    
    for (size_t i = 0; i < request->query_param_count; i++) {
        if (strcmp(request->query_params[i].name, name) == 0) {
            return request->query_params[i].value;
        }
    }
    
    return NULL;
}

int http_request_add_query_param(HTTP_REQUEST *request, const char *name, const char *value)
{
    if (!request || !name || !value) {
        return FRAMEWORK_ERROR_INVALID;
    }
    
    // Resize if needed
    if (request->query_param_count >= request->query_param_capacity) {
        size_t new_capacity = request->query_param_capacity * 2;
        HTTP_PARAM *new_params = (HTTP_PARAM*)realloc(request->query_params,
                                                       new_capacity * sizeof(HTTP_PARAM));
        if (!new_params) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        request->query_params = new_params;
        request->query_param_capacity = new_capacity;
    }
    
    strncpy(request->query_params[request->query_param_count].name, name, 
            sizeof(request->query_params[0].name) - 1);
    strncpy(request->query_params[request->query_param_count].value, value,
            sizeof(request->query_params[0].value) - 1);
    request->query_param_count++;
    
    return FRAMEWORK_SUCCESS;
}

/* HTTP Response Functions */
HTTP_RESPONSE* http_response_create(void)
{
    HTTP_RESPONSE *response = (HTTP_RESPONSE*)calloc(1, sizeof(HTTP_RESPONSE));
    if (!response) return NULL;
    
    response->status = HTTP_STATUS_OK;
    strcpy(response->status_message, "OK");
    
    response->header_capacity = INITIAL_HEADER_CAPACITY;
    response->headers = (HTTP_HEADER*)calloc(response->header_capacity, sizeof(HTTP_HEADER));
    if (!response->headers) {
        free(response);
        return NULL;
    }
    response->header_count = 0;
    
    response->body_capacity = INITIAL_BODY_CAPACITY;
    response->body = (char*)malloc(response->body_capacity);
    if (!response->body) {
        free(response->headers);
        free(response);
        return NULL;
    }
    response->body_length = 0;
    
    /* Add default headers */
    http_response_add_header(response, "Server", "Equinox/1.0 (HTTP/1.1, HTTP/2)");
    http_response_add_header(response, "Connection", "close");
    
    return response;
}

void http_response_destroy(HTTP_RESPONSE *response)
{
    if (!response) return;
    
    if (response->headers) {
        free(response->headers);
    }
    if (response->body) {
        free(response->body);
    }
    free(response);
}

int http_response_set_status(HTTP_RESPONSE *response, HTTP_STATUS status)
{
    if (!response) return FRAMEWORK_ERROR_NULL_PTR;
    
    response->status = status;
    strncpy(response->status_message, http_status_to_string(status),
            sizeof(response->status_message) - 1);
    
    return FRAMEWORK_SUCCESS;
}

int http_response_add_header(HTTP_RESPONSE *response, const char *name, const char *value)
{
    if (!response || !name || !value) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (response->header_count >= response->header_capacity) {
        size_t new_capacity = response->header_capacity * 2;
        HTTP_HEADER *new_headers = (HTTP_HEADER*)realloc(response->headers,
                                                          new_capacity * sizeof(HTTP_HEADER));
        if (!new_headers) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        response->headers = new_headers;
        response->header_capacity = new_capacity;
    }
    
    strncpy(response->headers[response->header_count].name, name,
            sizeof(response->headers[0].name) - 1);
    strncpy(response->headers[response->header_count].value, value,
            sizeof(response->headers[0].value) - 1);
    response->header_count++;
    
    return FRAMEWORK_SUCCESS;
}

int http_response_set_body(HTTP_RESPONSE *response, const char *body, size_t length)
{
    if (!response || !body) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (length >= response->body_capacity) {
        size_t new_capacity = length + 1;
        char *new_body = (char*)realloc(response->body, new_capacity);
        if (!new_body) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        response->body = new_body;
        response->body_capacity = new_capacity;
    }
    
    memcpy(response->body, body, length);
    response->body[length] = '\0';
    response->body_length = length;
    
    return FRAMEWORK_SUCCESS;
}

int http_response_set_json(HTTP_RESPONSE *response, const char *json)
{
    if (!response || !json) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    http_response_add_header(response, "Content-Type", "application/json");
    return http_response_set_body(response, json, strlen(json));
}

int http_response_set_text(HTTP_RESPONSE *response, const char *text)
{
    if (!response || !text) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    http_response_add_header(response, "Content-Type", "text/plain");
    return http_response_set_body(response, text, strlen(text));
}

/* Utility Functions */
const char* http_method_to_string(HTTP_METHOD method)
{
    switch (method) {
        case HTTP_METHOD_GET: return "GET";
        case HTTP_METHOD_POST: return "POST";
        case HTTP_METHOD_PUT: return "PUT";
        case HTTP_METHOD_DELETE: return "DELETE";
        case HTTP_METHOD_PATCH: return "PATCH";
        case HTTP_METHOD_HEAD: return "HEAD";
        case HTTP_METHOD_OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

HTTP_METHOD http_method_from_string(const char *method)
{
    if (strcmp(method, "GET") == 0) return HTTP_METHOD_GET;
    if (strcmp(method, "POST") == 0) return HTTP_METHOD_POST;
    if (strcmp(method, "PUT") == 0) return HTTP_METHOD_PUT;
    if (strcmp(method, "DELETE") == 0) return HTTP_METHOD_DELETE;
    if (strcmp(method, "PATCH") == 0) return HTTP_METHOD_PATCH;
    if (strcmp(method, "HEAD") == 0) return HTTP_METHOD_HEAD;
    if (strcmp(method, "OPTIONS") == 0) return HTTP_METHOD_OPTIONS;
    return HTTP_METHOD_UNKNOWN;
}

const char* http_status_to_string(HTTP_STATUS status)
{
    switch (status) {
        case HTTP_STATUS_OK: return "OK";
        case HTTP_STATUS_CREATED: return "Created";
        case HTTP_STATUS_ACCEPTED: return "Accepted";
        case HTTP_STATUS_NO_CONTENT: return "No Content";
        case HTTP_STATUS_BAD_REQUEST: return "Bad Request";
        case HTTP_STATUS_UNAUTHORIZED: return "Unauthorized";
        case HTTP_STATUS_FORBIDDEN: return "Forbidden";
        case HTTP_STATUS_NOT_FOUND: return "Not Found";
        case HTTP_STATUS_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_STATUS_INTERNAL_ERROR: return "Internal Server Error";
        case HTTP_STATUS_NOT_IMPLEMENTED: return "Not Implemented";
        case HTTP_STATUS_SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}


/* MIME type detection */
const char* http_get_mime_type(const char *filename)
{
    if (!filename) return "application/octet-stream";
    
    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    
    ext++; /* Skip the dot */
    
    /* Text files */
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) return "text/html";
    if (strcasecmp(ext, "css") == 0) return "text/css";
    if (strcasecmp(ext, "js") == 0) return "application/javascript";
    if (strcasecmp(ext, "json") == 0) return "application/json";
    if (strcasecmp(ext, "xml") == 0) return "application/xml";
    if (strcasecmp(ext, "txt") == 0) return "text/plain";
    
    /* Images */
    if (strcasecmp(ext, "png") == 0) return "image/png";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "gif") == 0) return "image/gif";
    if (strcasecmp(ext, "svg") == 0) return "image/svg+xml";
    if (strcasecmp(ext, "ico") == 0) return "image/x-icon";
    
    /* Fonts */
    if (strcasecmp(ext, "woff") == 0) return "font/woff";
    if (strcasecmp(ext, "woff2") == 0) return "font/woff2";
    if (strcasecmp(ext, "ttf") == 0) return "font/ttf";
    
    return "application/octet-stream";
}

/* Static file serving */
int http_server_add_static_path(HTTP_SERVER *server, const char *url_path, const char *directory_path)
{
    if (!server || !url_path || !directory_path) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (access(directory_path, R_OK) != 0) {
        framework_log(LOG_LEVEL_ERROR, "Static directory not accessible: %s", directory_path);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    strncpy(server->static_url_path, url_path, sizeof(server->static_url_path) - 1);
    strncpy(server->static_directory, directory_path, sizeof(server->static_directory) - 1);
    server->static_enabled = 1;
    
    framework_log(LOG_LEVEL_INFO, "Static files: %s -> %s", url_path, directory_path);
    return FRAMEWORK_SUCCESS;
}

void http_server_set_default_file(HTTP_SERVER *server, const char *filename)
{
    if (server && filename) {
        strncpy(server->default_file, filename, sizeof(server->default_file) - 1);
    }
}

static int serve_static_file(HTTP_SERVER *server, const char *url_path, HTTP_RESPONSE *response)
{
    if (!server->static_enabled) return -1;
    
    size_t static_path_len = strlen(server->static_url_path);
    if (strncmp(url_path, server->static_url_path, static_path_len) != 0) return -1;
    
    const char *relative_path = url_path + static_path_len;
    if (*relative_path == '/') relative_path++;
    
    char file_path[1024];
    if (*relative_path == '\0' || relative_path[strlen(relative_path) - 1] == '/') {
        snprintf(file_path, sizeof(file_path), "%s/%s%s",
                server->static_directory, relative_path, server->default_file);
    } else {
        snprintf(file_path, sizeof(file_path), "%s/%s",
                server->static_directory, relative_path);
    }
    
    if (strstr(file_path, "..") != NULL) {
        framework_log(LOG_LEVEL_WARNING, "Directory traversal blocked: %s", url_path);
        http_response_set_status(response, HTTP_STATUS_FORBIDDEN);
        http_response_set_text(response, "403 Forbidden");
        return 0;
    }
    
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        http_response_set_status(response, HTTP_STATUS_NOT_FOUND);
        http_response_set_text(response, "404 Not Found");
        return 0;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0 || file_size > 10 * 1024 * 1024) {
        fclose(file);
        http_response_set_status(response, HTTP_STATUS_INTERNAL_ERROR);
        http_response_set_text(response, "File too large");
        return 0;
    }
    
    char *content = (char*)malloc(file_size);
    if (!content) {
        fclose(file);
        http_response_set_status(response, HTTP_STATUS_INTERNAL_ERROR);
        return 0;
    }
    
    size_t read_size = fread(content, 1, file_size, file);
    fclose(file);
    
    if (read_size != (size_t)file_size) {
        free(content);
        http_response_set_status(response, HTTP_STATUS_INTERNAL_ERROR);
        return 0;
    }
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", http_get_mime_type(file_path));
    http_response_set_body(response, content, file_size);
    
    free(content);
    framework_log(LOG_LEVEL_DEBUG, "Served: %s (%ld bytes)", file_path, file_size);
    return 0;
}
