/*
 * WebSocket Echo Server Demo
 * Echoes back any message received from clients
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "websocket.h"
#include "application.h"
#include "framework.h"

static APPLICATION *app = NULL;
static WEBSOCKET_SERVER *ws_server = NULL;

void handle_signal(int sig) {
    (void)sig;
    if (app) {
        application_stop(app);
    }
}

void on_connect(WEBSOCKET_CONNECTION *conn, void *user_data) {
    (void)user_data;
    
    char addr[64];
    ws_get_remote_addr(conn, addr, sizeof(addr));
    printf("✓ Client connected from %s\n", addr);
    
    /* Send welcome message */
    ws_send_text(conn, "Welcome to WebSocket Echo Server!");
}

void on_message(WEBSOCKET_CONNECTION *conn, WS_MESSAGE *message, void *user_data) {
    (void)user_data;
    
    if (message->opcode == WS_OPCODE_TEXT) {
        /* Echo text message */
        char *text = (char*)malloc(message->length + 1);
        memcpy(text, message->data, message->length);
        text[message->length] = '\0';
        
        printf("Received: %s\n", text);
        
        /* Echo back */
        char response[1024];
        snprintf(response, sizeof(response), "Echo: %s", text);
        ws_send_text(conn, response);
        
        free(text);
    } else if (message->opcode == WS_OPCODE_BINARY) {
        /* Echo binary message */
        printf("Received binary message (%zu bytes)\n", message->length);
        ws_send_binary(conn, message->data, message->length);
    }
}

void on_close(WEBSOCKET_CONNECTION *conn, WS_CLOSE_STATUS status, 
             const char *reason, void *user_data) {
    (void)user_data;
    
    char addr[64];
    ws_get_remote_addr(conn, addr, sizeof(addr));
    printf("✗ Client disconnected from %s (status: %d, reason: %s)\n",
           addr, status, reason ? reason : "none");
}

void on_error(WEBSOCKET_CONNECTION *conn, const char *error, void *user_data) {
    (void)conn;
    (void)user_data;
    
    printf("Error: %s\n", error);
}

int main(int argc, char *argv[]) {
    int port = 8080;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    /* Setup signal handler */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("=== WebSocket Echo Server ===\n\n");
    
    /* Initialize framework */
    framework_init();
    
    /* Create application */
    app = application_create("WebSocketEchoServer", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Configure server */
    WS_SERVER_CONFIG config = {
        .host = NULL,  /* Listen on all interfaces */
        .port = port,
        .path = "/ws",
        .max_connections = 100,
        .max_message_size = 10 * 1024 * 1024,  /* 10MB */
        .ping_interval = 30,  /* Ping every 30 seconds */
        .ping_timeout = 10    /* Timeout after 10 seconds */
    };
    
    /* Create server */
    ws_server = ws_server_create(&config);
    if (!ws_server) {
        fprintf(stderr, "Failed to create WebSocket server\n");
        application_destroy(app);
        framework_shutdown();
        return 1;
    }
    
    /* Set callbacks */
    WS_CALLBACKS callbacks = {
        .on_connect = on_connect,
        .on_message = on_message,
        .on_close = on_close,
        .on_error = on_error,
        .user_data = NULL
    };
    ws_server_set_callbacks(ws_server, &callbacks);
    
    /* Register WebSocket server with application */
    application_set_websocket_server(app, ws_server);
    
    /* Start server */
    if (ws_server_start(ws_server) != 0) {
        fprintf(stderr, "Failed to start WebSocket server\n");
        ws_server_destroy(ws_server);
        application_destroy(app);
        framework_shutdown();
        return 1;
    }
    
    printf("Server started on ws://0.0.0.0:%d/ws\n", port);
    printf("Connect using: wscat -c ws://localhost:%d/ws\n", port);
    printf("Press Ctrl+C to stop\n\n");
    
    /* Run application with unified event loop */
    application_run(app);
    
    printf("\n\nShutting down...\n");
    
    /* Cleanup */
    ws_server_stop(ws_server);
    ws_server_destroy(ws_server);
    application_destroy(app);
    framework_shutdown();
    
    printf("Server stopped\n");
    return 0;
}
