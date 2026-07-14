# Application Run - Architecture Overview

## What Changed

### Before: Manual Lifecycle Management
```
┌─────────────────────────────────────────┐
│  User Application Code                  │
├─────────────────────────────────────────┤
│  • Create HTTP server                   │
│  • Create Kafka client                  │
│  • Set up signal handlers               │
│  • Start HTTP server                    │
│  • Start Kafka client                   │
│  • while (running) { sleep(1); }        │
│  • Stop Kafka client                    │
│  • Stop HTTP server                     │
│  • Cleanup                              │
│                                         │
│  ~50-100 lines of boilerplate           │
└─────────────────────────────────────────┘
```

### After: Unified Event Loop
```
┌─────────────────────────────────────────┐
│  User Application Code                  │
├─────────────────────────────────────────┤
│  • Create HTTP server (configure)       │
│  • Create Kafka client (configure)      │
│  • application_set_http_server()        │
│  • application_set_kafka_client()       │
│  • application_run()   ← ONE FUNCTION!  │
│  • Cleanup                              │
│                                         │
│  ~10-20 lines total                     │
└─────────────────────────────────────────┘
```

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                     APPLICATION                          │
├──────────────────────────────────────────────────────────┤
│  • http_server:    HTTP_SERVER*                          │
│  • kafka_client:   KAFKA_CLIENT*                         │
│  • running:        int                                   │
└────────────┬─────────────────────────────────────────────┘
             │
             │  application_run()
             ▼
┌──────────────────────────────────────────────────────────┐
│              UNIFIED EVENT LOOP                          │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  1. Setup signal handlers (SIGINT, SIGTERM)              │
│                                                          │
│  2. Start components:                                    │
│     ├─ http_server_start()    if configured             │
│     └─ kafka_client_start()   if configured             │
│                                                          │
│  3. Run event loop:                                      │
│     ├─ HTTP only:    http_server_run() [blocking]       │
│     ├─ Kafka only:   sleep loop [Kafka in threads]      │
│     └─ Both:         HTTP main + Kafka threads          │
│                                                          │
│  4. On signal (Ctrl+C):                                  │
│     ├─ Set running = 0                                   │
│     ├─ kafka_client_stop()                               │
│     └─ http_server_stop()                                │
│                                                          │
│  5. Return FRAMEWORK_SUCCESS                             │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

## Threading Model

```
┌────────────────────────────────────────────────────┐
│  Main Thread                                       │
│  ┌──────────────────────────────────────────────┐ │
│  │ application_run()                            │ │
│  │   ├─ Start HTTP server                       │ │
│  │   ├─ Start Kafka client                      │ │
│  │   └─ http_server_run()  [BLOCKS HERE]        │ │
│  └──────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────┘
                     │
        ┌────────────┼────────────┐
        │            │            │
        ▼            ▼            ▼
┌────────────┐ ┌──────────┐ ┌──────────┐
│HTTP Worker │ │HTTP Wrkr │ │HTTP Wrkr │
│  Thread    │ │ Thread   │ │ Thread   │
│            │ │          │ │          │
│ handle_req │ │handle_req│ │handle_req│
└────────────┘ └──────────┘ └──────────┘

                     │
        ┌────────────┼────────────┐
        │            │            │
        ▼            ▼            ▼
┌────────────┐ ┌──────────┐ ┌──────────┐
│Kafka Cons. │ │Kafka Cons│ │Kafka Prod│
│  Thread    │ │  Thread  │ │  Thread  │
│            │ │          │ │          │
│topic:users │ │topic:ord │ │(async)   │
└────────────┘ └──────────┘ └──────────┘
```

## Signal Flow

```
     User presses Ctrl+C
            │
            ▼
    ┌───────────────┐
    │ SIGINT signal │
    └───────┬───────┘
            │
            ▼
┌──────────────────────────┐
│ application_signal_handler│
│  • g_app->running = 0     │
│  • http_server_stop()     │
└──────────┬───────────────┘
           │
           ▼
┌────────────────────────────┐
│  application_run() wakes   │
│  • kafka_client_stop()     │
│  • Return FRAMEWORK_SUCCESS│
└──────────┬─────────────────┘
           │
           ▼
┌────────────────────────────┐
│  User cleanup code         │
│  • kafka_client_destroy()  │
│  • http_server_destroy()   │
│  • application_destroy()   │
└────────────────────────────┘
```

## API Flow

```c
/* 1. Create and configure */
APPLICATION *app = application_create("MyApp", 1);
HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
KAFKA_CLIENT *kafka = kafka_client_create();

http_server_add_route(server, ...);
kafka_consumer_register(kafka, ...);
kafka_producer_init(kafka, ...);

/* 2. Register with application */
application_set_http_server(app, server);
application_set_kafka_client(app, kafka);

/* 3. Run (blocks until Ctrl+C) */
int result = application_run(app);

/* 4. Cleanup */
kafka_client_destroy(kafka);
http_server_destroy(server);
application_destroy(app);
```

## Use Cases

### 1. Microservice with REST API + Event Processing
```
HTTP Endpoints          Kafka Topics
──────────────         ──────────────
POST /orders    ─┐    ┌─ order-created
GET  /orders/:id │    │  order-updated
PUT  /orders/:id ┼────┤  order-completed
                 │    └─ payment-events
                 └─ Producer
```

### 2. Admin Dashboard + Monitoring
```
HTTP Dashboard          Kafka Metrics
──────────────         ──────────────
GET  /dashboard  ─┐   ┌─ app.metrics
GET  /health      ┼───┤  app.logs
GET  /metrics     │   └─ app.alerts
POST /config      │
                  └─ Producer (logs)
```

### 3. API Gateway + Message Queue
```
HTTP API                Kafka Topics
──────────────         ──────────────
POST /api/*      ─┐   ┌─ requests
                  ├───┤  responses
GET  /api/*      ─┘   └─ audit-log
```

## Benefits Summary

| Aspect | Before | After |
|--------|--------|-------|
| **Lines of code** | ~50-100 | ~10-20 |
| **Signal handling** | Manual | Automatic |
| **Error handling** | Per component | Unified |
| **Lifecycle mgmt** | Manual | Automatic |
| **Event loop** | Custom | Built-in |
| **Thread safety** | User responsibility | Framework handled |
| **Graceful shutdown** | Complex | One line |
| **Code clarity** | Scattered | Organized |

## Example Applications

All built examples available in `build/`:

1. **unified_app** - HTTP + Kafka together (recommended)
2. **http_server_app** - HTTP only
3. **kafka_demo** - Kafka only
4. **kafka_ssl_demo** - Kafka with SSL
5. **kafka_auth_demo** - Kafka with authentication
6. **kafka_multi_topic_demo** - Multiple topic handlers

Run with: `make run-unified` or `./build/unified_app`
