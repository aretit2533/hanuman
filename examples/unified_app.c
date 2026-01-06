/*
 * Unified Application Demo
 * Demonstrates application_run() managing both HTTP and Kafka in one event loop
 */

#include "application.h"
#include "framework.h"
#include "http_server.h"
#include "kafka_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Application context for shared data */
typedef struct {
    int message_count;
    char last_kafka_message[512];
} APP_CONTEXT;

/* ==================== HTTP Route Handlers ==================== */

void handle_root(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>Unified App</title></head>\n"
        "<body>\n"
        "<h1>Unified Application Demo</h1>\n"
        "<p>This application runs both HTTP and Kafka in a single event loop.</p>\n"
        "<ul>\n"
        "<li><a href='/api/status'>GET /api/status</a> - Application status</li>\n"
        "<li><a href='/api/kafka/stats'>GET /api/kafka/stats</a> - Kafka statistics</li>\n"
        "<li>POST /api/kafka/send - Send Kafka message</li>\n"
        "</ul>\n"
        "</body>\n"
        "</html>";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/html");
    http_response_set_body(response, html, strlen(html));
}

void handle_status(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    APP_CONTEXT *ctx = (APP_CONTEXT*)user_data;
    
    char json[1024];
    snprintf(json, sizeof(json),
            "{"
            "\"status\": \"running\","
            "\"framework\": \"Equinox Unified\","
            "\"http\": \"enabled\","
            "\"kafka\": \"enabled\","
            "\"message_count\": %d,"
            "\"timestamp\": %ld"
            "}", ctx ? ctx->message_count : 0, (long)time(NULL));
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

void handle_kafka_stats(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    APP_CONTEXT *ctx = (APP_CONTEXT*)user_data;
    
    char json[1024];
    snprintf(json, sizeof(json),
            "{"
            "\"messages_received\": %d,"
            "\"last_message\": \"%s\","
            "\"timestamp\": %ld"
            "}", 
            ctx ? ctx->message_count : 0,
            ctx ? ctx->last_kafka_message : "",
            (long)time(NULL));
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_set_json(response, json);
}

void handle_kafka_send(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    APPLICATION *app = (APPLICATION*)user_data;
    
    if (!app || !app->kafka_client) {
        http_response_set_status(response, HTTP_STATUS_INTERNAL_ERROR);
        http_response_set_json(response, "{\"error\": \"Kafka not available\"}");
        return;
    }
    
    /* Get message from POST body */
    const char *message = request->body_length > 0 ? request->body : "{}";
    
    /* Send to Kafka */
    char payload[512];
    snprintf(payload, sizeof(payload), 
            "{\"data\": %s, \"timestamp\": %ld}", 
            message, (long)time(NULL));
    
    int result = kafka_produce_string(app->kafka_client, "api-events", NULL, payload);
    
    if (result == FRAMEWORK_SUCCESS) {
        http_response_set_status(response, HTTP_STATUS_OK);
        http_response_set_json(response, "{\"status\": \"sent\", \"topic\": \"api-events\"}");
    } else {
        http_response_set_status(response, HTTP_STATUS_INTERNAL_ERROR);
        http_response_set_json(response, "{\"error\": \"Failed to send message\"}");
    }
}

/* ==================== Kafka Message Handlers ==================== */

void handle_user_events(KAFKA_MESSAGE *message, void *user_data)
{
    APP_CONTEXT *ctx = (APP_CONTEXT*)user_data;
    
    const char *payload = kafka_message_get_payload_string(message);
    
    framework_log(LOG_LEVEL_INFO, "Received user event: %s", 
                 payload ? payload : "(null)");
    
    if (ctx) {
        ctx->message_count++;
        if (payload) {
            strncpy(ctx->last_kafka_message, payload, sizeof(ctx->last_kafka_message) - 1);
        }
    }
}

void handle_api_events(KAFKA_MESSAGE *message, void *user_data)
{
    APP_CONTEXT *ctx = (APP_CONTEXT*)user_data;
    
    const char *payload = kafka_message_get_payload_string(message);
    
    framework_log(LOG_LEVEL_INFO, "Received API event: %s", 
                 payload ? payload : "(null)");
    
    if (ctx) {
        ctx->message_count++;
    }
}

/* ==================== Main Application ==================== */

int main(int argc, char *argv[])
{
    const char *http_host = "0.0.0.0";
    int http_port = 8080;
    const char *kafka_broker = "localhost:9092";
    
    /* Parse command line arguments */
    if (argc > 1) kafka_broker = argv[1];
    if (argc > 2) http_port = atoi(argv[2]);
    
    /* Initialize framework */
    framework_init();
    
    /* Create application */
    APPLICATION *app = application_create("Unified HTTP+Kafka App", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Create application context */
    APP_CONTEXT *ctx = calloc(1, sizeof(APP_CONTEXT));
    if (!ctx) {
        fprintf(stderr, "Failed to allocate context\n");
        application_destroy(app);
        return 1;
    }
    ctx->message_count = 0;
    strcpy(ctx->last_kafka_message, "none");
    
    /* ==================== Setup HTTP Server ==================== */
    
    HTTP_SERVER *http_server = http_server_create(http_host, http_port);
    if (!http_server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        free(ctx);
        application_destroy(app);
        return 1;
    }
    
    /* Register HTTP routes */
    http_server_add_route(http_server, HTTP_METHOD_GET, "/", handle_root, ctx);
    http_server_add_route(http_server, HTTP_METHOD_GET, "/api/status", handle_status, ctx);
    http_server_add_route(http_server, HTTP_METHOD_GET, "/api/kafka/stats", handle_kafka_stats, ctx);
    http_server_add_route(http_server, HTTP_METHOD_POST, "/api/kafka/send", handle_kafka_send, app);
    
    /* ==================== Setup Kafka Client ==================== */
    
    KAFKA_CLIENT *kafka = kafka_client_create();
    if (!kafka) {
        fprintf(stderr, "Failed to create Kafka client\n");
        http_server_destroy(http_server);
        free(ctx);
        application_destroy(app);
        return 1;
    }
    
    /* Configure and register Kafka consumers */
    KAFKA_CONSUMER_CONFIG consumer_config;
    kafka_consumer_config_default(&consumer_config, kafka_broker, "unified-app-group");
    
    if (kafka_consumer_register(kafka, "user-events", &consumer_config, 
                                handle_user_events, ctx) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register Kafka consumer for user-events\n");
        kafka_client_destroy(kafka);
        http_server_destroy(http_server);
        free(ctx);
        application_destroy(app);
        return 1;
    }
    
    if (kafka_consumer_register(kafka, "api-events", &consumer_config,
                                handle_api_events, ctx) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register Kafka consumer for api-events\n");
        kafka_client_destroy(kafka);
        http_server_destroy(http_server);
        free(ctx);
        application_destroy(app);
        return 1;
    }
    
    /* Initialize Kafka producer */
    KAFKA_PRODUCER_CONFIG producer_config;
    kafka_producer_config_default(&producer_config, kafka_broker);
    
    if (kafka_producer_init(kafka, &producer_config) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to initialize Kafka producer\n");
        kafka_client_destroy(kafka);
        http_server_destroy(http_server);
        free(ctx);
        application_destroy(app);
        return 1;
    }
    
    /* ==================== Register with Application ==================== */
    
    application_set_http_server(app, http_server);
    application_set_kafka_client(app, kafka);
    
    /* ==================== Run Unified Event Loop ==================== */
    
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Unified Application Running                             ║\n");
    printf("║  ─────────────────────────────────────────────────────── ║\n");
    printf("║  HTTP Server: http://localhost:%d                      ║\n", http_port);
    printf("║  Kafka Broker: %-41s ║\n", kafka_broker);
    printf("║  ─────────────────────────────────────────────────────── ║\n");
    printf("║  Consuming from:                                         ║\n");
    printf("║    • user-events                                         ║\n");
    printf("║    • api-events                                          ║\n");
    printf("║  ─────────────────────────────────────────────────────── ║\n");
    printf("║  Try:                                                    ║\n");
    printf("║    curl http://localhost:%d/api/status                 ║\n", http_port);
    printf("║    curl http://localhost:%d/api/kafka/stats            ║\n", http_port);
    printf("║    curl -X POST http://localhost:%d/api/kafka/send     ║\n", http_port);
    printf("║         -d '{\"test\":\"message\"}'                          ║\n");
    printf("║  ─────────────────────────────────────────────────────── ║\n");
    printf("║  Press Ctrl+C to stop                                    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    /* Run unified event loop - handles both HTTP and Kafka */
    int result = application_run(app);
    
    /* ==================== Cleanup ==================== */
    
    printf("\nShutting down...\n");
    
    kafka_client_destroy(kafka);
    http_server_destroy(http_server);
    free(ctx);
    application_destroy(app);
    framework_shutdown();
    
    printf("Application stopped successfully\n");
    return result == FRAMEWORK_SUCCESS ? 0 : 1;
}
