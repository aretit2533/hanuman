#define _POSIX_C_SOURCE 200809L
#include "kafka_client.h"
#include "application.h"
#include "framework.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <librdkafka/rdkafka.h>

#define INITIAL_CONSUMER_CAPACITY 10
#define KAFKA_POLL_TIMEOUT_MS 1000

/* Internal consumer structure */
struct _kafka_consumer_ {
    char **topics;           /* Array of topic names */
    size_t topic_count;      /* Number of topics */
    rd_kafka_t *rk;
    rd_kafka_topic_partition_list_t *topic_list;
    kafka_consumer_handler_fn handler;
    void *user_data;
    pthread_t thread;
    int running;
};

/* Internal producer structure */
struct _kafka_producer_ {
    rd_kafka_t *rk;
    KAFKA_PRODUCER_CONFIG config;
};

/* ============================================================================
 * Kafka Client Functions
 * ========================================================================== */

KAFKA_CLIENT* kafka_client_create(void)
{
    KAFKA_CLIENT *client = (KAFKA_CLIENT*)calloc(1, sizeof(KAFKA_CLIENT));
    if (!client) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate Kafka client");
        return NULL;
    }
    
    client->consumer_capacity = INITIAL_CONSUMER_CAPACITY;
    client->consumers = (KAFKA_CONSUMER**)calloc(client->consumer_capacity, 
                                                  sizeof(KAFKA_CONSUMER*));
    if (!client->consumers) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate consumers array");
        free(client);
        return NULL;
    }
    
    client->consumer_count = 0;
    client->producer = NULL;
    client->running = 0;
    client->app = NULL;
    client->context = NULL;
    
    framework_log(LOG_LEVEL_INFO, "Kafka client created");
    return client;
}

void kafka_client_destroy(KAFKA_CLIENT *client)
{
    if (!client) return;
    
    if (client->running) {
        kafka_client_stop(client);
    }
    
    /* Destroy consumers */
    if (client->consumers) {
        for (size_t i = 0; i < client->consumer_count; i++) {
            KAFKA_CONSUMER *consumer = client->consumers[i];
            if (consumer) {
                if (consumer->topic_list) {
                    rd_kafka_topic_partition_list_destroy(consumer->topic_list);
                }
                if (consumer->rk) {
                    rd_kafka_consumer_close(consumer->rk);
                    rd_kafka_destroy(consumer->rk);
                }
                /* Free topic array */
                if (consumer->topics) {
                    for (size_t j = 0; j < consumer->topic_count; j++) {
                        free(consumer->topics[j]);
                    }
                    free(consumer->topics);
                }
                free(consumer);
            }
        }
        free(client->consumers);
    }
    
    /* Destroy producer */
    if (client->producer) {
        if (client->producer->rk) {
            rd_kafka_flush(client->producer->rk, 10000); /* Wait up to 10 seconds */
            rd_kafka_destroy(client->producer->rk);
        }
        free(client->producer);
    }
    
    free(client);
    framework_log(LOG_LEVEL_INFO, "Kafka client destroyed");
}

/* Consumer thread function */
static void* consumer_thread_fn(void *arg)
{
    KAFKA_CONSUMER *consumer = (KAFKA_CONSUMER*)arg;
    
    if (consumer->topic_count == 1) {
        framework_log(LOG_LEVEL_INFO, "Kafka consumer thread started for topic: %s", 
                     consumer->topics[0]);
    } else {
        framework_log(LOG_LEVEL_INFO, "Kafka consumer thread started for %zu topics", 
                     consumer->topic_count);
    }
    
    while (consumer->running) {
        rd_kafka_message_t *rkmsg = rd_kafka_consumer_poll(consumer->rk, 
                                                           KAFKA_POLL_TIMEOUT_MS);
        if (!rkmsg) {
            continue; /* Timeout, no message */
        }
        
        if (rkmsg->err) {
            if (rkmsg->err != RD_KAFKA_RESP_ERR__PARTITION_EOF) {
                framework_log(LOG_LEVEL_ERROR, "Kafka consumer error: %s",
                            rd_kafka_message_errstr(rkmsg));
            }
            rd_kafka_message_destroy(rkmsg);
            continue;
        }
        
        /* Create message structure for handler */
        KAFKA_MESSAGE message;
        message.topic = (char*)rd_kafka_topic_name(rkmsg->rkt);
        message.partition = rkmsg->partition;
        message.offset = rkmsg->offset;
        message.key = (char*)rkmsg->key;
        message.key_len = rkmsg->key_len;
        message.payload = (char*)rkmsg->payload;
        message.payload_len = rkmsg->len;
        message.timestamp = 0;  /* Timestamp not directly available in older librdkafka */
        message.user_data = consumer->user_data;
        
        /* Call user handler */
        if (consumer->handler) {
            consumer->handler(&message, consumer->user_data);
        }
        
        rd_kafka_message_destroy(rkmsg);
    }
    
    if (consumer->topic_count == 1) {
        framework_log(LOG_LEVEL_INFO, "Kafka consumer thread stopped for topic: %s",
                     consumer->topics[0]);
    } else {
        framework_log(LOG_LEVEL_INFO, "Kafka consumer thread stopped for %zu topics",
                     consumer->topic_count);
    }
    return NULL;
}

int kafka_client_start(KAFKA_CLIENT *client)
{
    if (!client) return FRAMEWORK_ERROR_NULL_PTR;
    if (client->running) return FRAMEWORK_ERROR_STATE;
    
    client->running = 1;
    
    /* Start all consumer threads */
    for (size_t i = 0; i < client->consumer_count; i++) {
        KAFKA_CONSUMER *consumer = client->consumers[i];
        consumer->running = 1;
        
        if (pthread_create(&consumer->thread, NULL, consumer_thread_fn, consumer) != 0) {
            if (consumer->topic_count == 1) {
                framework_log(LOG_LEVEL_ERROR, "Failed to create consumer thread for topic: %s",
                            consumer->topics[0]);
            } else {
                framework_log(LOG_LEVEL_ERROR, "Failed to create consumer thread for %zu topics",
                            consumer->topic_count);
            }
            consumer->running = 0;
            return FRAMEWORK_ERROR_INVALID;
        }
    }
    
    framework_log(LOG_LEVEL_INFO, "Kafka client started with %zu consumer(s)", 
                 client->consumer_count);
    return FRAMEWORK_SUCCESS;
}

int kafka_client_stop(KAFKA_CLIENT *client)
{
    if (!client) return FRAMEWORK_ERROR_NULL_PTR;
    if (!client->running) return FRAMEWORK_ERROR_STATE;
    
    client->running = 0;
    
    /* Stop all consumer threads */
    for (size_t i = 0; i < client->consumer_count; i++) {
        KAFKA_CONSUMER *consumer = client->consumers[i];
        consumer->running = 0;
        pthread_join(consumer->thread, NULL);
    }
    
    framework_log(LOG_LEVEL_INFO, "Kafka client stopped");
    return FRAMEWORK_SUCCESS;
}

/* ============================================================================
 * Kafka Consumer Functions
 * ========================================================================== */

/* Helper function to get SASL mechanism string */
static const char* get_sasl_mechanism(KAFKA_AUTH_TYPE auth_type)
{
    switch (auth_type) {
        case KAFKA_AUTH_SASL_PLAIN:
            return "PLAIN";
        case KAFKA_AUTH_SASL_SCRAM_SHA256:
            return "SCRAM-SHA-256";
        case KAFKA_AUTH_SASL_SCRAM_SHA512:
            return "SCRAM-SHA-512";
        case KAFKA_AUTH_SASL_GSSAPI:
            return "GSSAPI";
        case KAFKA_AUTH_SASL_OAUTHBEARER:
            return "OAUTHBEARER";
        default:
            return NULL;
    }
}

void kafka_consumer_config_default(KAFKA_CONSUMER_CONFIG *config,
                                  const char *brokers,
                                  const char *group_id)
{
    if (!config) return;
    
    memset(config, 0, sizeof(KAFKA_CONSUMER_CONFIG));
    
    if (brokers) {
        strncpy(config->brokers, brokers, sizeof(config->brokers) - 1);
    } else {
        strcpy(config->brokers, "localhost:9092");
    }
    
    if (group_id) {
        strncpy(config->group_id, group_id, sizeof(config->group_id) - 1);
    } else {
        strcpy(config->group_id, "default-group");
    }
    
    strcpy(config->auto_offset_reset, "latest");
    config->enable_auto_commit = 1;
    config->auto_commit_interval_ms = 5000;
    config->session_timeout_ms = 30000;
    config->max_poll_interval_ms = 300000;
    
    /* SSL/TLS defaults (disabled) */
    config->enable_ssl = 0;
    config->ssl_ca_location[0] = '\0';
    config->ssl_cert_location[0] = '\0';
    config->ssl_key_location[0] = '\0';
    config->ssl_key_password[0] = '\0';
}

int kafka_consumer_register(KAFKA_CLIENT *client, const char *topic,
                           KAFKA_CONSUMER_CONFIG *config,
                           kafka_consumer_handler_fn handler,
                           void *user_data)
{
    const char *topics[] = { topic };
    return kafka_consumer_register_multi(client, topics, 1, config, handler, user_data);
}

int kafka_consumer_register_multi(KAFKA_CLIENT *client,
                                 const char **topics,
                                 size_t topic_count,
                                 KAFKA_CONSUMER_CONFIG *config,
                                 kafka_consumer_handler_fn handler,
                                 void *user_data)
{
    if (!client || !topics || topic_count == 0 || !config || !handler) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    /* Resize array if needed */
    if (client->consumer_count >= client->consumer_capacity) {
        size_t new_capacity = client->consumer_capacity * 2;
        KAFKA_CONSUMER **new_consumers = (KAFKA_CONSUMER**)realloc(
            client->consumers, new_capacity * sizeof(KAFKA_CONSUMER*));
        if (!new_consumers) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        client->consumers = new_consumers;
        client->consumer_capacity = new_capacity;
    }
    
    /* Create consumer */
    KAFKA_CONSUMER *consumer = (KAFKA_CONSUMER*)calloc(1, sizeof(KAFKA_CONSUMER));
    if (!consumer) {
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    /* Allocate and copy topics */
    consumer->topics = (char**)calloc(topic_count, sizeof(char*));
    if (!consumer->topics) {
        free(consumer);
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    for (size_t i = 0; i < topic_count; i++) {
        consumer->topics[i] = strdup(topics[i]);
        if (!consumer->topics[i]) {
            /* Cleanup on failure */
            for (size_t j = 0; j < i; j++) {
                free(consumer->topics[j]);
            }
            free(consumer->topics);
            free(consumer);
            return FRAMEWORK_ERROR_MEMORY;
        }
    }
    consumer->topic_count = topic_count;
    consumer->handler = handler;
    consumer->user_data = user_data;
    consumer->running = 0;
    
    /* Create Kafka configuration */
    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    char errstr[512];
    
    if (rd_kafka_conf_set(conf, "bootstrap.servers", config->brokers,
                         errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        framework_log(LOG_LEVEL_ERROR, "Failed to set bootstrap.servers: %s", errstr);
        rd_kafka_conf_destroy(conf);
        for (size_t i = 0; i < consumer->topic_count; i++) {
            free(consumer->topics[i]);
        }
        free(consumer->topics);
        free(consumer);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    if (rd_kafka_conf_set(conf, "group.id", config->group_id,
                         errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        framework_log(LOG_LEVEL_ERROR, "Failed to set group.id: %s", errstr);
        rd_kafka_conf_destroy(conf);
        for (size_t i = 0; i < consumer->topic_count; i++) {
            free(consumer->topics[i]);
        }
        free(consumer->topics);
        free(consumer);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    if (rd_kafka_conf_set(conf, "auto.offset.reset", config->auto_offset_reset,
                         errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        framework_log(LOG_LEVEL_ERROR, "Failed to set auto.offset.reset: %s", errstr);
        rd_kafka_conf_destroy(conf);
        for (size_t i = 0; i < consumer->topic_count; i++) {
            free(consumer->topics[i]);
        }
        free(consumer->topics);
        free(consumer);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    char auto_commit[16];
    snprintf(auto_commit, sizeof(auto_commit), "%s", 
            config->enable_auto_commit ? "true" : "false");
    if (rd_kafka_conf_set(conf, "enable.auto.commit", auto_commit,
                         errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        framework_log(LOG_LEVEL_ERROR, "Failed to set enable.auto.commit: %s", errstr);
        rd_kafka_conf_destroy(conf);
        for (size_t i = 0; i < consumer->topic_count; i++) {
            free(consumer->topics[i]);
        }
        free(consumer->topics);
        free(consumer);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* SSL/TLS Configuration */
    if (config->enable_ssl) {
        if (rd_kafka_conf_set(conf, "security.protocol", "ssl",
                             errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            framework_log(LOG_LEVEL_ERROR, "Failed to set security.protocol: %s", errstr);
            rd_kafka_conf_destroy(conf);
            for (size_t i = 0; i < consumer->topic_count; i++) {
                free(consumer->topics[i]);
            }
            free(consumer->topics);
            free(consumer);
            return FRAMEWORK_ERROR_INVALID;
        }
        
        if (strlen(config->ssl_ca_location) > 0) {
            if (rd_kafka_conf_set(conf, "ssl.ca.location", config->ssl_ca_location,
                                 errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                framework_log(LOG_LEVEL_ERROR, "Failed to set ssl.ca.location: %s", errstr);
                goto error_cleanup;
            }
        }
        
        if (strlen(config->ssl_cert_location) > 0) {
            if (rd_kafka_conf_set(conf, "ssl.certificate.location", config->ssl_cert_location,
                                 errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                framework_log(LOG_LEVEL_ERROR, "Failed to set ssl.certificate.location: %s", errstr);
                goto error_cleanup;
            }
        }
        
        if (strlen(config->ssl_key_location) > 0) {
            if (rd_kafka_conf_set(conf, "ssl.key.location", config->ssl_key_location,
                                 errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                framework_log(LOG_LEVEL_ERROR, "Failed to set ssl.key.location: %s", errstr);
                goto error_cleanup;
            }
        }
        
        if (strlen(config->ssl_key_password) > 0) {
            if (rd_kafka_conf_set(conf, "ssl.key.password", config->ssl_key_password,
                                 errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                framework_log(LOG_LEVEL_ERROR, "Failed to set ssl.key.password: %s", errstr);
                goto error_cleanup;
            }
        }
        
        if (consumer->topic_count == 1) {
            framework_log(LOG_LEVEL_INFO, "SSL/TLS enabled for consumer (topic: %s)", 
                         consumer->topics[0]);
        } else {
            framework_log(LOG_LEVEL_INFO, "SSL/TLS enabled for consumer (%zu topics)", 
                         consumer->topic_count);
        }
    }
    
    /* Authentication Configuration */
    if (config->auth_type != KAFKA_AUTH_NONE) {
        const char *mechanism = get_sasl_mechanism(config->auth_type);
        if (!mechanism) {
            framework_log(LOG_LEVEL_ERROR, "Invalid authentication type: %d", config->auth_type);
            goto error_cleanup;
        }
        
        /* Determine security protocol */
        const char *security_protocol = config->enable_ssl ? "sasl_ssl" : "sasl_plaintext";
        if (rd_kafka_conf_set(conf, "security.protocol", security_protocol,
                             errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            framework_log(LOG_LEVEL_ERROR, "Failed to set security.protocol: %s", errstr);
            goto error_cleanup;
        }
        
        /* Set SASL mechanism */
        if (rd_kafka_conf_set(conf, "sasl.mechanism", mechanism,
                             errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.mechanism: %s", errstr);
            goto error_cleanup;
        }
        
        /* Set username and password for SASL mechanisms that use them */
        if (config->auth_type == KAFKA_AUTH_SASL_PLAIN ||
            config->auth_type == KAFKA_AUTH_SASL_SCRAM_SHA256 ||
            config->auth_type == KAFKA_AUTH_SASL_SCRAM_SHA512) {
            
            if (strlen(config->sasl_username) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.username", config->sasl_username,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.username: %s", errstr);
                    goto error_cleanup;
                }
            }
            
            if (strlen(config->sasl_password) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.password", config->sasl_password,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.password: %s", errstr);
                    goto error_cleanup;
                }
            }
        }
        
        /* Kerberos/GSSAPI specific configuration */
        if (config->auth_type == KAFKA_AUTH_SASL_GSSAPI) {
            if (strlen(config->kerberos_service_name) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.kerberos.service.name", config->kerberos_service_name,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.kerberos.service.name: %s", errstr);
                    goto error_cleanup;
                }
            }
            
            if (strlen(config->kerberos_principal) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.kerberos.principal", config->kerberos_principal,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.kerberos.principal: %s", errstr);
                    goto error_cleanup;
                }
            }
            
            if (strlen(config->kerberos_keytab) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.kerberos.keytab", config->kerberos_keytab,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.kerberos.keytab: %s", errstr);
                    goto error_cleanup;
                }
            }
        }
        
        framework_log(LOG_LEVEL_INFO, "Authentication enabled: %s", mechanism);
    }
    
    /* Create consumer instance */
    consumer->rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!consumer->rk) {
        framework_log(LOG_LEVEL_ERROR, "Failed to create Kafka consumer: %s", errstr);
        goto error_cleanup;
    }
    
    /* Subscribe to topics */
    consumer->topic_list = rd_kafka_topic_partition_list_new((int)topic_count);
    for (size_t i = 0; i < topic_count; i++) {
        rd_kafka_topic_partition_list_add(consumer->topic_list, consumer->topics[i], 
                                         RD_KAFKA_PARTITION_UA);
    }
    
    rd_kafka_resp_err_t err = rd_kafka_subscribe(consumer->rk, consumer->topic_list);
    if (err) {
        if (topic_count == 1) {
            framework_log(LOG_LEVEL_ERROR, "Failed to subscribe to topic %s: %s",
                        consumer->topics[0], rd_kafka_err2str(err));
        } else {
            framework_log(LOG_LEVEL_ERROR, "Failed to subscribe to %zu topics: %s",
                        topic_count, rd_kafka_err2str(err));
        }
        rd_kafka_topic_partition_list_destroy(consumer->topic_list);
        rd_kafka_destroy(consumer->rk);
        goto error_cleanup;
    }
    
    /* Add to client */
    client->consumers[client->consumer_count++] = consumer;
    
    if (topic_count == 1) {
        framework_log(LOG_LEVEL_INFO, "Kafka consumer registered for topic: %s (group: %s)",
                     consumer->topics[0], config->group_id);
    } else {
        framework_log(LOG_LEVEL_INFO, "Kafka consumer registered for %zu topics (group: %s)",
                     topic_count, config->group_id);
        for (size_t i = 0; i < topic_count; i++) {
            framework_log(LOG_LEVEL_DEBUG, "  - %s", consumer->topics[i]);
        }
    }
    
    return FRAMEWORK_SUCCESS;

error_cleanup:
    rd_kafka_conf_destroy(conf);
    for (size_t i = 0; i < consumer->topic_count; i++) {
        free(consumer->topics[i]);
    }
    free(consumer->topics);
    free(consumer);
    return FRAMEWORK_ERROR_INVALID;
}

/* ============================================================================
 * Kafka Producer Functions
 * ========================================================================== */

void kafka_producer_config_default(KAFKA_PRODUCER_CONFIG *config,
                                  const char *brokers)
{
    if (!config) return;
    
    memset(config, 0, sizeof(KAFKA_PRODUCER_CONFIG));
    
    if (brokers) {
        strncpy(config->brokers, brokers, sizeof(config->brokers) - 1);
    } else {
        strcpy(config->brokers, "localhost:9092");
    }
    
    config->compression_type = 0;  /* none */
    config->batch_size = 16384;
    config->linger_ms = 0;
    config->acks = 1;  /* leader acknowledgment */
    config->retries = 3;
    
    /* SSL/TLS defaults (disabled) */
    config->enable_ssl = 0;
    config->ssl_ca_location[0] = '\0';
    config->ssl_cert_location[0] = '\0';
    config->ssl_key_location[0] = '\0';
    config->ssl_key_password[0] = '\0';
}

int kafka_producer_init(KAFKA_CLIENT *client, KAFKA_PRODUCER_CONFIG *config)
{
    if (!client || !config) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (client->producer) {
        framework_log(LOG_LEVEL_WARNING, "Producer already initialized");
        return FRAMEWORK_ERROR_STATE;
    }
    
    client->producer = (KAFKA_PRODUCER*)calloc(1, sizeof(KAFKA_PRODUCER));
    if (!client->producer) {
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    memcpy(&client->producer->config, config, sizeof(KAFKA_PRODUCER_CONFIG));
    
    /* Create Kafka configuration */
    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    char errstr[512];
    
    if (rd_kafka_conf_set(conf, "bootstrap.servers", config->brokers,
                         errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        framework_log(LOG_LEVEL_ERROR, "Failed to set bootstrap.servers: %s", errstr);
        rd_kafka_conf_destroy(conf);
        free(client->producer);
        client->producer = NULL;
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Set acks */
    char acks_str[8];
    snprintf(acks_str, sizeof(acks_str), "%d", config->acks);
    if (rd_kafka_conf_set(conf, "acks", acks_str,
                         errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        framework_log(LOG_LEVEL_ERROR, "Failed to set acks: %s", errstr);
        rd_kafka_conf_destroy(conf);
        free(client->producer);
        client->producer = NULL;
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* SSL/TLS Configuration */
    if (config->enable_ssl) {
        if (rd_kafka_conf_set(conf, "security.protocol", "ssl",
                             errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            framework_log(LOG_LEVEL_ERROR, "Failed to set security.protocol: %s", errstr);
            rd_kafka_conf_destroy(conf);
            free(client->producer);
            client->producer = NULL;
            return FRAMEWORK_ERROR_INVALID;
        }
        
        if (strlen(config->ssl_ca_location) > 0) {
            if (rd_kafka_conf_set(conf, "ssl.ca.location", config->ssl_ca_location,
                                 errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                framework_log(LOG_LEVEL_ERROR, "Failed to set ssl.ca.location: %s", errstr);
                rd_kafka_conf_destroy(conf);
                free(client->producer);
                client->producer = NULL;
                return FRAMEWORK_ERROR_INVALID;
            }
        }
        
        if (strlen(config->ssl_cert_location) > 0) {
            if (rd_kafka_conf_set(conf, "ssl.certificate.location", config->ssl_cert_location,
                                 errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                framework_log(LOG_LEVEL_ERROR, "Failed to set ssl.certificate.location: %s", errstr);
                rd_kafka_conf_destroy(conf);
                free(client->producer);
                client->producer = NULL;
                return FRAMEWORK_ERROR_INVALID;
            }
        }
        
        if (strlen(config->ssl_key_location) > 0) {
            if (rd_kafka_conf_set(conf, "ssl.key.location", config->ssl_key_location,
                                 errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                framework_log(LOG_LEVEL_ERROR, "Failed to set ssl.key.location: %s", errstr);
                rd_kafka_conf_destroy(conf);
                free(client->producer);
                client->producer = NULL;
                return FRAMEWORK_ERROR_INVALID;
            }
        }
        
        if (strlen(config->ssl_key_password) > 0) {
            if (rd_kafka_conf_set(conf, "ssl.key.password", config->ssl_key_password,
                                 errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                framework_log(LOG_LEVEL_ERROR, "Failed to set ssl.key.password: %s", errstr);
                rd_kafka_conf_destroy(conf);
                free(client->producer);
                client->producer = NULL;
                return FRAMEWORK_ERROR_INVALID;
            }
        }
        
        framework_log(LOG_LEVEL_INFO, "SSL/TLS enabled for producer");
    }
    
    /* Authentication Configuration */
    if (config->auth_type != KAFKA_AUTH_NONE) {
        const char *mechanism = get_sasl_mechanism(config->auth_type);
        if (!mechanism) {
            framework_log(LOG_LEVEL_ERROR, "Invalid authentication type: %d", config->auth_type);
            rd_kafka_conf_destroy(conf);
            free(client->producer);
            client->producer = NULL;
            return FRAMEWORK_ERROR_INVALID;
        }
        
        /* Determine security protocol */
        const char *security_protocol = config->enable_ssl ? "sasl_ssl" : "sasl_plaintext";
        if (rd_kafka_conf_set(conf, "security.protocol", security_protocol,
                             errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            framework_log(LOG_LEVEL_ERROR, "Failed to set security.protocol: %s", errstr);
            rd_kafka_conf_destroy(conf);
            free(client->producer);
            client->producer = NULL;
            return FRAMEWORK_ERROR_INVALID;
        }
        
        /* Set SASL mechanism */
        if (rd_kafka_conf_set(conf, "sasl.mechanism", mechanism,
                             errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.mechanism: %s", errstr);
            rd_kafka_conf_destroy(conf);
            free(client->producer);
            client->producer = NULL;
            return FRAMEWORK_ERROR_INVALID;
        }
        
        /* Set username and password for SASL mechanisms that use them */
        if (config->auth_type == KAFKA_AUTH_SASL_PLAIN ||
            config->auth_type == KAFKA_AUTH_SASL_SCRAM_SHA256 ||
            config->auth_type == KAFKA_AUTH_SASL_SCRAM_SHA512) {
            
            if (strlen(config->sasl_username) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.username", config->sasl_username,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.username: %s", errstr);
                    rd_kafka_conf_destroy(conf);
                    free(client->producer);
                    client->producer = NULL;
                    return FRAMEWORK_ERROR_INVALID;
                }
            }
            
            if (strlen(config->sasl_password) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.password", config->sasl_password,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.password: %s", errstr);
                    rd_kafka_conf_destroy(conf);
                    free(client->producer);
                    client->producer = NULL;
                    return FRAMEWORK_ERROR_INVALID;
                }
            }
        }
        
        /* Kerberos/GSSAPI specific configuration */
        if (config->auth_type == KAFKA_AUTH_SASL_GSSAPI) {
            if (strlen(config->kerberos_service_name) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.kerberos.service.name", config->kerberos_service_name,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.kerberos.service.name: %s", errstr);
                    rd_kafka_conf_destroy(conf);
                    free(client->producer);
                    client->producer = NULL;
                    return FRAMEWORK_ERROR_INVALID;
                }
            }
            
            if (strlen(config->kerberos_principal) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.kerberos.principal", config->kerberos_principal,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.kerberos.principal: %s", errstr);
                    rd_kafka_conf_destroy(conf);
                    free(client->producer);
                    client->producer = NULL;
                    return FRAMEWORK_ERROR_INVALID;
                }
            }
            
            if (strlen(config->kerberos_keytab) > 0) {
                if (rd_kafka_conf_set(conf, "sasl.kerberos.keytab", config->kerberos_keytab,
                                     errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                    framework_log(LOG_LEVEL_ERROR, "Failed to set sasl.kerberos.keytab: %s", errstr);
                    rd_kafka_conf_destroy(conf);
                    free(client->producer);
                    client->producer = NULL;
                    return FRAMEWORK_ERROR_INVALID;
                }
            }
        }
        
        framework_log(LOG_LEVEL_INFO, "Authentication enabled for producer: %s", mechanism);
    }
    
    /* Create producer instance */
    client->producer->rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!client->producer->rk) {
        framework_log(LOG_LEVEL_ERROR, "Failed to create Kafka producer: %s", errstr);
        rd_kafka_conf_destroy(conf);
        free(client->producer);
        client->producer = NULL;
        return FRAMEWORK_ERROR_INVALID;
    }
    
    framework_log(LOG_LEVEL_INFO, "Kafka producer initialized (brokers: %s)", 
                 config->brokers);
    
    return FRAMEWORK_SUCCESS;
}

int kafka_produce(KAFKA_CLIENT *client, const char *topic,
                 const void *key, size_t key_len,
                 const void *payload, size_t payload_len)
{
    if (!client || !topic || !payload) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (!client->producer || !client->producer->rk) {
        framework_log(LOG_LEVEL_ERROR, "Producer not initialized");
        return FRAMEWORK_ERROR_STATE;
    }
    
    rd_kafka_resp_err_t err = rd_kafka_producev(
        client->producer->rk,
        RD_KAFKA_V_TOPIC(topic),
        RD_KAFKA_V_KEY(key, key_len),
        RD_KAFKA_V_VALUE((void*)payload, payload_len),
        RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
        RD_KAFKA_V_END);
    
    if (err) {
        framework_log(LOG_LEVEL_ERROR, "Failed to produce message to topic %s: %s",
                    topic, rd_kafka_err2str(err));
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Poll to handle delivery reports */
    rd_kafka_poll(client->producer->rk, 0);
    
    framework_log(LOG_LEVEL_DEBUG, "Message produced to topic: %s (%zu bytes)",
                 topic, payload_len);
    
    return FRAMEWORK_SUCCESS;
}

int kafka_produce_string(KAFKA_CLIENT *client, const char *topic,
                        const char *key, const char *payload)
{
    if (!payload) return FRAMEWORK_ERROR_NULL_PTR;
    
    size_t key_len = key ? strlen(key) : 0;
    size_t payload_len = strlen(payload);
    
    return kafka_produce(client, topic, key, key_len, payload, payload_len);
}

/* ============================================================================
 * Kafka Message Functions
 * ========================================================================== */

const char* kafka_message_get_payload_string(KAFKA_MESSAGE *message)
{
    if (!message || !message->payload) return NULL;
    
    /* Allocate buffer with null terminator if needed */
    static __thread char buffer[65536];
    size_t copy_len = message->payload_len < sizeof(buffer) - 1 ? 
                     message->payload_len : sizeof(buffer) - 1;
    
    memcpy(buffer, message->payload, copy_len);
    buffer[copy_len] = '\0';
    
    return buffer;
}

const char* kafka_message_get_key_string(KAFKA_MESSAGE *message)
{
    if (!message || !message->key) return NULL;
    
    static __thread char buffer[1024];
    size_t copy_len = message->key_len < sizeof(buffer) - 1 ? 
                     message->key_len : sizeof(buffer) - 1;
    
    memcpy(buffer, message->key, copy_len);
    buffer[copy_len] = '\0';
    
    return buffer;
}

/* ============================================================================
 * Application Integration
 * ========================================================================== */
