# Kafka Integration

This document describes the Kafka integration in the Hanuman framework using librdkafka.

## Overview

The Hanuman framework now supports Apache Kafka for distributed messaging with consumer and producer capabilities. Consumer registration follows the same pattern as HTTP route registration, making it familiar and easy to use.

## Features

- **Consumer Registration**: Register topic handlers similar to HTTP routes
- **Producer Support**: Send messages to Kafka topics
- **Multi-Topic Support**: Subscribe to multiple topics with different handlers
- **Thread-Safe**: Each consumer runs in its own thread
- **Configuration**: Flexible consumer and producer configuration
- **Application Integration**: Seamlessly integrates with the application lifecycle

## Dependencies

- **librdkafka**: The C library for Apache Kafka
  ```bash
  # Install on Ubuntu/Debian
  sudo apt-get install librdkafka-dev
  
  # Install on macOS
  brew install librdkafka
  
  # Install on RHEL/CentOS
  sudo yum install librdkafka-devel
  ```

## API Reference

### Kafka Client

#### Create and Destroy
```c
KAFKA_CLIENT* kafka_client_create(void);
void kafka_client_destroy(KAFKA_CLIENT *client);
```

#### Lifecycle
```c
int kafka_client_start(KAFKA_CLIENT *client);  // Start consuming
int kafka_client_stop(KAFKA_CLIENT *client);   // Stop consuming
```

### Consumer Registration

Register consumers for topics (similar to HTTP route registration):

```c
int kafka_consumer_register(KAFKA_CLIENT *client, 
                           const char *topic,
                           KAFKA_CONSUMER_CONFIG *config,
                           kafka_consumer_handler_fn handler,
                           void *user_data);
```

#### Consumer Handler Signature
```c
typedef void (*kafka_consumer_handler_fn)(KAFKA_MESSAGE *message, void *user_data);
```

#### Consumer Configuration
```c
typedef struct _kafka_consumer_config_ {
    char brokers[512];              // Kafka broker list (e.g., "localhost:9092")
    char group_id[128];             // Consumer group ID
    char auto_offset_reset[32];     // "earliest", "latest", "none"
    int enable_auto_commit;         // 1 for true, 0 for false
    int auto_commit_interval_ms;    // Auto-commit interval
    int session_timeout_ms;         // Session timeout
    int max_poll_interval_ms;       // Max poll interval
    
    // SSL/TLS Configuration
    int enable_ssl;                 // 1 to enable SSL/TLS, 0 to disable
    char ssl_ca_location[512];      // Path to CA certificate file
    char ssl_cert_location[512];    // Path to client certificate file
    char ssl_key_location[512];     // Path to client private key file
    char ssl_key_password[128];     // Password for private key (optional)
} KAFKA_CONSUMER_CONFIG;

// Create default configuration
void kafka_consumer_config_default(KAFKA_CONSUMER_CONFIG *config,
                                  const char *brokers,
                                  const char *group_id);
```

### Producer

#### Initialize Producer
```c
int kafka_producer_init(KAFKA_CLIENT *client, 
                       KAFKA_PRODUCER_CONFIG *config);
```

#### Produce Messages
```c
// Produce with key and payload
int kafka_produce(KAFKA_CLIENT *client, 
                 const char *topic,
                 const void *key, size_t key_len,
                 const void *payload, size_t payload_len);

// Produce string message
int kafka_produce_string(KAFKA_CLIENT *client, 
                        const char *topic,
                        const char *key, 
                        const char *payload);
```

#### Producer Configuration
```c
typedef struct _kafka_producer_config_ {
    char brokers[512];              // Kafka broker list
    int compression_type;           // 0=none, 1=gzip, 2=snappy, 3=lz4, 4=zstd
    int batch_size;                 // Batch size in bytes
    int linger_ms;                  // Linger time in milliseconds
    int acks;                       // -1=all, 0=none, 1=leader
    int retries;                    // Number of retries
    
    // SSL/TLS Configuration
    int enable_ssl;                 // 1 to enable SSL/TLS, 0 to disable
    char ssl_ca_location[512];      // Path to CA certificate file
    char ssl_cert_location[512];    // Path to client certificate file
    char ssl_key_location[512];     // Path to client private key file
    char ssl_key_password[128];     // Password for private key (optional)
} KAFKA_PRODUCER_CONFIG;

// Create default configuration
void kafka_producer_config_default(KAFKA_PRODUCER_CONFIG *config,
                                  const char *brokers);
```

### Message Structure

```c
typedef struct _kafka_message_ {
    char *topic;
    int partition;
    int64_t offset;
    char *key;
    size_t key_len;
    char *payload;
    size_t payload_len;
    int64_t timestamp;
    void *user_data;
} KAFKA_MESSAGE;

// Helper functions
const char* kafka_message_get_payload_string(KAFKA_MESSAGE *message);
const char* kafka_message_get_key_string(KAFKA_MESSAGE *message);
```

## Usage Example

### Basic Consumer and Producer

```c
#include "application.h"
#include "kafka_client.h"
#include "framework.h"

// Consumer handler - like an HTTP route handler
void handle_user_event(KAFKA_MESSAGE *message, void *user_data)
{
    const char *key = kafka_message_get_key_string(message);
    const char *payload = kafka_message_get_payload_string(message);
    
    printf("Received: key=%s, payload=%s\n", key, payload);
    
    // Process the message...
}

int main(void)
{
    // Create application
    APPLICATION *app = application_create("My App", 1);
    
    // Create Kafka client
    KAFKA_CLIENT *kafka = kafka_client_create();
    application_set_kafka_client(app, kafka);
    
    // Configure consumer
    KAFKA_CONSUMER_CONFIG consumer_config;
    kafka_consumer_config_default(&consumer_config, "localhost:9092", "my-group");
    consumer_config.auto_offset_reset = "earliest";
    
    // Register consumers (like registering HTTP routes!)
    kafka_consumer_register(kafka, "user-events", &consumer_config,
                          handle_user_event, NULL);
    
    kafka_consumer_register(kafka, "order-events", &consumer_config,
                          handle_order_event, NULL);
    
    // Initialize producer
    KAFKA_PRODUCER_CONFIG producer_config;
    kafka_producer_config_default(&producer_config, "localhost:9092");
    producer_config.acks = -1;  // Wait for all replicas
    
    kafka_producer_init(kafka, &producer_config);
    
    // Start consuming
    kafka_client_start(kafka);
    
    // Produce messages
    kafka_produce_string(kafka, "user-events", "user123", 
                        "{\"action\":\"login\"}");
    
    // Keep running...
    while (running) {
        sleep(1);
    }
    
    // Cleanup
    kafka_client_stop(kafka);
    kafka_client_destroy(kafka);
    application_destroy(app);
    
    return 0;
}
```

### Multiple Topic Handlers

```c
// Register multiple handlers for different topics
kafka_consumer_register(kafka, "user-events", &config, 
                       handle_user_event, NULL);
                       
kafka_consumer_register(kafka, "order-events", &config, 
                       handle_order_event, NULL);
                       
kafka_consumer_register(kafka, "payment-events", &config, 
                       handle_payment_event, NULL);
```

### Register Single Handler for Multiple Topics

Use `kafka_consumer_register_multi` to consume from multiple topics with one handler:

```c
// Single handler for multiple related topics
const char *business_topics[] = {
    "user-events",
    "order-events", 
    "payment-events",
    "inventory-events"
};

kafka_consumer_register_multi(kafka, business_topics, 4, &config,
                             handle_business_event, NULL);

// The handler receives messages from all topics
void handle_business_event(KAFKA_MESSAGE *message, void *user_data)
{
    printf("Event from topic: %s\n", message->topic);
    printf("Payload: %s\n", kafka_message_get_payload_string(message));
}
```

### Custom User Data

```c
typedef struct {
    int counter;
    char name[64];
} MyContext;

void my_handler(KAFKA_MESSAGE *message, void *user_data)
{
    MyContext *ctx = (MyContext*)user_data;
    ctx->counter++;
    printf("%s: processed %d messages\n", ctx->name, ctx->counter);
}

// Register with context
MyContext ctx = { .counter = 0, .name = "my-handler" };
kafka_consumer_register(kafka, "my-topic", &config, 
                       my_handler, &ctx);
```

## Pattern Comparison

### HTTP Route Registration
```c
http_server_add_route(server, HTTP_METHOD_GET, "/api/users/:id", 
                     handle_get_user, NULL);
```

### Kafka Consumer Registration
```c
kafka_consumer_register(kafka, "user-events", &config,
                       handle_user_event, NULL);
```

Both follow the same pattern:
1. Target (path/topic)
2. Configuration
3. Handler function
4. User data

## Building

The Kafka support requires librdkafka. Build with:

```bash
# Build with Kafka support
make debug

# Run Kafka demo
make run-kafka

# Or build manually
gcc -o kafka_demo examples/kafka_demo.c \
    -Llib -lequinox -lrdkafka -lpthread
```

## Running the Demo

### Start Kafka

```bash
# Start Zookeeper
bin/zookeeper-server-start.sh config/zookeeper.properties

# Start Kafka
bin/kafka-server-start.sh config/server.properties

# Create topics
bin/kafka-topics.sh --create --topic user-events \
    --bootstrap-server localhost:9092
    
bin/kafka-topics.sh --create --topic order-events \
    --bootstrap-server localhost:9092
```

### Run Demo Application

```bash
make run-kafka
```

The demo will:
- Register consumers for multiple topics
- Start consuming messages
- Produce test messages every 10 seconds
- Display received messages

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                 Application                          │
│  ┌──────────────────────────────────────────────┐  │
│  │            Kafka Client                       │  │
│  │  ┌────────────┐         ┌─────────────────┐  │  │
│  │  │ Consumer 1 │         │    Producer     │  │  │
│  │  │  Thread    │         │                 │  │  │
│  │  │            │         │  - Produce      │  │  │
│  │  │ Topic: A   │         │  - Async Send   │  │  │
│  │  │ Handler    │         │  - Batching     │  │  │
│  │  └────────────┘         └─────────────────┘  │  │
│  │                                               │  │
│  │  ┌────────────┐                              │  │
│  │  │ Consumer 2 │                              │  │
│  │  │  Thread    │                              │  │
│  │  │            │                              │  │
│  │  │ Topic: B   │                              │  │
│  │  │ Handler    │                              │  │
│  │  └────────────┘                              │  │
│  └──────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
                    │                  ▲
                    │                  │
                    ▼                  │
         ┌──────────────────────────────────┐
         │         Apache Kafka              │
         │  ┌────────────┐  ┌────────────┐  │
         │  │  Topic A   │  │  Topic B   │  │
         │  └────────────┘  └────────────┘  │
         └──────────────────────────────────┘
```

## Best Practices

1. **Consumer Groups**: Use appropriate group IDs for parallel processing
2. **Error Handling**: Handle message processing errors gracefully
3. **Offset Management**: Choose appropriate auto-commit settings
4. **Threading**: Each consumer runs in its own thread
5. **Cleanup**: Always stop and destroy clients on shutdown
6. **Monitoring**: Use framework logging to track message flow
7. **SSL/TLS**: Always use SSL/TLS in production environments
8. **Certificate Management**: Keep certificates up to date and secure
9. **Authentication**: Use SASL authentication for production Kafka clusters
10. **Password Security**: Never hardcode credentials, use environment variables

## Authentication Support

The framework supports multiple SASL authentication mechanisms for secure Kafka access.

### Authentication Types

- **KAFKA_AUTH_NONE** - No authentication
- **KAFKA_AUTH_SASL_PLAIN** - Username/password (simple)
- **KAFKA_AUTH_SASL_SCRAM_SHA256** - SCRAM-SHA-256 (recommended)
- **KAFKA_AUTH_SASL_SCRAM_SHA512** - SCRAM-SHA-512 (most secure)
- **KAFKA_AUTH_SASL_GSSAPI** - Kerberos
- **KAFKA_AUTH_SASL_OAUTHBEARER** - OAuth Bearer

### SASL/PLAIN Example

```c
KAFKA_CONSUMER_CONFIG config;
kafka_consumer_config_default(&config, "broker:9092", "my-group");

config.auth_type = KAFKA_AUTH_SASL_PLAIN;
strcpy(config.sasl_username, "myuser");
strcpy(config.sasl_password, "mypassword");

kafka_consumer_register(kafka, "topic", &config, handler, NULL);
```

### SASL/SCRAM-SHA-256 (Recommended)

```c
config.auth_type = KAFKA_AUTH_SASL_SCRAM_SHA256;
strcpy(config.sasl_username, "admin");
strcpy(config.sasl_password, "secure-password");
```

### SSL + Authentication (Production)

```c
KAFKA_CONSUMER_CONFIG config;
kafka_consumer_config_default(&config, "broker:9093", "group");

// SSL
config.enable_ssl = 1;
strcpy(config.ssl_ca_location, "/path/to/ca-cert.pem");

// Authentication
config.auth_type = KAFKA_AUTH_SASL_SCRAM_SHA256;
strcpy(config.sasl_username, "user");
strcpy(config.sasl_password, getenv("KAFKA_PASSWORD"));

kafka_consumer_register(kafka, "secure-topic", &config, handler, NULL);
```

## SSL/TLS Configuration

### Enabling SSL/TLS

Both consumer and producer configurations support SSL/TLS encryption:

```c
KAFKA_CONSUMER_CONFIG config;
kafka_consumer_config_default(&config, "broker:9093", "my-group");

// Enable SSL
config.enable_ssl = 1;
strcpy(config.ssl_ca_location, "/path/to/ca-cert.pem");
strcpy(config.ssl_cert_location, "/path/to/client-cert.pem");
strcpy(config.ssl_key_location, "/path/to/client-key.pem");
strcpy(config.ssl_key_password, "optional-key-password");  // If key is encrypted

kafka_consumer_register(kafka, "secure-topic", &config, handler, NULL);
```

### SSL Producer Example

```c
KAFKA_PRODUCER_CONFIG producer_config;
kafka_producer_config_default(&producer_config, "broker:9093");

// Enable SSL
producer_config.enable_ssl = 1;
strcpy(producer_config.ssl_ca_location, "/path/to/ca-cert.pem");
strcpy(producer_config.ssl_cert_location, "/path/to/client-cert.pem");
strcpy(producer_config.ssl_key_location, "/path/to/client-key.pem");

kafka_producer_init(kafka, &producer_config);
```

### Certificate Files

Three types of certificate files are supported:

1. **CA Certificate** (`ssl_ca_location`): Certificate Authority certificate to verify broker's identity
2. **Client Certificate** (`ssl_cert_location`): Client's public certificate for mutual TLS authentication
3. **Client Key** (`ssl_key_location`): Client's private key corresponding to the client certificate

### SSL Port Configuration

When using SSL/TLS, ensure your brokers string uses the SSL port (typically 9093):

```c
// Non-SSL
config.brokers = "localhost:9092";

// SSL
config.brokers = "localhost:9093";
```

### Generating Test Certificates

For testing purposes, you can generate self-signed certificates:

```bash
# Generate CA key and certificate
openssl req -new -x509 -keyout ca-key.pem -out ca-cert.pem -days 365

# Generate client key
openssl genrsa -out client-key.pem 2048

# Generate client certificate signing request
openssl req -new -key client-key.pem -out client-cert.csr

# Sign client certificate with CA
openssl x509 -req -in client-cert.csr -CA ca-cert.pem \
  -CAkey ca-key.pem -CAcreateserial -out client-cert.pem -days 365
```

### Complete SSL Example

```c
#include "application.h"
#include "kafka_client.h"
#include "framework.h"

void handle_secure_message(KAFKA_MESSAGE *message, void *user_data)
{
    const char *payload = kafka_message_get_payload_string(message);
    printf("Secure message received: %s\n", payload);
}

int main(void)
{
    APPLICATION *app = application_create("Secure Kafka App", 1);
    KAFKA_CLIENT *kafka = kafka_client_create();
    application_set_kafka_client(app, kafka);
    
    // Configure SSL consumer
    KAFKA_CONSUMER_CONFIG consumer_config;
    kafka_consumer_config_default(&consumer_config, "broker:9093", "secure-group");
    consumer_config.enable_ssl = 1;
    strcpy(consumer_config.ssl_ca_location, "/etc/kafka/ssl/ca-cert.pem");
    strcpy(consumer_config.ssl_cert_location, "/etc/kafka/ssl/client-cert.pem");
    strcpy(consumer_config.ssl_key_location, "/etc/kafka/ssl/client-key.pem");
    
    kafka_consumer_register(kafka, "secure-topic", &consumer_config,
                          handle_secure_message, NULL);
    
    // Configure SSL producer
    KAFKA_PRODUCER_CONFIG producer_config;
    kafka_producer_config_default(&producer_config, "broker:9093");
    producer_config.enable_ssl = 1;
    strcpy(producer_config.ssl_ca_location, "/etc/kafka/ssl/ca-cert.pem");
    strcpy(producer_config.ssl_cert_location, "/etc/kafka/ssl/client-cert.pem");
    strcpy(producer_config.ssl_key_location, "/etc/kafka/ssl/client-key.pem");
    
    kafka_producer_init(kafka, &producer_config);
    
    // Start and use...
    kafka_client_start(kafka);
    kafka_produce_string(kafka, "secure-topic", NULL, "Encrypted message");
    
    // Cleanup
    sleep(10);
    kafka_client_stop(kafka);
    kafka_client_destroy(kafka);
    application_destroy(app);
    
    return 0;
}
```

## Best Practices

1. **Consumer Groups**: Use appropriate group IDs for parallel processing
2. **Error Handling**: Handle message processing errors gracefully
3. **Offset Management**: Choose appropriate auto-commit settings
4. **Threading**: Each consumer runs in its own thread
5. **Cleanup**: Always stop and destroy clients on shutdown
6. **Monitoring**: Use framework logging to track message flow

## Performance Tips

- Enable compression for large messages
- Tune batch size and linger time for throughput
- Use appropriate consumer group settings
- Monitor partition lag
- Consider manual offset management for critical applications

## See Also

- [HTTP Server Documentation](HTTP_SERVER.md)
- [Framework Documentation](README.md)
- [librdkafka Documentation](https://docs.confluent.io/platform/current/clients/librdkafka/html/index.html)
