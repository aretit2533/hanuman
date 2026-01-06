/**
 * HTTP Proxy Demo
 * 
 * Demonstrates using HTTP server and client together.
 * Creates a simple proxy that forwards requests to another server.
 */

#include "framework.h"
#include "application.h"
#include "http_server.h"
#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <strings.h>

static APPLICATION *app = NULL;

void signal_handler(int sig)
{
    (void)sig;
    printf("\nShutting down...\n");
    if (app) {
        application_stop(app);
    }
}

/* Proxy handler - forwards request to httpbin.org */
void handle_proxy(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    /* Build target URL */
    char target_url[1024];
    snprintf(target_url, sizeof(target_url), "http://httpbin.org%s", request->path);
    
    framework_log(LOG_LEVEL_INFO, "Proxying %s to %s", request->path, target_url);
    
    /* Create client request */
    HTTP_CLIENT_REQUEST *client_req = http_client_request_create(
        http_method_to_string(request->method), 
        target_url
    );
    
    if (!client_req) {
        http_response_set_status(response, HTTP_STATUS_INTERNAL_ERROR);
        http_response_set_text(response, "Failed to create proxy request");
        return;
    }
    
    /* Forward original headers (except Host) */
    for (size_t i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i].name, "Host") != 0) {
            http_client_request_add_header(client_req, 
                                          request->headers[i].name,
                                          request->headers[i].value);
        }
    }
    
    /* Forward body if present */
    if (request->body && request->body_length > 0) {
        http_client_request_set_body(client_req, request->body, request->body_length);
    }
    
    /* Execute proxied request */
    HTTP_CLIENT_RESPONSE *client_resp = http_client_execute(client_req);
    http_client_request_destroy(client_req);
    
    if (!client_resp) {
        http_response_set_status(response, HTTP_STATUS_INTERNAL_ERROR);
        http_response_set_text(response, "Proxy request failed");
        return;
    }
    
    if (client_resp->error_message) {
        http_response_set_status(response, HTTP_STATUS_INTERNAL_ERROR);
        http_response_set_text(response, client_resp->error_message);
        http_client_response_destroy(client_resp);
        return;
    }
    
    /* Forward response */
    http_response_set_status(response, client_resp->status_code);
    
    /* Forward response headers */
    for (size_t i = 0; i < client_resp->header_count; i++) {
        /* Skip some headers that should not be forwarded */
        if (strcasecmp(client_resp->header_names[i], "Transfer-Encoding") != 0 &&
            strcasecmp(client_resp->header_names[i], "Connection") != 0) {
            http_response_add_header(response, 
                                    client_resp->header_names[i],
                                    client_resp->header_values[i]);
        }
    }
    
    /* Forward response body */
    if (client_resp->body && client_resp->body_length > 0) {
        http_response_set_body(response, client_resp->body, client_resp->body_length);
    }
    
    framework_log(LOG_LEVEL_INFO, "Proxied response: %d (%.2f ms)", 
                 client_resp->status_code, 
                 client_resp->elapsed_time_ms);
    
    http_client_response_destroy(client_resp);
}

/* Info handler */
void handle_info(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>HTTP Proxy Demo</title></head>\n"
        "<body>\n"
        "<h1>Equinox HTTP Proxy Demo</h1>\n"
        "<p>This server proxies requests to httpbin.org</p>\n"
        "<h2>Try these:</h2>\n"
        "<ul>\n"
        "<li><a href='/get'>/get</a> - Proxied GET request</li>\n"
        "<li><a href='/headers'>/headers</a> - Show headers</li>\n"
        "<li><a href='/ip'>/ip</a> - Show origin IP</li>\n"
        "<li><a href='/user-agent'>/user-agent</a> - Show user agent</li>\n"
        "<li><a href='/status/200'>/status/200</a> - Status code test</li>\n"
        "</ul>\n"
        "<p>POST to <code>/post</code> with JSON body to test POST proxying</p>\n"
        "</body>\n"
        "</html>";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/html");
    http_response_set_body(response, html, strlen(html));
}

int main(void)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create application */
    app = application_create("HTTPProxy", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Create HTTP server */
    HTTP_SERVER *server = http_server_create("0.0.0.0", 9090);
    if (!server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        application_destroy(app);
        return 1;
    }
    
    /* Add info route */
    http_server_add_route(server, HTTP_METHOD_GET, "/", handle_info, NULL);
    
    /* Add proxy routes (catch-all pattern would be better, but use specific routes) */
    const char *proxy_paths[] = {
        "/get", "/post", "/put", "/delete", "/patch",
        "/headers", "/ip", "/user-agent", 
        "/status/*", "/delay/*"
    };
    
    for (size_t i = 0; i < sizeof(proxy_paths) / sizeof(proxy_paths[0]); i++) {
        http_server_add_route(server, HTTP_METHOD_GET, proxy_paths[i], handle_proxy, NULL);
        http_server_add_route(server, HTTP_METHOD_POST, proxy_paths[i], handle_proxy, NULL);
        http_server_add_route(server, HTTP_METHOD_PUT, proxy_paths[i], handle_proxy, NULL);
        http_server_add_route(server, HTTP_METHOD_DELETE, proxy_paths[i], handle_proxy, NULL);
        http_server_add_route(server, HTTP_METHOD_PATCH, proxy_paths[i], handle_proxy, NULL);
    }
    
    application_set_http_server(app, server);
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║   HTTP Proxy Demo                      ║\n");
    printf("║   Server + Client Integration          ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");
    printf("Proxy server: http://localhost:9090\n");
    printf("Target: httpbin.org\n");
    printf("\n");
    printf("Example requests:\n");
    printf("  curl http://localhost:9090/\n");
    printf("  curl http://localhost:9090/get\n");
    printf("  curl http://localhost:9090/headers\n");
    printf("  curl -X POST -d '{\"test\":\"data\"}' http://localhost:9090/post\n");
    printf("\n");
    printf("Press Ctrl+C to stop\n\n");
    
    /* Run */
    application_run(app);
    
    /* Cleanup */
    application_destroy(app);
    
    printf("Proxy stopped\n");
    return 0;
}
