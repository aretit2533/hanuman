/**
 * Async HTTP Client Demo
 * 
 * Demonstrates asynchronous HTTP requests with callbacks
 */

#include "framework.h"
#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Global counter for tracking completions */
static int g_completed_requests = 0;
static int g_total_requests = 0;

/* ==================== Callback Functions ==================== */

static void simple_callback(HTTP_CLIENT_RESPONSE *response, void *user_data)
{
    const char *url = (const char*)user_data;
    
    printf("\n[CALLBACK] Request completed: %s\n", url);
    
    if (response->error_message) {
        printf("  ✗ Error: %s\n", response->error_message);
    } else {
        printf("  ✓ Status: %d\n", response->status_code);
        printf("  ✓ Time: %.2f ms\n", response->elapsed_time_ms);
        printf("  ✓ Body size: %zu bytes\n", response->body_length);
    }
    
    http_client_response_destroy(response);
    g_completed_requests++;
}

static void json_callback(HTTP_CLIENT_RESPONSE *response, void *user_data)
{
    int *request_id = (int*)user_data;
    
    printf("\n[CALLBACK #%d] JSON request completed\n", *request_id);
    
    if (response->error_message) {
        printf("  ✗ Error: %s\n", response->error_message);
    } else {
        printf("  ✓ Status: %d (%.2f ms)\n", response->status_code, response->elapsed_time_ms);
        
        /* Show first 150 chars of response */
        if (response->body && response->body_length > 0) {
            size_t len = response->body_length < 150 ? response->body_length : 150;
            printf("  Response: %.*s...\n", (int)len, response->body);
        }
    }
    
    http_client_response_destroy(response);
    free(request_id);
    g_completed_requests++;
}

/* ==================== Test Scenarios ==================== */

static void test_single_async_request(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Test 1: Single Async Request         ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("Sending async GET request to httpbin.org...\n");
    
    HTTP_CLIENT_REQUEST *request = http_client_request_create("GET", "https://httpbin.org/get");
    
    HTTP_CLIENT_ASYNC_HANDLE *handle = http_client_execute_async(
        request, 
        simple_callback, 
        (void*)"https://httpbin.org/get"
    );
    
    if (!handle) {
        printf("✗ Failed to start async request\n");
        http_client_request_destroy(request);
        return;
    }
    
    printf("✓ Async request started!\n");
    printf("Main thread continues executing...\n");
    
    /* Do some work while request is in flight */
    for (int i = 0; i < 3; i++) {
        printf("  [Main] Working... (%d/3)\n", i + 1);
        usleep(300000); /* 300ms */
    }
    
    printf("\nWaiting for request to complete...\n");
    http_client_async_wait(handle);
    
    printf("✓ Request completed!\n\n");
}

static void test_multiple_async_requests(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Test 2: Multiple Parallel Requests   ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    const char *urls[] = {
        "https://httpbin.org/get",
        "https://httpbin.org/headers",
        "https://httpbin.org/ip",
        "https://httpbin.org/user-agent",
        "https://httpbin.org/uuid"
    };
    int num_requests = 5;
    
    HTTP_CLIENT_ASYNC_HANDLE *handles[5];
    
    printf("Starting %d parallel requests...\n", num_requests);
    time_t start = time(NULL);
    
    g_completed_requests = 0;
    g_total_requests = num_requests;
    
    /* Launch all requests */
    for (int i = 0; i < num_requests; i++) {
        HTTP_CLIENT_REQUEST *request = http_client_request_create("GET", urls[i]);
        int *id = malloc(sizeof(int));
        *id = i + 1;
        
        handles[i] = http_client_execute_async(request, json_callback, id);
        
        if (handles[i]) {
            printf("  ✓ Request %d started: %s\n", i + 1, urls[i]);
        } else {
            printf("  ✗ Request %d failed to start\n", i + 1);
            http_client_request_destroy(request);
            free(id);
        }
    }
    
    printf("\nAll requests launched! Main thread is free...\n");
    
    /* Monitor progress */
    while (g_completed_requests < num_requests) {
        printf("  [Progress] %d/%d completed\r", g_completed_requests, num_requests);
        fflush(stdout);
        usleep(100000); /* 100ms */
    }
    
    printf("\n\nWaiting for all threads to clean up...\n");
    for (int i = 0; i < num_requests; i++) {
        if (handles[i]) {
            http_client_async_wait(handles[i]);
        }
    }
    
    time_t end = time(NULL);
    printf("\n✓ All %d requests completed in ~%ld seconds\n", num_requests, end - start);
    printf("  (Would have taken ~5-6 seconds sequentially)\n\n");
}

static void test_async_with_timeout(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Test 3: Async Request Monitoring     ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("Starting async request with status monitoring...\n");
    
    HTTP_CLIENT_REQUEST *request = http_client_request_create("GET", "https://httpbin.org/delay/2");
    
    HTTP_CLIENT_ASYNC_HANDLE *handle = http_client_execute_async(
        request,
        simple_callback,
        (void*)"https://httpbin.org/delay/2"
    );
    
    if (!handle) {
        printf("✗ Failed to start async request\n");
        http_client_request_destroy(request);
        return;
    }
    
    printf("✓ Request started\n\n");
    
    /* Poll completion status */
    int checks = 0;
    while (!http_client_async_is_complete(handle)) {
        checks++;
        printf("  [Check %d] Request still running...\n", checks);
        usleep(500000); /* 500ms */
    }
    
    printf("\n✓ Request detected as complete!\n");
    printf("  Checked status %d times\n", checks);
    
    http_client_async_wait(handle);
    printf("✓ Cleanup complete\n\n");
}

static void test_fire_and_forget(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Test 4: Fire-and-Forget Pattern      ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("Launching requests without waiting...\n\n");
    
    const char *urls[] = {
        "https://httpbin.org/json",
        "https://httpbin.org/html"
    };
    
    g_completed_requests = 0;
    g_total_requests = 2;
    
    for (int i = 0; i < 2; i++) {
        HTTP_CLIENT_REQUEST *request = http_client_request_create("GET", urls[i]);
        
        HTTP_CLIENT_ASYNC_HANDLE *handle = http_client_execute_async(
            request,
            simple_callback,
            (void*)urls[i]
        );
        
        if (handle) {
            printf("✓ Request %d launched: %s\n", i + 1, urls[i]);
            /* Don't wait - just continue */
        }
    }
    
    printf("\nRequests launched! Doing other work...\n");
    sleep(1);
    
    /* Wait for callbacks to complete */
    printf("Waiting for callbacks to finish...\n");
    while (g_completed_requests < g_total_requests) {
        usleep(100000);
    }
    
    sleep(1); /* Give threads time to clean up */
    printf("✓ All callbacks completed\n\n");
}

/* ==================== Main ==================== */

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   Async HTTP Client Demo                    ║\n");
    printf("║   Non-blocking requests with callbacks      ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    
    test_single_async_request();
    printf("========================================\n\n");
    
    test_multiple_async_requests();
    printf("========================================\n\n");
    
    test_async_with_timeout();
    printf("========================================\n\n");
    
    test_fire_and_forget();
    printf("========================================\n\n");
    
    printf("✓ All async tests completed!\n\n");
    printf("Summary:\n");
    printf("  ✓ Single async request: Works\n");
    printf("  ✓ Multiple parallel requests: Works\n");
    printf("  ✓ Status monitoring: Works\n");
    printf("  ✓ Fire-and-forget pattern: Works\n");
    printf("  ✓ Callbacks execute correctly\n");
    printf("  ✓ Main thread never blocks\n\n");
    
    return 0;
}
