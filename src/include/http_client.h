/**
 * HTTP Client Module
 * 
 * Provides functionality for making HTTP requests to external servers.
 * Supports GET, POST, PUT, DELETE, PATCH with custom headers and body.
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "framework.h"
#include <stddef.h>

/* Forward declarations */
typedef struct _http_client_request_ HTTP_CLIENT_REQUEST;
typedef struct _http_client_response_ HTTP_CLIENT_RESPONSE;
typedef struct _http_client_async_handle_ HTTP_CLIENT_ASYNC_HANDLE;

/**
 * Callback function type for async requests
 * 
 * @param response Response object (caller must free with http_client_response_destroy)
 * @param user_data User-provided data pointer
 */
typedef void (*HTTP_CLIENT_CALLBACK)(HTTP_CLIENT_RESPONSE *response, void *user_data);

/* HTTP Client Request */
struct _http_client_request_ {
    char *url;
    char *method;
    char *body;
    size_t body_length;
    
    /* Headers */
    char **header_names;
    char **header_values;
    size_t header_count;
    size_t header_capacity;
    
    /* Options */
    int timeout_seconds;
    int follow_redirects;
    int max_redirects;
    int verify_ssl;
};

/* HTTP Client Response */
struct _http_client_response_ {
    int status_code;
    char *body;
    size_t body_length;
    
    /* Headers */
    char **header_names;
    char **header_values;
    size_t header_count;
    size_t header_capacity;
    
    /* Metadata */
    double elapsed_time_ms;
    char *error_message;
};

/* ==================== Request Management ==================== */

/**
 * Create a new HTTP client request
 * 
 * @param method HTTP method (GET, POST, PUT, DELETE, PATCH)
 * @param url Target URL (http:// or https://)
 * @return New HTTP_CLIENT_REQUEST or NULL on failure
 */
HTTP_CLIENT_REQUEST* http_client_request_create(const char *method, const char *url);

/**
 * Destroy an HTTP client request and free resources
 * 
 * @param request Request to destroy
 */
void http_client_request_destroy(HTTP_CLIENT_REQUEST *request);

/**
 * Add a header to the request
 * 
 * @param request Target request
 * @param name Header name
 * @param value Header value
 * @return FRAMEWORK_SUCCESS or error code
 */
int http_client_request_add_header(HTTP_CLIENT_REQUEST *request, 
                                    const char *name, 
                                    const char *value);

/**
 * Set request body (for POST, PUT, PATCH)
 * 
 * @param request Target request
 * @param body Body content
 * @param length Body length (0 = strlen)
 * @return FRAMEWORK_SUCCESS or error code
 */
int http_client_request_set_body(HTTP_CLIENT_REQUEST *request, 
                                  const char *body, 
                                  size_t length);

/**
 * Set request timeout in seconds
 * 
 * @param request Target request
 * @param timeout_seconds Timeout (default: 30)
 */
void http_client_request_set_timeout(HTTP_CLIENT_REQUEST *request, int timeout_seconds);

/**
 * Enable/disable SSL certificate verification
 * 
 * @param request Target request
 * @param verify 1 to verify, 0 to skip (default: 1)
 */
void http_client_request_set_verify_ssl(HTTP_CLIENT_REQUEST *request, int verify);

/**
 * Enable/disable following redirects
 * 
 * @param request Target request
 * @param follow 1 to follow, 0 to stop (default: 1)
 * @param max_redirects Maximum redirects to follow (default: 5)
 */
void http_client_request_set_follow_redirects(HTTP_CLIENT_REQUEST *request, 
                                               int follow, 
                                               int max_redirects);

/* ==================== Response Management ==================== */

/**
 * Destroy an HTTP client response and free resources
 * 
 * @param response Response to destroy
 */
void http_client_response_destroy(HTTP_CLIENT_RESPONSE *response);

/**
 * Get a header value from the response
 * 
 * @param response Target response
 * @param name Header name (case-insensitive)
 * @return Header value or NULL if not found
 */
const char* http_client_response_get_header(HTTP_CLIENT_RESPONSE *response, 
                                             const char *name);

/* ==================== Request Execution ==================== */

/**
 * Execute an HTTP request synchronously
 * 
 * Blocks until the request completes or times out.
 * 
 * @param request Request to execute
 * @return Response object (must be freed with http_client_response_destroy)
 */
HTTP_CLIENT_RESPONSE* http_client_execute(HTTP_CLIENT_REQUEST *request);

/**
 * Execute an HTTP request asynchronously with callback
 * 
 * Returns immediately and invokes callback when request completes.
 * Request is automatically freed after callback executes.
 * 
 * @param request Request to execute (ownership transferred)
 * @param callback Function to call with response
 * @param user_data User data to pass to callback
 * @return Async handle for tracking/canceling, or NULL on failure
 */
HTTP_CLIENT_ASYNC_HANDLE* http_client_execute_async(HTTP_CLIENT_REQUEST *request,
                                                      HTTP_CLIENT_CALLBACK callback,
                                                      void *user_data);

/**
 * Wait for an async request to complete
 * 
 * Blocks until the async request finishes. Handle is freed automatically.
 * 
 * @param handle Async handle from http_client_execute_async
 * @return FRAMEWORK_SUCCESS or error code
 */
int http_client_async_wait(HTTP_CLIENT_ASYNC_HANDLE *handle);

/**
 * Cancel an async request
 * 
 * Attempts to cancel the request. If already completed, has no effect.
 * Handle is freed automatically.
 * 
 * @param handle Async handle from http_client_execute_async
 * @return FRAMEWORK_SUCCESS or error code
 */
int http_client_async_cancel(HTTP_CLIENT_ASYNC_HANDLE *handle);

/**
 * Check if an async request is complete
 * 
 * @param handle Async handle from http_client_execute_async
 * @return 1 if complete, 0 if still running, -1 on error
 */
int http_client_async_is_complete(HTTP_CLIENT_ASYNC_HANDLE *handle);

/* ==================== Convenience Functions ==================== */

/**
 * Perform a simple GET request
 * 
 * @param url Target URL
 * @return Response object (must be freed)
 */
HTTP_CLIENT_RESPONSE* http_client_get(const char *url);

/**
 * Perform a POST request with JSON body
 * 
 * @param url Target URL
 * @param json_body JSON string
 * @return Response object (must be freed)
 */
HTTP_CLIENT_RESPONSE* http_client_post_json(const char *url, const char *json_body);

/**
 * Perform a POST request with form data
 * 
 * @param url Target URL
 * @param form_data Form-encoded data
 * @return Response object (must be freed)
 */
HTTP_CLIENT_RESPONSE* http_client_post_form(const char *url, const char *form_data);

#endif /* HTTP_CLIENT_H */
