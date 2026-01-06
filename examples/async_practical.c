/**
 * Practical Async HTTP Examples
 * 
 * Real-world async patterns for the HTTP client
 */

#include "framework.h"
#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* ==================== Example 1: Async Data Aggregation ==================== */

typedef struct {
    int weather_done;
    int news_done;
    int stocks_done;
    pthread_mutex_t mutex;
} dashboard_state_t;

static void weather_callback(HTTP_CLIENT_RESPONSE *response, void *user_data)
{
    dashboard_state_t *state = (dashboard_state_t*)user_data;
    
    printf("[Weather API] ");
    if (response->error_message) {
        printf("Failed: %s\n", response->error_message);
    } else {
        printf("✓ Received (Status: %d, %.0f ms)\n", 
               response->status_code, response->elapsed_time_ms);
    }
    
    pthread_mutex_lock(&state->mutex);
    state->weather_done = 1;
    pthread_mutex_unlock(&state->mutex);
    
    http_client_response_destroy(response);
}

static void news_callback(HTTP_CLIENT_RESPONSE *response, void *user_data)
{
    dashboard_state_t *state = (dashboard_state_t*)user_data;
    
    printf("[News API] ");
    if (response->error_message) {
        printf("Failed: %s\n", response->error_message);
    } else {
        printf("✓ Received (Status: %d, %.0f ms, %zu bytes)\n", 
               response->status_code, response->elapsed_time_ms, response->body_length);
    }
    
    pthread_mutex_lock(&state->mutex);
    state->news_done = 1;
    pthread_mutex_unlock(&state->mutex);
    
    http_client_response_destroy(response);
}

static void stocks_callback(HTTP_CLIENT_RESPONSE *response, void *user_data)
{
    dashboard_state_t *state = (dashboard_state_t*)user_data;
    
    printf("[Stocks API] ");
    if (response->error_message) {
        printf("Failed: %s\n", response->error_message);
    } else {
        printf("✓ Received (Status: %d, %.0f ms)\n", 
               response->status_code, response->elapsed_time_ms);
    }
    
    pthread_mutex_lock(&state->mutex);
    state->stocks_done = 1;
    pthread_mutex_unlock(&state->mutex);
    
    http_client_response_destroy(response);
}

static void example_dashboard_aggregation(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Example 1: Dashboard Data Aggregation║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("Scenario: Loading dashboard from 3 different APIs\n");
    printf("Loading weather, news, and stocks data in parallel...\n\n");
    
    dashboard_state_t state = {0, 0, 0};
    pthread_mutex_init(&state.mutex, NULL);
    
    /* Launch 3 parallel requests to different endpoints */
    HTTP_CLIENT_REQUEST *weather_req = http_client_request_create("GET", "https://httpbin.org/uuid");
    HTTP_CLIENT_REQUEST *news_req = http_client_request_create("GET", "https://httpbin.org/json");
    HTTP_CLIENT_REQUEST *stocks_req = http_client_request_create("GET", "https://httpbin.org/headers");
    
    http_client_execute_async(weather_req, weather_callback, &state);
    http_client_execute_async(news_req, news_callback, &state);
    http_client_execute_async(stocks_req, stocks_callback, &state);
    
    /* Wait for all to complete */
    printf("Waiting for all APIs to respond...\n");
    while (1) {
        pthread_mutex_lock(&state.mutex);
        int all_done = state.weather_done && state.news_done && state.stocks_done;
        pthread_mutex_unlock(&state.mutex);
        
        if (all_done) break;
        usleep(50000);
    }
    
    sleep(1); /* Let threads clean up */
    pthread_mutex_destroy(&state.mutex);
    
    printf("\n✓ Dashboard fully loaded!\n");
    printf("  All 3 data sources fetched in parallel\n\n");
}

/* ==================== Example 2: Request Queue Processing ==================== */

typedef struct {
    int processed;
    int failed;
    pthread_mutex_t mutex;
} queue_stats_t;

static void queue_callback(HTTP_CLIENT_RESPONSE *response, void *user_data)
{
    queue_stats_t *stats = (queue_stats_t*)user_data;
    
    pthread_mutex_lock(&stats->mutex);
    if (response->error_message) {
        stats->failed++;
        printf("  ✗ Request failed: %s\n", response->error_message);
    } else {
        stats->processed++;
        printf("  ✓ Request completed: %d (%.0f ms)\n", 
               response->status_code, response->elapsed_time_ms);
    }
    pthread_mutex_unlock(&stats->mutex);
    
    http_client_response_destroy(response);
}

static void example_request_queue(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Example 2: Background Request Queue  ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("Scenario: Processing 8 API calls in the background\n");
    printf("Processing queue...\n\n");
    
    queue_stats_t stats = {0, 0};
    pthread_mutex_init(&stats.mutex, NULL);
    
    const char *endpoints[] = {
        "https://httpbin.org/get",
        "https://httpbin.org/uuid",
        "https://httpbin.org/headers",
        "https://httpbin.org/ip",
        "https://httpbin.org/user-agent",
        "https://httpbin.org/json",
        "https://httpbin.org/html",
        "https://httpbin.org/robots.txt"
    };
    
    int total = sizeof(endpoints) / sizeof(endpoints[0]);
    
    /* Launch all requests */
    for (int i = 0; i < total; i++) {
        HTTP_CLIENT_REQUEST *req = http_client_request_create("GET", endpoints[i]);
        http_client_execute_async(req, queue_callback, &stats);
    }
    
    printf("Queue processing started...\n\n");
    
    /* Monitor progress */
    int last_total = 0;
    while (1) {
        pthread_mutex_lock(&stats.mutex);
        int current_total = stats.processed + stats.failed;
        pthread_mutex_unlock(&stats.mutex);
        
        if (current_total != last_total) {
            printf("Progress: %d/%d completed\n", current_total, total);
            last_total = current_total;
        }
        
        if (current_total >= total) break;
        usleep(100000);
    }
    
    sleep(1);
    
    pthread_mutex_lock(&stats.mutex);
    printf("\n✓ Queue processing complete!\n");
    printf("  Success: %d/%d\n", stats.processed, total);
    printf("  Failed: %d/%d\n", stats.failed, total);
    pthread_mutex_unlock(&stats.mutex);
    
    pthread_mutex_destroy(&stats.mutex);
    printf("\n");
}

/* ==================== Example 3: Async POST with Result Processing ==================== */

static void post_callback(HTTP_CLIENT_RESPONSE *response, void *user_data)
{
    const char *operation = (const char*)user_data;
    
    printf("[%s] ", operation);
    
    if (response->error_message) {
        printf("Failed: %s\n", response->error_message);
    } else {
        printf("Success! Status: %d (%.0f ms)\n", 
               response->status_code, response->elapsed_time_ms);
        
        if (response->body && strstr(response->body, "\"json\"")) {
            printf("  Server received JSON data correctly\n");
        }
    }
    
    http_client_response_destroy(response);
}

static void example_async_post(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Example 3: Async POST Operations     ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("Scenario: Submitting multiple forms asynchronously\n");
    printf("Sending data...\n\n");
    
    /* Create POST requests with JSON */
    const char *json_data = "{\"user\": \"async_test\", \"action\": \"submit\"}";
    
    HTTP_CLIENT_REQUEST *req1 = http_client_request_create("POST", "https://httpbin.org/post");
    http_client_request_add_header(req1, "Content-Type", "application/json");
    http_client_request_set_body(req1, json_data, 0);
    
    HTTP_CLIENT_REQUEST *req2 = http_client_request_create("POST", "https://httpbin.org/post");
    http_client_request_add_header(req2, "Content-Type", "application/json");
    http_client_request_set_body(req2, "{\"type\": \"analytics\", \"event\": \"page_view\"}", 0);
    
    http_client_execute_async(req1, post_callback, (void*)"Form Submission");
    http_client_execute_async(req2, post_callback, (void*)"Analytics Event");
    
    printf("Requests sent! Main thread continues...\n\n");
    
    /* Wait for responses */
    sleep(3);
    
    printf("\n✓ All POST operations completed\n\n");
}

/* ==================== Main ==================== */

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   Practical Async HTTP Examples             ║\n");
    printf("║   Real-world use cases                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    
    example_dashboard_aggregation();
    printf("========================================\n\n");
    
    example_request_queue();
    printf("========================================\n\n");
    
    example_async_post();
    printf("========================================\n\n");
    
    printf("✓ All examples completed!\n\n");
    printf("Key Benefits:\n");
    printf("  • Non-blocking: Main thread stays responsive\n");
    printf("  • Parallel execution: Multiple requests at once\n");
    printf("  • Callbacks: Clean result handling\n");
    printf("  • Flexible: Fire-and-forget or wait patterns\n\n");
    
    return 0;
}
