#include "framework.h"
#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

/* Global server for signal handling */
static HTTP_SERVER *global_server = NULL;

/* User data structure */
typedef struct {
    int request_count;
} UserData;

/* Signal handler for graceful shutdown */
void signal_handler(int signum)
{
    (void)signum;
    printf("\n\nShutting down server...\n");
    if (global_server) {
        http_server_stop(global_server);
    }
}

/* Route handler: GET / */
void handle_root(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>Equinox HTTP/2 Server</title></head>\n"
        "<body>\n"
        "<h1>Welcome to Hanuman Framework HTTP/2 Server</h1>\n"
        "<p><strong>Supported Protocols:</strong> HTTP/1.1, HTTP/2</p>\n"
        "<p><strong>Available endpoints:</strong></p>\n"
        "<ul>\n"
        "<li><a href='/'>GET /</a> - This page</li>\n"
        "<li><a href='/api/status'>GET /api/status</a> - Server status (JSON)</li>\n"
        "<li><a href='/api/users'>GET /api/users</a> - List users (JSON)</li>\n"
        "<li>POST /api/users - Create user</li>\n"
        "<li>PUT /api/users/:id - Update user</li>\n"
        "<li>PATCH /api/users/:id - Partial update user</li>\n"
        "<li>DELETE /api/users/:id - Delete user</li>\n"
        "</ul>\n"
        "<h2>HTTP Methods Supported</h2>\n"
        "<ul>\n"
        "<li>GET</li>\n"
        "<li>POST</li>\n"
        "<li>PUT</li>\n"
        "<li>PATCH (NEW!)</li>\n"
        "<li>DELETE</li>\n"
        "<li>HEAD</li>\n"
        "<li>OPTIONS</li>\n"
        "</ul>\n"
        "</body>\n"
        "</html>";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/html");
    http_response_set_body(response, html, strlen(html));
}

/* Route handler: GET /api/status */
void handle_status(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    UserData *data = (UserData*)user_data;
    
    char json[512];
    snprintf(json, sizeof(json),
            "{"
            "\"status\": \"running\","
            "\"framework\": \"Equinox\","
            "\"version\": \"1.0.0\","
            "\"protocols\": [\"HTTP/1.1\", \"HTTP/2\"],"
            "\"methods\": [\"GET\", \"POST\", \"PUT\", \"PATCH\", \"DELETE\", \"HEAD\", \"OPTIONS\"],"
            "\"requests_served\": %d"
            "}", data ? data->request_count : 0);
    
    if (data) data->request_count++;
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

/* Route handler: GET /api/users */
void handle_get_users(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    const char *user_id = http_request_get_path_param(request, "id");
    printf("Requested user ID: %s\n", user_id ? user_id : "N/A");
    
    const char *json = 
        "{"
        "\"users\": ["
        "{"
        "\"id\": 1,"
        "\"name\": \"Alice\","
        "\"email\": \"alice@example.com\","
        "\"role\": \"admin\""
        "},"
        "{"
        "\"id\": 2,"
        "\"name\": \"Bob\","
        "\"email\": \"bob@example.com\","
        "\"role\": \"user\""
        "},"
        "{"
        "\"id\": 3,"
        "\"name\": \"Charlie\","
        "\"email\": \"charlie@example.com\","
        "\"role\": \"user\""
        "}"
        "],"
        "\"count\": 3"
        "}";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

/* Route handler: POST /api/users */
void handle_create_user(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    if (!request->body || request->body_length == 0) {
        http_response_set_status(response, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(response, "{\"error\": \"Request body required\"}");
        return;
    }
    
    char json[512];
    snprintf(json, sizeof(json),
            "{"
            "\"success\": true,"
            "\"message\": \"User created successfully\","
            "\"method\": \"POST\","
            "\"id\": 4"
            "}");
    
    http_response_set_status(response, HTTP_STATUS_CREATED);
    http_response_set_json(response, json);
}

/* Route handler: PUT /api/users/1 */
void handle_update_user(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    if (!request->body || request->body_length == 0) {
        http_response_set_status(response, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(response, "{\"error\": \"Request body required\"}");
        return;
    }
    
    char json[512];
    snprintf(json, sizeof(json),
            "{"
            "\"success\": true,"
            "\"message\": \"User updated successfully (full replacement)\","
            "\"method\": \"PUT\","
            "\"id\": 1"
            "}");
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

/* Route handler: PATCH /api/users/1 (NEW!) */
void handle_patch_user(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    if (!request->body || request->body_length == 0) {
        http_response_set_status(response, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(response, "{\"error\": \"Request body required\"}");
        return;
    }
    
    char json[512];
    snprintf(json, sizeof(json),
            "{"
            "\"success\": true,"
            "\"message\": \"User updated successfully (partial update)\","
            "\"method\": \"PATCH\","
            "\"id\": 1,"
            "\"note\": \"PATCH allows partial updates, PUT requires full object replacement\""
            "}");
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

/* Route handler: DELETE /api/users/1 */
void handle_delete_user(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    char json[256];
    snprintf(json, sizeof(json),
            "{"
            "\"success\": true,"
            "\"message\": \"User deleted successfully\","
            "\"method\": \"DELETE\","
            "\"id\": 1"
            "}");
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    /* Initialize framework */
    framework_init();
    framework_set_log_level(LOG_LEVEL_INFO);
    
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║   Equinox HTTP Server Framework v1.0.0              ║\n");
    printf("║   Supporting HTTP/1.1 and HTTP/2                    ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    /* Create application */
    APPLICATION *app = application_create("HTTPServerApp", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Create HTTP server */
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    if (!server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        application_destroy(app);
        return 1;
    }
    
    global_server = server;
    
    /* Register signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create user data */
    UserData *user_data = (UserData*)calloc(1, sizeof(UserData));
    user_data->request_count = 0;
    
    /* Register routes */
    printf("Registering routes...\n");
    http_server_get(server, "/", handle_root, NULL);
    http_server_get(server, "/api/status", handle_status, user_data);
    http_server_get(server, "/api/users/:id", handle_get_users, NULL);
    http_server_post(server, "/api/users", handle_create_user, NULL);
    http_server_put(server, "/api/users/:id", handle_update_user, NULL);
    http_server_patch(server, "/api/users/:id", handle_patch_user, NULL);  /* NEW! */
    http_server_delete(server, "/api/users/:id", handle_delete_user, NULL);
    
    printf("✓ Registered 7 routes\n");
    
    /* Attach server to application */
    application_set_http_server(app, server);
    
    /* Initialize and start application */
    printf("\nInitializing application...\n");
    if (application_initialize(app) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to initialize application\n");
        http_server_destroy(server);
        application_destroy(app);
        free(user_data);
        return 1;
    }
    
    if (application_start(app) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start application\n");
        application_cleanup(app);
        http_server_destroy(server);
        application_destroy(app);
        free(user_data);
        return 1;
    }
    
    /* Start HTTP server */
    printf("\nStarting HTTP server...\n");
    if (http_server_start(server) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start HTTP server\n");
        application_stop(app);
        application_cleanup(app);
        http_server_destroy(server);
        application_destroy(app);
        free(user_data);
        return 1;
    }
    
    printf("\n╔═══════════════════════════════════════════════════════╗\n");
    printf("║  Server is running!                                   ║\n");
    printf("║  Protocols: HTTP/1.1, HTTP/2                          ║\n");
    printf("║  Methods: GET, POST, PUT, PATCH, DELETE               ║\n");
    printf("║  URL: http://localhost:8080                           ║\n");
    printf("║  Press Ctrl+C to stop                                 ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    /* Run server (blocking) */
    http_server_run(server);
    
    /* Cleanup */
    printf("\nCleaning up...\n");
    application_stop(app);
    application_cleanup(app);
    http_server_destroy(server);
    application_destroy(app);
    free(user_data);
    framework_shutdown();
    
    printf("Server stopped. Goodbye!\n");
    
    return 0;
}
