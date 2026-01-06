#include "application.h"
#include "http_server.h"
#include "framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Handler for /api/users/:id */
static void user_handler(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    /* Get path parameter */
    const char *user_id = http_request_get_path_param(request, "id");
    
    /* Get query parameters */
    const char *format = http_request_get_query_param(request, "format");
    const char *include_details = http_request_get_query_param(request, "details");
    
    /* Build response */
    char body[1024];
    int offset = 0;
    
    offset += snprintf(body + offset, sizeof(body) - offset,
                      "User Information\n");
    offset += snprintf(body + offset, sizeof(body) - offset,
                      "================\n\n");
    
    if (user_id) {
        offset += snprintf(body + offset, sizeof(body) - offset,
                          "User ID: %s\n", user_id);
    } else {
        offset += snprintf(body + offset, sizeof(body) - offset,
                          "User ID: (not provided)\n");
    }
    
    if (format) {
        offset += snprintf(body + offset, sizeof(body) - offset,
                          "Format: %s\n", format);
    }
    
    if (include_details) {
        offset += snprintf(body + offset, sizeof(body) - offset,
                          "Include Details: %s\n", include_details);
    }
    
    offset += snprintf(body + offset, sizeof(body) - offset,
                      "\nQuery String: %s\n", request->query_string);
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/plain");
    http_response_set_text(response, body);
}

/* Handler for /api/posts/:postId/comments/:commentId */
static void comment_handler(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    const char *post_id = http_request_get_path_param(request, "postId");
    const char *comment_id = http_request_get_path_param(request, "commentId");
    
    char body[1024];
    snprintf(body, sizeof(body),
             "Comment Information\n"
             "===================\n\n"
             "Post ID: %s\n"
             "Comment ID: %s\n",
             post_id ? post_id : "(not provided)",
             comment_id ? comment_id : "(not provided)");
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/plain");
    http_response_set_text(response, body);
}

/* Handler for /api/search with query parameters */
static void search_handler(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    const char *query = http_request_get_query_param(request, "q");
    const char *page = http_request_get_query_param(request, "page");
    const char *limit = http_request_get_query_param(request, "limit");
    const char *sort = http_request_get_query_param(request, "sort");
    
    char body[1024];
    int offset = 0;
    
    offset += snprintf(body + offset, sizeof(body) - offset,
                      "Search Results\n");
    offset += snprintf(body + offset, sizeof(body) - offset,
                      "==============\n\n");
    
    if (query) {
        offset += snprintf(body + offset, sizeof(body) - offset,
                          "Search Query: %s\n", query);
    }
    
    if (page) {
        offset += snprintf(body + offset, sizeof(body) - offset,
                          "Page: %s\n", page);
    }
    
    if (limit) {
        offset += snprintf(body + offset, sizeof(body) - offset,
                          "Results Per Page: %s\n", limit);
    }
    
    if (sort) {
        offset += snprintf(body + offset, sizeof(body) - offset,
                          "Sort By: %s\n", sort);
    }
    
    offset += snprintf(body + offset, sizeof(body) - offset,
                      "\nRaw Query String: %s\n", request->query_string);
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/plain");
    http_response_set_text(response, body);
}

/* Handler for root path */
static void root_handler(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *help_text = 
        "Path Parameter & Query String Demo\n"
        "===================================\n\n"
        "Try these endpoints:\n\n"
        "1. GET /api/users/123\n"
        "   - Path parameter: id=123\n\n"
        "2. GET /api/users/456?format=json&details=true\n"
        "   - Path parameter: id=456\n"
        "   - Query parameters: format=json, details=true\n\n"
        "3. GET /api/posts/100/comments/5\n"
        "   - Path parameters: postId=100, commentId=5\n\n"
        "4. GET /api/search?q=test&page=2&limit=10&sort=date\n"
        "   - Query parameters: q, page, limit, sort\n\n"
        "5. GET /api/search?q=hello+world&special=%21%40%23\n"
        "   - URL encoded: spaces and special chars\n";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/plain");
    http_response_set_text(response, help_text);
}

int main(void)
{
    /* Initialize application */
    APPLICATION *app = application_create("Parameter Demo", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Initialize HTTP server */
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    if (!server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        application_destroy(app);
        return 1;
    }
    
    /* Register routes with path parameters */
    http_server_add_route(server, HTTP_METHOD_GET, "/", root_handler, NULL);
    http_server_add_route(server, HTTP_METHOD_GET, "/api/users/:id", user_handler, NULL);
    http_server_add_route(server, HTTP_METHOD_GET, "/api/posts/:postId/comments/:commentId", 
                         comment_handler, NULL);
    http_server_add_route(server, HTTP_METHOD_GET, "/api/search", search_handler, NULL);
    
    printf("\n");
    printf("===========================================\n");
    printf("  Path Parameter & Query String Demo\n");
    printf("===========================================\n");
    printf("\n");
    printf("Server running on http://localhost:8080/\n");
    printf("\n");
    printf("Example requests:\n");
    printf("  curl http://localhost:8080/\n");
    printf("  curl http://localhost:8080/api/users/123\n");
    printf("  curl 'http://localhost:8080/api/users/456?format=json&details=true'\n");
    printf("  curl http://localhost:8080/api/posts/100/comments/5\n");
    printf("  curl 'http://localhost:8080/api/search?q=test&page=2&limit=10'\n");
    printf("\n");
    printf("Press Ctrl+C to stop the server\n");
    printf("\n");
    
    /* Start server */
    if (http_server_start(server) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start HTTP server\n");
        http_server_destroy(server);
        application_destroy(app);
        return 1;
    }
    
    /* Run server event loop (blocking) */
    http_server_run(server);
    
    /* Cleanup */
    http_server_destroy(server);
    application_destroy(app);
    
    return 0;
}
