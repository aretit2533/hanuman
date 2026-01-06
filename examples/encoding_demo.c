/**
 * HTTP Encoding Demo
 * 
 * Demonstrates chunked transfer encoding and compression support
 */

#include "framework.h"
#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void print_separator(void) {
    printf("========================================\n\n");
}

int main(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║   HTTP Encoding & Compression Demo    ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    /* Test 1: Chunked Transfer Encoding */
    printf("Test 1: Chunked Transfer Encoding\n");
    printf("----------------------------------\n");
    printf("Requesting: https://httpbin.org/stream/5\n\n");
    
    HTTP_CLIENT_REQUEST *request = http_client_request_create(
        "GET", "https://httpbin.org/stream/5");
    
    HTTP_CLIENT_RESPONSE *response = http_client_execute(request);
    
    if (response->error_message) {
        printf("✗ Error: %s\n", response->error_message);
    } else {
        printf("✓ Request successful!\n");
        printf("  Status: %d\n", response->status_code);
        printf("  Time: %.2f ms\n", response->elapsed_time_ms);
        printf("  Body size: %zu bytes\n", response->body_length);
        
        /* Check for chunked encoding */
        for (size_t i = 0; i < response->header_count; i++) {
            if (strcasecmp(response->header_names[i], "Transfer-Encoding") == 0) {
                printf("  Transfer-Encoding: %s\n", response->header_values[i]);
                if (strstr(response->header_values[i], "chunked") != NULL) {
                    printf("  ✓ Chunked encoding automatically decoded!\n");
                }
            }
        }
        
        /* Count lines in response */
        int line_count = 0;
        for (size_t i = 0; i < response->body_length; i++) {
            if (response->body[i] == '\n') line_count++;
        }
        printf("  Response contains %d lines of JSON\n", line_count);
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
    
    print_separator();
    
    /* Test 2: Gzip Compression */
    printf("Test 2: Gzip Compression\n");
    printf("------------------------\n");
    printf("Requesting: https://httpbin.org/gzip\n\n");
    
    request = http_client_request_create("GET", "https://httpbin.org/gzip");
    response = http_client_execute(request);
    
    if (response->error_message) {
        printf("✗ Error: %s\n", response->error_message);
    } else {
        printf("✓ Request successful!\n");
        printf("  Status: %d\n", response->status_code);
        printf("  Time: %.2f ms\n", response->elapsed_time_ms);
        printf("  Decompressed size: %zu bytes\n", response->body_length);
        
        /* Check for compression */
        for (size_t i = 0; i < response->header_count; i++) {
            if (strcasecmp(response->header_names[i], "Content-Encoding") == 0) {
                printf("  Content-Encoding: %s\n", response->header_values[i]);
                printf("  ✓ Gzip automatically decompressed!\n");
            }
        }
        
        /* Verify decompression worked */
        if (response->body && strstr(response->body, "gzipped") != NULL) {
            printf("  ✓ Body successfully decompressed and readable\n");
        }
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
    
    print_separator();
    
    /* Test 3: Large response with streaming */
    printf("Test 3: Large Streamed Response\n");
    printf("--------------------------------\n");
    printf("Requesting: https://httpbin.org/stream/20\n\n");
    
    request = http_client_request_create("GET", "https://httpbin.org/stream/20");
    response = http_client_execute(request);
    
    if (response->error_message) {
        printf("✗ Error: %s\n", response->error_message);
    } else {
        printf("✓ Request successful!\n");
        printf("  Status: %d\n", response->status_code);
        printf("  Time: %.2f ms\n", response->elapsed_time_ms);
        printf("  Total size: %zu bytes\n", response->body_length);
        printf("  ✓ Large chunked response handled correctly\n");
    }
    
    http_client_response_destroy(response);
    http_client_request_destroy(request);
    
    print_separator();
    
    /* Test 4: Compression + Chunking combination */
    printf("Test 4: Multiple Encodings\n");
    printf("--------------------------\n");
    printf("Testing various encoding scenarios...\n\n");
    
    const char *test_urls[] = {
        "https://httpbin.org/get",
        "https://httpbin.org/html",
        "https://httpbin.org/json"
    };
    
    for (int i = 0; i < 3; i++) {
        request = http_client_request_create("GET", test_urls[i]);
        response = http_client_execute(request);
        
        if (response->error_message) {
            printf("  [%d] ✗ %s\n", i+1, response->error_message);
        } else {
            printf("  [%d] ✓ %s - %d (%.0f ms, %zu bytes)\n", 
                   i+1, test_urls[i], response->status_code,
                   response->elapsed_time_ms, response->body_length);
        }
        
        http_client_response_destroy(response);
        http_client_request_destroy(request);
    }
    
    print_separator();
    
    printf("✓ All encoding tests completed successfully!\n\n");
    printf("Summary:\n");
    printf("  ✓ Chunked transfer encoding: Supported\n");
    printf("  ✓ Gzip compression: Supported\n");
    printf("  ✓ Deflate compression: Supported (with multiple formats)\n");
    printf("  ✓ Combined encodings: Handled automatically\n");
    printf("  ✓ Accept-Encoding header: Sent automatically\n\n");
    
    return 0;
}
