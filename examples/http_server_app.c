#include "framework.h"
#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

/* Global server for signal handling */
static HTTP_SERVER *global_server = NULL;

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
        "<head><title>Equinox HTTP Server</title></head>\n"
        "<body>\n"
        "<h1>Welcome to Hanuman Framework HTTP Server</h1>\n"
        "<p>Available endpoints:</p>\n"
        "<ul>\n"
        "<li><a href='/'>GET /</a> - This page</li>\n"
        "<li><a href='/api/status'>GET /api/status</a> - Server status</li>\n"
        "<li><a href='/api/hello'>GET /api/hello</a> - Hello message</li>\n"
        "<li>POST /api/echo - Echo request body</li>\n"
        "<li><a href='/api/users'>GET /api/users</a> - List users (JSON)</li>\n"
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
    (void)user_data;
    
    char json[512];
    snprintf(json, sizeof(json),
            "{"
            "\"status\": \"running\","
            "\"framework\": \"Equinox\","
            "\"version\": \"1.0.0\","
            "\"server\": \"HTTP/1.1\""
            "}");
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

/* Route handler: GET /api/hello */
void handle_hello(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    const char *name = "World";
    
    /* Check for name query parameter */
    if (strlen(request->query_string) > 0) {
        char *name_param = strstr(request->query_string, "name=");
        if (name_param) {
            name = name_param + 5;  /* Skip "name=" */
            /* Simple extraction - just get until & or end */
            char *end = strchr(name, '&');
            if (end) *end = '\0';
        }
    }
    
    char json[512];
    snprintf(json, sizeof(json),
            "{"
            "\"message\": \"Hello, %s!\","
            "\"timestamp\": %ld"
            "}", name, (long)time(NULL));
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

/* Route handler: POST /api/echo */
void handle_echo(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    if (!request->body || request->body_length == 0) {
        http_response_set_status(response, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(response, "{\"error\": \"No body provided\"}");
        return;
    }
    
    char json[2048];
    snprintf(json, sizeof(json),
            "{"
            "\"echo\": \"%.*s\","
            "\"length\": %zu"
            "}", 
            (int)request->body_length, request->body,
            request->body_length);
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

/* Route handler: GET /api/users */
void handle_users(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *json = 
        "{"
        "\"users\": ["
        "{"
        "\"id\": 1,"
        "\"name\": \"Alice\","
        "\"email\": \"alice@example.com\""
        "},"
        "{"
        "\"id\": 2,"
        "\"name\": \"Bob\","
        "\"email\": \"bob@example.com\""
        "},"
        "{"
        "\"id\": 3,"
        "\"name\": \"Charlie\","
        "\"email\": \"charlie@example.com\""
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
        http_response_set_json(response, "{\"error\": \"No body provided\"}");
        return;
    }
    
    /* In a real application, you would parse the JSON body and create a user */
    char json[512];
    snprintf(json, sizeof(json),
            "{"
            "\"success\": true,"
            "\"message\": \"User created\","
            "\"id\": 4"
            "}");
    
    http_response_set_status(response, HTTP_STATUS_CREATED);
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
    printf("║     Equinox HTTP Server Framework v1.0.0            ║\n");
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
    
    /* Register routes */
    printf("Registering routes...\n");
    http_server_get(server, "/", handle_root, NULL);
    http_server_get(server, "/api/status", handle_status, NULL);
    http_server_get(server, "/api/hello", handle_hello, NULL);
    http_server_post(server, "/api/echo", handle_echo, NULL);
    http_server_get(server, "/api/users", handle_users, NULL);
    http_server_post(server, "/api/users", handle_create_user, NULL);
    
    /* Attach server to application */
    application_set_http_server(app, server);
    
    /* Initialize and start application */
    printf("\nInitializing application...\n");
    if (application_initialize(app) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to initialize application\n");
        http_server_destroy(server);
        application_destroy(app);
        return 1;
    }
    
    if (application_start(app) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start application\n");
        application_cleanup(app);
        http_server_destroy(server);
        application_destroy(app);
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
        return 1;
    }
    
    printf("\n╔═══════════════════════════════════════════════════════╗\n");
    printf("║  Server is running!                                   ║\n");
    printf("║  Open http://localhost:8080 in your browser          ║\n");
    printf("║  Press Ctrl+C to stop                                ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    /* Run server (blocking) */
    http_server_run(server);
    
    /* Cleanup */
    printf("\nCleaning up...\n");
    application_stop(app);
    application_cleanup(app);
    http_server_destroy(server);
    application_destroy(app);
    framework_shutdown();
    
    printf("Server stopped. Goodbye!\n");
    
    return 0;
}
