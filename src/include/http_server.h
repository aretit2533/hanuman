#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
typedef struct _application_ APPLICATION;
typedef struct _http_server_ HTTP_SERVER;
typedef struct _http_route_ HTTP_ROUTE;

/* HTTP Methods */
typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_UNKNOWN
} HTTP_METHOD;

/* HTTP Status Codes */
typedef enum {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_ACCEPTED = 202,
    HTTP_STATUS_NO_CONTENT = 204,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_UNAUTHORIZED = 401,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_INTERNAL_ERROR = 500,
    HTTP_STATUS_NOT_IMPLEMENTED = 501,
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503
} HTTP_STATUS;

/* HTTP Header */
typedef struct _http_header_ {
    char name[128];
    char value[512];
} HTTP_HEADER;

/* HTTP Parameter (for path and query parameters) */
typedef struct _http_param_ {
    char name[128];
    char value[512];
} HTTP_PARAM;

/* HTTP Request */
typedef struct _http_request_ {
    HTTP_METHOD method;
    char path[512];
    char query_string[512];
    char http_version[16];
    
    HTTP_HEADER *headers;
    size_t header_count;
    size_t header_capacity;
    
    char *body;
    size_t body_length;
    
    /* Path parameters (e.g., /users/:id -> id=123) */
    HTTP_PARAM *path_params;
    size_t path_param_count;
    size_t path_param_capacity;
    
    /* Query parameters (e.g., ?name=value&foo=bar) */
    HTTP_PARAM *query_params;
    size_t query_param_count;
    size_t query_param_capacity;
    
    void *user_data;  /* User-defined data */
} HTTP_REQUEST;

/* HTTP Response */
typedef struct _http_response_ {
    HTTP_STATUS status;
    char status_message[128];
    
    HTTP_HEADER *headers;
    size_t header_count;
    size_t header_capacity;
    
    char *body;
    size_t body_length;
    size_t body_capacity;
} HTTP_RESPONSE;

/* Route handler function type */
typedef void (*http_route_handler_fn)(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data);

/* HTTP Server structure */
struct _http_server_ {
    char host[256];
    int port;
    int server_socket;
    int running;
    
    HTTP_ROUTE **routes;
    size_t route_count;
    size_t route_capacity;
    
    APPLICATION *app;
    void *context;
    
    /* EPOLL for concurrent connections */
    int epoll_fd;
    struct _connection_state_ *connection_states;
    size_t connection_count;
    size_t max_connections;
    
    /* Static file serving */
    char static_url_path[256];
    char static_directory[512];
    char default_file[64];
    int static_enabled;
};

/* Forward declare connection state */
typedef struct _connection_state_ CONNECTION_STATE;

/* HTTP Server functions */
HTTP_SERVER* http_server_create(const char *host, int port);
void http_server_destroy(HTTP_SERVER *server);

int http_server_start(HTTP_SERVER *server);
int http_server_stop(HTTP_SERVER *server);
int http_server_run(HTTP_SERVER *server);  /* Blocking call */

/* Route registration */
int http_server_add_route(HTTP_SERVER *server, HTTP_METHOD method, const char *path, 
                         http_route_handler_fn handler, void *user_data);

/**
 * Serve static files from a directory
 * @param server HTTP server instance
 * @param url_path URL path prefix (e.g., "/static")
 * @param directory_path Filesystem directory path
 * @return 0 on success, error code on failure
 */
int http_server_add_static_path(HTTP_SERVER *server, const char *url_path, const char *directory_path);

/**
 * Set default file for directory requests (default: "index.html")
 * @param server HTTP server instance
 * @param filename Default filename
 */
void http_server_set_default_file(HTTP_SERVER *server, const char *filename);

/* Convenience route registration functions */
int http_server_get(HTTP_SERVER *server, const char *path, http_route_handler_fn handler, void *user_data);
int http_server_post(HTTP_SERVER *server, const char *path, http_route_handler_fn handler, void *user_data);
int http_server_put(HTTP_SERVER *server, const char *path, http_route_handler_fn handler, void *user_data);
int http_server_delete(HTTP_SERVER *server, const char *path, http_route_handler_fn handler, void *user_data);
int http_server_patch(HTTP_SERVER *server, const char *path, http_route_handler_fn handler, void *user_data);

/* HTTP Request functions */
HTTP_REQUEST* http_request_create(void);
void http_request_destroy(HTTP_REQUEST *request);
int http_request_add_header(HTTP_REQUEST *request, const char *name, const char *value);
const char* http_request_get_header(HTTP_REQUEST *request, const char *name);

/* Parameter helper functions */
const char* http_request_get_path_param(HTTP_REQUEST *request, const char *name);
const char* http_request_get_query_param(HTTP_REQUEST *request, const char *name);
int http_request_add_path_param(HTTP_REQUEST *request, const char *name, const char *value);
int http_request_add_query_param(HTTP_REQUEST *request, const char *name, const char *value);

/* HTTP Response functions */
HTTP_RESPONSE* http_response_create(void);
void http_response_destroy(HTTP_RESPONSE *response);

int http_response_set_status(HTTP_RESPONSE *response, HTTP_STATUS status);
int http_response_add_header(HTTP_RESPONSE *response, const char *name, const char *value);
int http_response_set_body(HTTP_RESPONSE *response, const char *body, size_t length);
int http_response_set_json(HTTP_RESPONSE *response, const char *json);
int http_response_set_text(HTTP_RESPONSE *response, const char *text);

/* Utility functions */
const char* http_method_to_string(HTTP_METHOD method);
HTTP_METHOD http_method_from_string(const char *method);
const char* http_status_to_string(HTTP_STATUS status);

/**
 * Get MIME type for file extension
 * @param filename Filename or path
 * @return MIME type string
 */
const char* http_get_mime_type(const char *filename);

#endif /* HTTP_SERVER_H */
