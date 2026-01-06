/**
 * Chunked Transfer Encoding Test
 * 
 * Tests chunked encoding support in HTTP client
 */

#include "framework.h"
#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("Testing Chunked Transfer Encoding\n");
    printf("==================================\n\n");
    
    /* Test 1: Stream endpoint (likely returns chunked) */
    printf("Test 1: Stream endpoint (httpbin.org/stream/3)\n");
    printf("----------------------------------------------\n");
    
    HTTP_CLIENT_REQUEST *request = http_client_request_create(
        "GET", "https://httpbin.org/stream/3");
    
    HTTP_CLIENT_RESPONSE *response = http_client_execute(request);
    
    if (response->error_message) {
        printf("✗ Error: %s\n", response->error_message);
    } else {
        printf("✓ Success!\n");
        printf("Status: %d\n", response->status_code);
        printf("Time: %.2f ms\n", response->elapsed_time_ms);
        printf("Body length: %zu bytes\n", response->body_length);
        
        /* Check headers */
        int found_chunked = 0;
        for (size_t i = 0; i < response->header_count; i++) {
            if (strcasecmp(response->header_names[i], "Transfer-Encoding") == 0) {
                printf("Transfer-Encoding: %s\n", response->header_values[i]);
                if (strstr(response->header_values[i], "chunked") != NULL) {
                    found_chunked = 1;
                }
            }
        }
        
        if (found_chunked) {
            printf("✓ Chunked encoding detected and handled!\n");
        }
        
        /* Show first 200 chars */
        if (response->body && response->body_length > 0) {
            printf("\nFirst 200 chars of response:\n");
            size_t len = response->body_length < 200 ? response->body_length : 200;
            printf("%.*s\n", (int)len, response->body);
        }
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
    
    printf("\n========================================\n\n");
    
    /* Test 2: Gzip with potential chunking */
    printf("Test 2: Gzip compression\n");
    printf("-------------------------\n");
    
    request = http_client_request_create("GET", "https://httpbin.org/gzip");
    response = http_client_execute(request);
    
    if (response->error_message) {
        printf("✗ Error: %s\n", response->error_message);
    } else {
        printf("✓ Success!\n");
        printf("Status: %d\n", response->status_code);
        printf("Body length: %zu bytes\n", response->body_length);
        
        /* Check for successful decompression */
        if (response->body && strstr(response->body, "gzipped") != NULL) {
            printf("✓ Gzip decompression successful!\n");
        }
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
    
    printf("\n✓ All tests completed!\n");
    return 0;
}
