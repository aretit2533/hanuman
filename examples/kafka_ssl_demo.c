#include "application.h"
#include "kafka_client.h"
#include "framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static int app_running = 1;

void signal_handler(int sig)
{
    (void)sig;
    app_running = 0;
}

/* Handler for secure-events topic */
void handle_secure_event(KAFKA_MESSAGE *message, void *user_data)
{
    (void)user_data;
    
    const char *key = kafka_message_get_key_string(message);
    const char *payload = kafka_message_get_payload_string(message);
    
    framework_log(LOG_LEVEL_INFO, "Received secure event (encrypted):");
    framework_log(LOG_LEVEL_INFO, "  Topic: %s", message->topic);
    framework_log(LOG_LEVEL_INFO, "  Partition: %d", message->partition);
    framework_log(LOG_LEVEL_INFO, "  Offset: %lld", message->offset);
    framework_log(LOG_LEVEL_INFO, "  Key: %s", key ? key : "(null)");
    framework_log(LOG_LEVEL_INFO, "  Payload: %s", payload ? payload : "(null)");
    
    printf("✓ Securely received: %s\n", payload ? payload : "(null)");
}

int main(int argc, char *argv[])
{
    /* Parse command line arguments */
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <broker:port> <ca-cert> <client-cert> <client-key> [key-password]\n", argv[0]);
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s localhost:9093 /etc/kafka/ssl/ca-cert.pem \\\n", argv[0]);
        fprintf(stderr, "                      /etc/kafka/ssl/client-cert.pem \\\n");
        fprintf(stderr, "                      /etc/kafka/ssl/client-key.pem\n");
        return 1;
    }
    
    const char *broker = argv[1];
    const char *ca_cert = argv[2];
    const char *client_cert = argv[3];
    const char *client_key = argv[4];
    const char *key_password = (argc > 5) ? argv[5] : "";
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create application */
    APPLICATION *app = application_create("Kafka SSL Demo", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Create Kafka client */
    KAFKA_CLIENT *kafka = kafka_client_create();
    if (!kafka) {
        fprintf(stderr, "Failed to create Kafka client\n");
        application_destroy(app);
        return 1;
    }
    
    /* Associate Kafka client with application */
    application_set_kafka_client(app, kafka);
    
    /* Configure consumer with SSL */
    KAFKA_CONSUMER_CONFIG consumer_config;
    kafka_consumer_config_default(&consumer_config, broker, "ssl-demo-group");
    strcpy(consumer_config.auto_offset_reset, "earliest");
    
    /* Enable SSL/TLS */
    consumer_config.enable_ssl = 1;
    strncpy(consumer_config.ssl_ca_location, ca_cert, sizeof(consumer_config.ssl_ca_location) - 1);
    strncpy(consumer_config.ssl_cert_location, client_cert, sizeof(consumer_config.ssl_cert_location) - 1);
    strncpy(consumer_config.ssl_key_location, client_key, sizeof(consumer_config.ssl_key_location) - 1);
    if (strlen(key_password) > 0) {
        strncpy(consumer_config.ssl_key_password, key_password, sizeof(consumer_config.ssl_key_password) - 1);
    }
    
    printf("Configuring SSL/TLS Consumer:\n");
    printf("  Broker: %s\n", broker);
    printf("  CA Certificate: %s\n", ca_cert);
    printf("  Client Certificate: %s\n", client_cert);
    printf("  Client Key: %s\n", client_key);
    printf("\n");
    
    /* Register consumer for secure-events topic */
    printf("Registering secure consumer...\n");
    if (kafka_consumer_register(kafka, "secure-events", &consumer_config,
                               handle_secure_event, NULL) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register consumer for secure-events\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Initialize producer with SSL */
    KAFKA_PRODUCER_CONFIG producer_config;
    kafka_producer_config_default(&producer_config, broker);
    producer_config.acks = -1;  /* Wait for all replicas */
    
    /* Enable SSL/TLS for producer */
    producer_config.enable_ssl = 1;
    strncpy(producer_config.ssl_ca_location, ca_cert, sizeof(producer_config.ssl_ca_location) - 1);
    strncpy(producer_config.ssl_cert_location, client_cert, sizeof(producer_config.ssl_cert_location) - 1);
    strncpy(producer_config.ssl_key_location, client_key, sizeof(producer_config.ssl_key_location) - 1);
    if (strlen(key_password) > 0) {
        strncpy(producer_config.ssl_key_password, key_password, sizeof(producer_config.ssl_key_password) - 1);
    }
    
    printf("Initializing SSL/TLS Producer...\n");
    if (kafka_producer_init(kafka, &producer_config) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to initialize Kafka producer\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Start Kafka client (begins consuming) */
    printf("Starting SSL/TLS Kafka client...\n");
    if (kafka_client_start(kafka) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start Kafka client\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    printf("\n=======================================================\n");
    printf("Kafka SSL/TLS Demo Application Running\n");
    printf("=======================================================\n");
    printf("🔒 SSL/TLS Encryption: ENABLED\n");
    printf("📡 Broker: %s\n", broker);
    printf("📥 Consuming from: secure-events\n");
    printf("📤 Producer ready for secure messaging\n");
    printf("\nPress Ctrl+C to stop...\n");
    printf("=======================================================\n\n");
    
    /* Produce encrypted test messages every 10 seconds */
    int counter = 0;
    while (app_running) {
        sleep(10);
        
        if (!app_running) break;
        
        counter++;
        
        /* Produce secure test message */
        char payload[256];
        snprintf(payload, sizeof(payload), 
                "{\"event\":\"secure_transaction\",\"id\":%d,\"amount\":%.2f,\"encrypted\":true}",
                counter, counter * 123.45);
        
        kafka_produce_string(kafka, "secure-events", "secure-key", payload);
        printf("🔒 Encrypted message sent: %s\n\n", payload);
    }
    
    printf("\nStopping Kafka SSL client...\n");
    kafka_client_stop(kafka);
    
    /* Cleanup */
    kafka_client_destroy(kafka);
    application_destroy(app);
    
    printf("✓ Application stopped securely\n");
    return 0;
}
