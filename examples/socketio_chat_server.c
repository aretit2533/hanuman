/*
 * Socket.IO Chat Server Demo
 * Real-time chat application with rooms
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "socketio.h"
#include "json_parser.h"
#include "application.h"
#include "framework.h"

static APPLICATION *app = NULL;
static SOCKETIO_SERVER *sio_server = NULL;

typedef struct {
    char username[64];
    char room[64];
} user_data_t;

void handle_signal(int sig) {
    (void)sig;
    if (app) {
        application_stop(app);
    }
}

/* Get current timestamp */
static void get_timestamp(char *buf, size_t buf_len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, buf_len, "%H:%M:%S", tm_info);
}

/* Handle 'join' event - User joins a room */
void on_join(SOCKETIO_SOCKET *socket, SIO_EVENT *event, void *user_data) {
    (void)user_data;
    
    if (!event->data) return;
    
    /* Parse JSON: {"username": "Alice", "room": "general"} */
    JSON_VALUE *json = json_parse(event->data);
    if (!json) return;
    
    user_data_t *udata = (user_data_t*)calloc(1, sizeof(user_data_t));
    
    /* Parse username and room from JSON using path notation */
    JSON_VALUE *username_val = json_get_path(json, "username");
    JSON_VALUE *room_val = json_get_path(json, "room");
    
    const char *username = username_val ? json_get_string(username_val) : "Anonymous";
    const char *room = room_val ? json_get_string(room_val) : "general";
    
    if (username) strncpy(udata->username, username, sizeof(udata->username) - 1);
    if (room) strncpy(udata->room, room, sizeof(udata->room) - 1);
    
    sio_set_user_data(socket, udata);
    
    /* Join the room */
    sio_join(socket, udata->room);
    
    /* Notify user */
    char response[256];
    snprintf(response, sizeof(response), 
            "{\"message\":\"You joined room '%s'\",\"room\":\"%s\"}",
            udata->room, udata->room);
    sio_emit(socket, "joined", response);
    
    /* Notify others in room */
    char timestamp[16];
    get_timestamp(timestamp, sizeof(timestamp));
    
    char broadcast[512];
    snprintf(broadcast, sizeof(broadcast),
            "{\"username\":\"System\",\"message\":\"%s joined the room\",\"timestamp\":\"%s\"}",
            udata->username, timestamp);
    sio_broadcast(socket, "message", broadcast);
    
    printf("✓ %s joined room '%s'\n", udata->username, udata->room);
    
    json_free(json);
}

/* Handle 'message' event - User sends a message */
void on_chat_message(SOCKETIO_SOCKET *socket, SIO_EVENT *event, void *user_data) {
    (void)user_data;
    
    user_data_t *udata = (user_data_t*)sio_get_user_data(socket);
    if (!udata || !event->data) return;
    
    /* Parse JSON: {"message": "Hello!"} */
    JSON_VALUE *json = json_parse(event->data);
    if (!json) return;
    
    JSON_VALUE *message_val = json_get_path(json, "message");
    const char *message = message_val ? json_get_string(message_val) : NULL;
    if (message) {
        char timestamp[16];
        get_timestamp(timestamp, sizeof(timestamp));
        
        /* Broadcast to room */
        char response[1024];
        snprintf(response, sizeof(response),
                "{\"username\":\"%s\",\"message\":\"%s\",\"timestamp\":\"%s\"}",
                udata->username, message, timestamp);
        
        sio_broadcast(socket, "message", response);
        
        /* Also send to sender */
        sio_emit(socket, "message", response);
        
        printf("[%s] %s: %s\n", timestamp, udata->username, message);
    }
    
    json_free(json);
}

/* Handle 'leave' event - User leaves a room */
void on_leave(SOCKETIO_SOCKET *socket, SIO_EVENT *event, void *user_data) {
    (void)event;
    (void)user_data;
    
    user_data_t *udata = (user_data_t*)sio_get_user_data(socket);
    if (!udata) return;
    
    /* Notify others in room */
    char timestamp[16];
    get_timestamp(timestamp, sizeof(timestamp));
    
    char broadcast[512];
    snprintf(broadcast, sizeof(broadcast),
            "{\"username\":\"System\",\"message\":\"%s left the room\",\"timestamp\":\"%s\"}",
            udata->username, timestamp);
    sio_broadcast(socket, "message", broadcast);
    
    /* Leave room */
    sio_leave(socket, udata->room);
    
    printf("✗ %s left room '%s'\n", udata->username, udata->room);
    
    free(udata);
    sio_set_user_data(socket, NULL);
}

/* Handle 'typing' event - User is typing */
void on_typing(SOCKETIO_SOCKET *socket, SIO_EVENT *event, void *user_data) {
    (void)event;
    (void)user_data;
    
    user_data_t *udata = (user_data_t*)sio_get_user_data(socket);
    if (!udata) return;
    
    /* Broadcast typing indicator */
    char response[256];
    snprintf(response, sizeof(response), "{\"username\":\"%s\"}", udata->username);
    sio_broadcast(socket, "typing", response);
}

void on_connect(SOCKETIO_SOCKET *socket, void *user_data) {
    (void)user_data;
    
    printf("✓ Client connected (ID: %s)\n", sio_get_id(socket));
    
    /* Send server info */
    time_t now = time(NULL);
    char info[256];
    snprintf(info, sizeof(info), 
            "{\"server\":\"Socket.IO Chat Server\",\"time\":%ld}", now);
    sio_emit(socket, "server_info", info);
}

void on_disconnect(SOCKETIO_SOCKET *socket, const char *reason, void *user_data) {
    (void)user_data;
    
    user_data_t *udata = (user_data_t*)sio_get_user_data(socket);
    if (udata) {
        printf("✗ %s disconnected (reason: %s)\n", udata->username, reason);
        free(udata);
    } else {
        printf("✗ Client disconnected (ID: %s, reason: %s)\n", 
               sio_get_id(socket), reason);
    }
}

int main(int argc, char *argv[]) {
    int port = 3000;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    /* Setup signal handler */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("=== Socket.IO Chat Server ===\n\n");
    
    /* Initialize framework */
    framework_init();
    
    /* Create application */
    app = application_create("SocketIOChatServer", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Configure server */
    SIO_SERVER_CONFIG config = {
        .host = NULL,
        .port = port,
        .path = "/socket.io",
        .max_connections = 100,
        .ping_interval = 25000,
        .ping_timeout = 20000,
        .allow_upgrades = true,
        .cors_origin = "*"
    };
    
    /* Create server */
    sio_server = sio_server_create(&config);
    if (!sio_server) {
        fprintf(stderr, "Failed to create Socket.IO server\n");
        application_destroy(app);
        framework_shutdown();
        return 1;
    }
    
    /* Set global callbacks */
    SIO_CALLBACKS callbacks = {
        .on_connect = on_connect,
        .on_disconnect = on_disconnect,
        .on_event = NULL,
        .on_error = NULL,
        .user_data = NULL
    };
    sio_server_set_callbacks(sio_server, &callbacks);
    
    /* Register event handlers */
    sio_server_on(sio_server, "join", on_join, NULL);
    sio_server_on(sio_server, "message", on_chat_message, NULL);
    sio_server_on(sio_server, "leave", on_leave, NULL);
    sio_server_on(sio_server, "typing", on_typing, NULL);
    
    /* Register Socket.IO server with application */
    application_set_socketio_server(app, sio_server);
    
    /* Start server */
    if (sio_server_start(sio_server) != 0) {
        fprintf(stderr, "Failed to start Socket.IO server\n");
        sio_server_destroy(sio_server);
        application_destroy(app);
        framework_shutdown();
        return 1;
    }
    
    printf("Server started on http://localhost:%d/socket.io\n", port);
    printf("\nAvailable events:\n");
    printf("  - join: {\"username\": \"name\", \"room\": \"room_name\"}\n");
    printf("  - message: {\"message\": \"your message\"}\n");
    printf("  - typing: {}\n");
    printf("  - leave: {}\n");
    printf("\nPress Ctrl+C to stop\n\n");
    
    /* Run application with unified event loop */
    application_run(app);
    
    printf("\n\nShutting down...\n");
    
    /* Cleanup */
    sio_server_stop(sio_server);
    sio_server_destroy(sio_server);
    application_destroy(app);
    framework_shutdown();
    
    printf("Server stopped\n");
    return 0;
}
