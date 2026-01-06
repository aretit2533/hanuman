# Unified Event Loop

The Equinox framework provides `application_run()` - a unified event loop that handles both HTTP and Kafka in a single function, making it easy to build applications that use both technologies.

## Overview

Instead of manually managing HTTP server and Kafka client lifecycles, you can use `application_run()` to:
- Start HTTP server and/or Kafka client automatically
- Handle signal-based graceful shutdown (SIGINT, SIGTERM)
- Run the event loop
- Stop everything cleanly on exit

## Basic Usage

```c
/* Create application */
APPLICATION *app = application_create("My App", 1);

/* Create and configure HTTP server */
HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
http_server_add_route(server, HTTP_METHOD_GET, "/", handle_root, NULL);

/* Create and configure Kafka client */
KAFKA_CLIENT *kafka = kafka_client_create();
KAFKA_CONSUMER_CONFIG config;
kafka_consumer_config_default(&config, "localhost:9092", "my-group");
kafka_consumer_register(kafka, "my-topic", &config, handle_message, NULL);

/* Register with application */
application_set_http_server(app, server);
application_set_kafka_client(app, kafka);

/* Run unified event loop - handles both HTTP and Kafka */
application_run(app);  // Blocks until Ctrl+C

/* Cleanup */
kafka_client_destroy(kafka);
http_server_destroy(server);
application_destroy(app);
```

## API Reference

### `application_set_http_server()`
```c
void application_set_http_server(APPLICATION *app, HTTP_SERVER *server);
```
Associates an HTTP server with the application.

**Parameters:**
- `app` - Application instance
- `server` - HTTP server to manage

### `application_set_kafka_client()`
```c
void application_set_kafka_client(APPLICATION *app, KAFKA_CLIENT *kafka);
```
Associates a Kafka client with the application.

**Parameters:**
- `app` - Application instance
- `kafka` - Kafka client to manage

### `application_run()`
```c
int application_run(APPLICATION *app);
```
Runs the unified event loop managing HTTP and/or Kafka.

**Behavior:**
- Starts HTTP server if configured
- Starts Kafka client if configured
- Registers signal handlers (SIGINT, SIGTERM)
- Runs event loop (blocks)
- Stops everything gracefully on signal

**Returns:**
- `FRAMEWORK_SUCCESS` on clean shutdown
- `FRAMEWORK_ERROR_NULL_PTR` if app is NULL
- `FRAMEWORK_ERROR_INVALID` if neither HTTP nor Kafka is configured
- `FRAMEWORK_ERROR_STATE` if startup fails

## Event Loop Modes

The function automatically selects the appropriate event loop mode:

### HTTP Only
```c
application_set_http_server(app, server);
// No Kafka client set
application_run(app);  // Uses http_server_run() - blocking epoll loop
```

### Kafka Only
```c
application_set_kafka_client(app, kafka);
// No HTTP server set
application_run(app);  // Simple sleep loop, Kafka runs in threads
```

### HTTP + Kafka (Combined)
```c
application_set_http_server(app, server);
application_set_kafka_client(app, kafka);
application_run(app);  // HTTP in main thread, Kafka in background threads
```

## Complete Example

See `examples/unified_app.c` for a complete working example that:
- Serves HTTP requests with multiple routes
- Consumes Kafka messages from multiple topics
- Produces Kafka messages via HTTP API
- Shares state between HTTP and Kafka handlers
- Handles graceful shutdown

### Running the Example

```bash
# Build
make debug

# Run with default settings (localhost:9092, port 8080)
./build/unified_app

# Run with custom settings
./build/unified_app kafka-broker:9092 3000

# Or use make target
make run-unified
```

### Testing the Example

```bash
# Check status
curl http://localhost:8080/api/status

# Check Kafka statistics
curl http://localhost:8080/api/kafka/stats

# Send message to Kafka via HTTP
curl -X POST http://localhost:8080/api/kafka/send \
  -H "Content-Type: application/json" \
  -d '{"user": "alice", "action": "login"}'
```

## Graceful Shutdown

The event loop handles signals automatically:

```c
application_run(app);  // Press Ctrl+C to stop

// Framework will:
// 1. Catch SIGINT/SIGTERM
// 2. Stop Kafka client gracefully
// 3. Stop HTTP server (close connections)
// 4. Return from application_run()
```

No need to implement signal handlers manually!

## Advanced: Shared Context

Share data between HTTP and Kafka handlers using application context:

```c
typedef struct {
    int request_count;
    int message_count;
    char last_message[256];
} APP_CONTEXT;

APP_CONTEXT *ctx = calloc(1, sizeof(APP_CONTEXT));

/* Pass context to HTTP handlers */
http_server_add_route(server, HTTP_METHOD_GET, "/stats", 
                     handle_stats, ctx);

/* Pass context to Kafka handlers */
kafka_consumer_register(kafka, "events", &config, 
                       handle_event, ctx);

/* Both handlers can now access shared state */
void handle_stats(HTTP_REQUEST *req, HTTP_RESPONSE *res, void *user_data) {
    APP_CONTEXT *ctx = (APP_CONTEXT*)user_data;
    printf("Requests: %d, Messages: %d\n", 
           ctx->request_count, ctx->message_count);
}

void handle_event(KAFKA_MESSAGE *msg, void *user_data) {
    APP_CONTEXT *ctx = (APP_CONTEXT*)user_data;
    ctx->message_count++;
}
```

## Benefits

### Before (Manual Management)
```c
signal(SIGINT, signal_handler);
http_server_start(server);
kafka_client_start(kafka);

// Complex manual loop
while (running) {
    sleep(1);
}

kafka_client_stop(kafka);
http_server_stop(server);
```

### After (Unified Loop)
```c
application_set_http_server(app, server);
application_set_kafka_client(app, kafka);
application_run(app);  // One line!
```

## Comparison with Other Frameworks

| Framework | HTTP | Kafka | Unified Loop |
|-----------|------|-------|--------------|
| Equinox | ✅ | ✅ | ✅ One function |
| Spring Boot | ✅ | ✅ | ❌ Separate configs |
| Node.js | ✅ | ✅ | ❌ Manual integration |
| Go | ✅ | ✅ | ❌ Goroutine management |

Equinox makes it simple!

## Thread Safety

- HTTP handlers run in thread pool (one per connection)
- Kafka consumers run in dedicated threads (one per registration)
- Use proper synchronization if sharing state between threads
- Consider using atomics or mutexes for counters

## See Also

- [HTTP Server Documentation](../README.md)
- [Kafka Integration Guide](../KAFKA_INTEGRATION.md)
- [Complete Example](../examples/unified_app.c)
