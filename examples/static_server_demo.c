/**
 * Static File Server Demo
 * 
 * Demonstrates serving static files (HTML, CSS, JS, images) from a directory.
 * 
 * Setup:
 *   mkdir -p public/css public/js public/images
 *   echo "<html><body><h1>Hello World</h1></body></html>" > public/index.html
 *   echo "body { background: #f0f0f0; }" > public/css/style.css
 *   echo "console.log('Hello from JS');" > public/js/app.js
 * 
 * Test:
 *   curl http://localhost:8080/              -> serves public/index.html
 *   curl http://localhost:8080/css/style.css -> serves CSS
 *   curl http://localhost:8080/js/app.js     -> serves JavaScript
 */

#include "../src/include/framework.h"
#include "../src/include/application.h"
#include "../src/include/http_server.h"
#include "../src/include/json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static APPLICATION *app = NULL;

void signal_handler(int sig)
{
    (void)sig;
    printf("\nShutting down...\n");
    if (app) {
        application_stop(app);
    }
}

void api_hello(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *json = "{"
        "\"message\": \"Hello from API\","
        "\"note\": \"Try accessing / for static files\""
        "}";
    
    http_response_set_json(response, json);
}

int main(void)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create application */
    app = application_create("StaticFileServer", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Create HTTP server on port 8080 */
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    if (!server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        application_destroy(app);
        return 1;
    }
    
    /* Add API route */
    http_server_add_route(server, HTTP_METHOD_GET, "/api/hello", api_hello, NULL);
    
    /* Configure static file serving
     * Maps URL path "/" to directory "./public"
     * Automatically serves index.html for directory requests
     */
    if (http_server_add_static_path(server, "/", "./public") != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to add static path\n");
        fprintf(stderr, "Make sure ./public directory exists\n");
        http_server_destroy(server);
        application_destroy(app);
        return 1;
    }
    
    /* Optionally change default file (default is already index.html) */
    http_server_set_default_file(server, "index.html");
    
    /* Set HTTP server in application */
    application_set_http_server(app, server);
    
    printf("Static file server demo\n");
    printf("========================\n");
    printf("Server: http://localhost:8080\n");
    printf("\n");
    printf("Static files served from: ./public/\n");
    printf("  /              -> public/index.html\n");
    printf("  /css/style.css -> public/css/style.css\n");
    printf("  /js/app.js     -> public/js/app.js\n");
    printf("  /images/*      -> public/images/*\n");
    printf("\n");
    printf("API endpoint:\n");
    printf("  /api/hello     -> JSON response\n");
    printf("\n");
    printf("Setup:\n");
    printf("  mkdir -p public/css public/js public/images\n");
    printf("  echo '<html><body><h1>Hello</h1></body></html>' > public/index.html\n");
    printf("\n");
    printf("Press Ctrl+C to stop\n\n");
    
    /* Run the application */
    application_run(app);
    
    /* Cleanup */
    application_destroy(app);
    
    printf("Server stopped\n");
    return 0;
}
