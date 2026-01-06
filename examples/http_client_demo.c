/**
 * HTTP Client Demo
 * 
 * Demonstrates making HTTP requests to external servers.
 * 
 * Features:
 * - GET requests
 * - POST with JSON body
 * - Custom headers
 * - Response parsing
 * - Error handling
 * 
 * Note: Currently supports HTTP only (not HTTPS)
 */

#include "framework.h"
#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Forward declaration */
char* strtolower(const char *str);

void print_separator(void)
{
    printf("\n");
    printf("========================================\n");
    printf("\n");
}

void demo_simple_get(void)
{
    printf("Demo 1: Simple GET Request\n");
    printf("---------------------------\n");
    
    /* Make a GET request to httpbin.org (echo service) */
    HTTP_CLIENT_RESPONSE *response = http_client_get("http://httpbin.org/get");
    
    if (!response) {
        printf("ERROR: Failed to create request\n");
        return;
    }
    
    if (response->error_message) {
        printf("ERROR: %s\n", response->error_message);
    } else {
        printf("Status: %d\n", response->status_code);
        printf("Time: %.2f ms\n", response->elapsed_time_ms);
        printf("Content-Type: %s\n", 
               http_client_response_get_header(response, "Content-Type"));
        printf("\nResponse body (first 500 chars):\n");
        if (response->body) {
            size_t len = response->body_length < 500 ? response->body_length : 500;
            printf("%.*s\n", (int)len, response->body);
        }
    }
    
    http_client_response_destroy(response);
}

void demo_get_with_headers(void)
{
    printf("Demo 2: GET with Custom Headers\n");
    printf("--------------------------------\n");
    
    HTTP_CLIENT_REQUEST *request = http_client_request_create("GET", 
                                                               "http://httpbin.org/headers");
    if (!request) {
        printf("ERROR: Failed to create request\n");
        return;
    }
    
    /* Add custom headers */
    http_client_request_add_header(request, "X-Custom-Header", "MyValue");
    http_client_request_add_header(request, "X-Request-ID", "12345");
    
    HTTP_CLIENT_RESPONSE *response = http_client_execute(request);
    
    if (response->error_message) {
        printf("ERROR: %s\n", response->error_message);
    } else {
        printf("Status: %d\n", response->status_code);
        printf("\nResponse shows our custom headers:\n");
        if (response->body) {
            size_t len = response->body_length < 600 ? response->body_length : 600;
            printf("%.*s\n", (int)len, response->body);
        }
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
}

void demo_post_json(void)
{
    printf("Demo 3: POST with JSON Body\n");
    printf("----------------------------\n");
    
    const char *json_data = "{"
        "\"name\": \"Hanuman Framework\","
        "\"version\": 1,"
        "\"features\": [\"HTTP\", \"Kafka\", \"JSON\"]"
        "}";
    
    HTTP_CLIENT_RESPONSE *response = http_client_post_json("http://httpbin.org/post", 
                                                            json_data);
    
    if (!response) {
        printf("ERROR: Failed to create request\n");
        return;
    }
    
    if (response->error_message) {
        printf("ERROR: %s\n", response->error_message);
    } else {
        printf("Status: %d\n", response->status_code);
        printf("Time: %.2f ms\n", response->elapsed_time_ms);
        printf("\nEcho response (first 800 chars):\n");
        if (response->body) {
            size_t len = response->body_length < 800 ? response->body_length : 800;
            printf("%.*s\n", (int)len, response->body);
        }
    }
    
    http_client_response_destroy(response);
}

void demo_different_methods(void)
{
    printf("Demo 4: Different HTTP Methods\n");
    printf("-------------------------------\n");
    
    const char *methods[] = {"GET", "POST", "PUT", "DELETE", "PATCH"};
    
    for (int i = 0; i < 5; i++) {
        char url[128];
        snprintf(url, sizeof(url), "http://httpbin.org/%s", 
                strtolower(methods[i]));
        
        HTTP_CLIENT_REQUEST *request = http_client_request_create(methods[i], url);
        if (!request) continue;
        
        if (strcmp(methods[i], "GET") != 0 && strcmp(methods[i], "DELETE") != 0) {
            http_client_request_set_body(request, "{\"test\": \"data\"}", 0);
            http_client_request_add_header(request, "Content-Type", "application/json");
        }
        
        HTTP_CLIENT_RESPONSE *response = http_client_execute(request);
        
        printf("%s request: ", methods[i]);
        if (response->error_message) {
            printf("ERROR - %s\n", response->error_message);
        } else {
            printf("Status %d (%.1f ms)\n", 
                   response->status_code, 
                   response->elapsed_time_ms);
        }
        
        http_client_response_destroy(response);
        http_client_request_destroy(request);
    }
}

void demo_error_handling(void)
{
    printf("Demo 5: Error Handling\n");
    printf("----------------------\n");
    
    /* Test 1: Invalid URL */
    printf("1. Invalid URL: ");
    HTTP_CLIENT_RESPONSE *r1 = http_client_get("invalid-url");
    if (r1) {
        printf("%s\n", r1->error_message ? r1->error_message : "Unknown error");
        http_client_response_destroy(r1);
    }
    
    /* Test 2: Non-existent host */
    printf("2. Non-existent host: ");
    HTTP_CLIENT_RESPONSE *r2 = http_client_get("http://this-host-does-not-exist-12345.com/");
    if (r2) {
        printf("%s\n", r2->error_message ? r2->error_message : "Unknown error");
        http_client_response_destroy(r2);
    }
    
    /* Test 3: HTTPS (now supported!) */
    printf("3. HTTPS request: ");
    HTTP_CLIENT_RESPONSE *r3 = http_client_get("https://httpbin.org/get");
    if (r3) {
        if (r3->error_message) {
            printf("%s\n", r3->error_message);
        } else {
            printf("Status %d (HTTPS working!)\n", r3->status_code);
        }
        http_client_response_destroy(r3);
    }
    
    /* Test 4: 404 response */
    printf("4. 404 Not Found: ");
    HTTP_CLIENT_RESPONSE *r4 = http_client_get("http://httpbin.org/status/404");
    if (r4) {
        if (r4->error_message) {
            printf("%s\n", r4->error_message);
        } else {
            printf("Status %d (this is expected)\n", r4->status_code);
        }
        http_client_response_destroy(r4);
    }
}

void demo_timeout(void)
{
    printf("Demo 6: Request Timeout\n");
    printf("-----------------------\n");
    
    HTTP_CLIENT_REQUEST *request = http_client_request_create("GET", 
                                                               "http://httpbin.org/delay/3");
    if (!request) {
        printf("ERROR: Failed to create request\n");
        return;
    }
    
    /* Set short timeout (2 seconds) for a 3-second delay endpoint */
    http_client_request_set_timeout(request, 2);
    
    printf("Requesting 3-second delay with 2-second timeout...\n");
    HTTP_CLIENT_RESPONSE *response = http_client_execute(request);
    
    if (response->error_message) {
        printf("Result: %s (expected timeout)\n", response->error_message);
    } else {
        printf("Result: Success - Status %d (%.2f ms)\n", 
               response->status_code, 
               response->elapsed_time_ms);
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
}

static void demo_compression()
{
    printf("Demo 8: Compression Support (gzip/deflate)\n");
    printf("-------------------------------------------\n");
    
    HTTP_CLIENT_REQUEST *request = http_client_request_create(
        "GET", "https://httpbin.org/gzip");
    
    printf("Testing gzip compression...\n");
    HTTP_CLIENT_RESPONSE *response = http_client_execute(request);
    
    if (response->error_message) {
        printf("✗ Error: %s\n", response->error_message);
    } else {
        printf("✓ Gzip works!\n");
        printf("Status: %d\n", response->status_code);
        printf("Time: %.2f ms\n", response->elapsed_time_ms);
        
        /* Check if response was decompressed */
        if (response->body && strstr(response->body, "gzipped") != NULL) {
            printf("✓ Response successfully decompressed\n");
            printf("Body length: %zu bytes\n", response->body_length);
        }
        
        /* Show headers */
        for (size_t i = 0; i < response->header_count; i++) {
            if (strcasecmp(response->header_names[i], "Content-Encoding") == 0) {
                printf("Content-Encoding: %s\n", response->header_values[i]);
            }
        }
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
    
    printf("\nTesting deflate compression...\n");
    request = http_client_request_create("GET", "https://httpbin.org/deflate");
    response = http_client_execute(request);
    
    if (response->error_message) {
        printf("✗ Error: %s\n", response->error_message);
    } else {
        printf("✓ Deflate works!\n");
        printf("Status: %d\n", response->status_code);
        printf("Time: %.2f ms\n", response->elapsed_time_ms);
        
        if (response->body && strstr(response->body, "deflated") != NULL) {
            printf("✓ Response successfully decompressed\n");
            printf("Body length: %zu bytes\n", response->body_length);
        }
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
}

/* Helper function to lowercase a string */
char* strtolower(const char *str)
{
    static char buffer[128];
    size_t i;
    for (i = 0; i < sizeof(buffer) - 1 && str[i]; i++) {
        buffer[i] = (str[i] >= 'A' && str[i] <= 'Z') ? str[i] + 32 : str[i];
    }
    buffer[i] = '\0';
    return buffer;
}

int main(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║   Equinox HTTP Client Demo            ║\n");
    printf("║   Making outbound HTTP requests        ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    print_separator();
    
    /* Run demos */
    demo_simple_get();
    print_separator();
    
    demo_get_with_headers();
    print_separator();
    
    demo_post_json();
    print_separator();
    
    demo_different_methods();
    print_separator();
    
    demo_error_handling();
    print_separator();
    
    demo_timeout();
    print_separator();
    
    printf("Demo 7: HTTPS Support\n");
    printf("---------------------\n");
    
    printf("Testing HTTPS connection to httpbin.org...\n");
    HTTP_CLIENT_RESPONSE *https_resp = http_client_get("https://httpbin.org/get");
    
    if (https_resp) {
        if (https_resp->error_message) {
            printf("ERROR: %s\n", https_resp->error_message);
        } else {
            printf("✓ HTTPS works!\n");
            printf("Status: %d\n", https_resp->status_code);
            printf("Time: %.2f ms\n", https_resp->elapsed_time_ms);
            printf("\nFirst 300 chars of response:\n");
            if (https_resp->body) {
                size_t len = https_resp->body_length < 300 ? https_resp->body_length : 300;
                printf("%.*s\n", (int)len, https_resp->body);
            }
        }
        http_client_response_destroy(https_resp);
    }
    
    print_separator();
    
    demo_compression();
    print_separator();
    
    printf("All demos complete!\n");
    printf("\nNote: This demo uses httpbin.org as a test service.\n");
    printf("      Now supports both HTTP and HTTPS!\n");
    
    return 0;
}
