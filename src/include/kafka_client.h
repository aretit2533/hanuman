#ifndef KAFKA_CLIENT_H
#define KAFKA_CLIENT_H

#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
typedef struct _application_ APPLICATION;
typedef struct _kafka_client_ KAFKA_CLIENT;
typedef struct _kafka_consumer_ KAFKA_CONSUMER;
typedef struct _kafka_producer_ KAFKA_PRODUCER;

/* Kafka message structure */
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

/* Kafka consumer handler function signature */
typedef void (*kafka_consumer_handler_fn)(KAFKA_MESSAGE *message, void *user_data);

/* Kafka Authentication Types */
typedef enum {
    KAFKA_AUTH_NONE = 0,           /* No authentication */
    KAFKA_AUTH_SASL_PLAIN,         /* SASL/PLAIN (username/password) */
    KAFKA_AUTH_SASL_SCRAM_SHA256,  /* SASL/SCRAM-SHA-256 */
    KAFKA_AUTH_SASL_SCRAM_SHA512,  /* SASL/SCRAM-SHA-512 */
    KAFKA_AUTH_SASL_GSSAPI,        /* SASL/GSSAPI (Kerberos) */
    KAFKA_AUTH_SASL_OAUTHBEARER    /* SASL/OAUTHBEARER */
} KAFKA_AUTH_TYPE;

/* Kafka Consumer Configuration */
typedef struct _kafka_consumer_config_ {
    char brokers[512];              /* Kafka broker list (e.g., "localhost:9092") */
    char group_id[128];             /* Consumer group ID */
    char auto_offset_reset[32];     /* "earliest", "latest", "none" */
    int enable_auto_commit;         /* 1 for true, 0 for false */
    int auto_commit_interval_ms;    /* Auto-commit interval in milliseconds */
    int session_timeout_ms;         /* Session timeout */
    int max_poll_interval_ms;       /* Max poll interval */
    
    /* SSL/TLS Configuration */
    int enable_ssl;                 /* 1 to enable SSL/TLS, 0 to disable */
    char ssl_ca_location[512];      /* Path to CA certificate file */
    char ssl_cert_location[512];    /* Path to client certificate file */
    char ssl_key_location[512];     /* Path to client private key file */
    char ssl_key_password[128];     /* Password for private key (optional) */
    
    /* Authentication Configuration */
    KAFKA_AUTH_TYPE auth_type;      /* Authentication type */
    char sasl_username[256];        /* SASL username */
    char sasl_password[256];        /* SASL password */
    char sasl_mechanism[32];        /* SASL mechanism (auto-set based on auth_type) */
    char kerberos_service_name[64]; /* Kerberos service name (for GSSAPI) */
    char kerberos_principal[256];   /* Kerberos principal (for GSSAPI) */
    char kerberos_keytab[512];      /* Kerberos keytab path (for GSSAPI) */
} KAFKA_CONSUMER_CONFIG;

/* Kafka Producer Configuration */
typedef struct _kafka_producer_config_ {
    char brokers[512];              /* Kafka broker list */
    int compression_type;           /* 0=none, 1=gzip, 2=snappy, 3=lz4, 4=zstd */
    int batch_size;                 /* Batch size in bytes */
    int linger_ms;                  /* Linger time in milliseconds */
    int acks;                       /* -1=all, 0=none, 1=leader */
    int retries;                    /* Number of retries */
    
    /* SSL/TLS Configuration */
    int enable_ssl;                 /* 1 to enable SSL/TLS, 0 to disable */
    char ssl_ca_location[512];      /* Path to CA certificate file */
    char ssl_cert_location[512];    /* Path to client certificate file */
    char ssl_key_location[512];     /* Path to client private key file */
    char ssl_key_password[128];     /* Password for private key (optional) */
    
    /* Authentication Configuration */
    KAFKA_AUTH_TYPE auth_type;      /* Authentication type */
    char sasl_username[256];        /* SASL username */
    char sasl_password[256];        /* SASL password */
    char sasl_mechanism[32];        /* SASL mechanism (auto-set based on auth_type) */
    char kerberos_service_name[64]; /* Kerberos service name (for GSSAPI) */
    char kerberos_principal[256];   /* Kerberos principal (for GSSAPI) */
    char kerberos_keytab[512];      /* Kerberos keytab path (for GSSAPI) */
} KAFKA_PRODUCER_CONFIG;

/* Kafka Client - Main container for consumers and producers */
struct _kafka_client_ {
    APPLICATION *app;
    void *context;
    
    /* Consumers */
    KAFKA_CONSUMER **consumers;
    size_t consumer_count;
    size_t consumer_capacity;
    
    /* Producer */
    KAFKA_PRODUCER *producer;
    
    int running;
};

/* ============================================================================
 * Kafka Client Functions
 * ========================================================================== */

/**
 * Create a new Kafka client
 * @return Pointer to created Kafka client, or NULL on failure
 */
KAFKA_CLIENT* kafka_client_create(void);

/**
 * Destroy a Kafka client and free resources
 * @param client The Kafka client to destroy
 */
void kafka_client_destroy(KAFKA_CLIENT *client);

/**
 * Start the Kafka client (begins consuming messages)
 * @param client The Kafka client
 * @return 0 on success, error code on failure
 */
int kafka_client_start(KAFKA_CLIENT *client);

/**
 * Stop the Kafka client
 * @param client The Kafka client
 * @return 0 on success, error code on failure
 */
int kafka_client_stop(KAFKA_CLIENT *client);

/* ============================================================================
 * Kafka Consumer Functions
 * ========================================================================== */

/**
 * Register a consumer for a specific topic (similar to HTTP route registration)
 * @param client The Kafka client
 * @param topic The topic to consume from
 * @param config Consumer configuration
 * @param handler Callback function to handle consumed messages
 * @param user_data User-defined data passed to the handler
 * @return 0 on success, error code on failure
 * 
 * Example:
 *   kafka_consumer_register(client, "user-events", &config, handle_user_event, NULL);
 */
int kafka_consumer_register(KAFKA_CLIENT *client, const char *topic,
                           KAFKA_CONSUMER_CONFIG *config,
                           kafka_consumer_handler_fn handler,
                           void *user_data);

/**
 * Register a consumer for multiple topics (similar to HTTP route registration)
 * All topics will use the same handler and configuration
 * @param client The Kafka client
 * @param topics Array of topic names to consume from
 * @param topic_count Number of topics in the array
 * @param config Consumer configuration
 * @param handler Callback function to handle consumed messages from all topics
 * @param user_data User-defined data passed to the handler
 * @return 0 on success, error code on failure
 * 
 * Example:
 *   const char *topics[] = {"user-events", "order-events", "payment-events"};
 *   kafka_consumer_register_multi(client, topics, 3, &config, handle_events, NULL);
 */
int kafka_consumer_register_multi(KAFKA_CLIENT *client, 
                                 const char **topics,
                                 size_t topic_count,
                                 KAFKA_CONSUMER_CONFIG *config,
                                 kafka_consumer_handler_fn handler,
                                 void *user_data);

/**
 * Create default consumer configuration
 * @param config Consumer configuration structure to initialize
 * @param brokers Kafka broker list (e.g., "localhost:9092")
 * @param group_id Consumer group ID
 */
void kafka_consumer_config_default(KAFKA_CONSUMER_CONFIG *config,
                                  const char *brokers,
                                  const char *group_id);

/* ============================================================================
 * Kafka Producer Functions
 * ========================================================================== */

/**
 * Initialize a producer for the Kafka client
 * @param client The Kafka client
 * @param config Producer configuration
 * @return 0 on success, error code on failure
 */
int kafka_producer_init(KAFKA_CLIENT *client, KAFKA_PRODUCER_CONFIG *config);

/**
 * Produce a message to a Kafka topic
 * @param client The Kafka client
 * @param topic The topic to produce to
 * @param key Message key (can be NULL)
 * @param key_len Length of the key
 * @param payload Message payload
 * @param payload_len Length of the payload
 * @return 0 on success, error code on failure
 * 
 * Example:
 *   kafka_produce(client, "user-events", "user123", 7, "{\"action\":\"login\"}", 20);
 */
int kafka_produce(KAFKA_CLIENT *client, const char *topic,
                 const void *key, size_t key_len,
                 const void *payload, size_t payload_len);

/**
 * Produce a message with a string payload
 * @param client The Kafka client
 * @param topic The topic to produce to
 * @param key Message key (can be NULL)
 * @param payload Message payload (null-terminated string)
 * @return 0 on success, error code on failure
 */
int kafka_produce_string(KAFKA_CLIENT *client, const char *topic,
                        const char *key, const char *payload);

/**
 * Create default producer configuration
 * @param config Producer configuration structure to initialize
 * @param brokers Kafka broker list (e.g., "localhost:9092")
 */
void kafka_producer_config_default(KAFKA_PRODUCER_CONFIG *config,
                                  const char *brokers);

/* ============================================================================
 * Kafka Message Functions
 * ========================================================================== */

/**
 * Get message payload as string (assumes null-terminated or adds null terminator)
 * @param message The Kafka message
 * @return Message payload as string, or NULL on failure
 */
const char* kafka_message_get_payload_string(KAFKA_MESSAGE *message);

/**
 * Get message key as string
 * @param message The Kafka message
 * @return Message key as string, or NULL if no key
 */
const char* kafka_message_get_key_string(KAFKA_MESSAGE *message);

/* ============================================================================
 * Application Integration
 * ========================================================================== */

#endif /* KAFKA_CLIENT_H */
