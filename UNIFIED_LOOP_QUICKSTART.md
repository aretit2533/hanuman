# Unified Event Loop - Quick Start

The Equinox framework now provides **`application_run()`** - a single function that manages both HTTP and Kafka in one unified event loop!

## Why This Matters

Previously, you had to manually:
- Start HTTP server
- Start Kafka client  
- Set up signal handlers
- Manage the event loop
- Stop everything cleanly

Now it's just **one function call**:

```c
application_run(app);  // That's it!
```

## Quick Example

```c
#include "application.h"
#include "http_server.h"
#include "kafka_client.h"

int main(void) {
    /* Create application */
    APPLICATION *app = application_create("My App", 1);
    
    /* Setup HTTP */
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    http_server_add_route(server, HTTP_METHOD_GET, "/", handle_home, NULL);
    application_set_http_server(app, server);
    
    /* Setup Kafka */
    KAFKA_CLIENT *kafka = kafka_client_create();
    KAFKA_CONSUMER_CONFIG config;
    kafka_consumer_config_default(&config, "localhost:9092", "my-group");
    kafka_consumer_register(kafka, "events", &config, handle_event, NULL);
    application_set_kafka_client(app, kafka);
    
    /* Run everything - handles signals, event loop, graceful shutdown */
    application_run(app);
    
    /* Cleanup */
    kafka_client_destroy(kafka);
    http_server_destroy(server);
    application_destroy(app);
    
    return 0;
}
```

## Try It Now

```bash
# Build the unified example
make debug

# Run it
./build/unified_app

# In another terminal, test it
curl http://localhost:8080/api/status
curl http://localhost:8080/api/kafka/stats
curl -X POST http://localhost:8080/api/kafka/send -d '{"test": "data"}'

# Stop with Ctrl+C - handles graceful shutdown automatically
```

## Key Features

✅ **One function manages everything**
- HTTP server in main thread
- Kafka consumers in background threads  
- Automatic signal handling (SIGINT, SIGTERM)
- Graceful shutdown on Ctrl+C

✅ **Flexible configuration**
- HTTP only
- Kafka only  
- Both HTTP and Kafka together

✅ **Clean API**
```c
application_set_http_server(app, server);   // Optional
application_set_kafka_client(app, kafka);   // Optional
application_run(app);                        // Required
```

## Complete Example

See `examples/unified_app.c` for a full working example showing:
- HTTP routes serving JSON
- Kafka consumers processing messages
- HTTP endpoint that produces to Kafka
- Shared state between HTTP and Kafka handlers
- Complete lifecycle management

## Documentation

- [Unified Event Loop Guide](docs/UNIFIED_EVENT_LOOP.md) - Complete documentation
- [Unified App Example](examples/unified_app.c) - Full source code
- [API Reference](src/include/application.h) - Function declarations

## Benefits

| Old Way | New Way |
|---------|---------|
| Signal handlers | ✅ Automatic |
| Manual start/stop | ✅ One function |
| Complex loop | ✅ Built-in |
| Error handling | ✅ Handled |
| ~50 lines | ✅ 3 lines |

Start building unified HTTP+Kafka applications with minimal code!
