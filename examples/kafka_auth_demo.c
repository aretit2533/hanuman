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

void handle_secure_message(KAFKA_MESSAGE *message, void *user_data)
{
    (void)user_data;
    
    const char *payload = kafka_message_get_payload_string(message);
    printf("🔐 Authenticated message received from %s: %s\n", message->topic, payload);
}

int main(int argc, char *argv[])
{
    /* Parse command line arguments */
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <broker> <username> <password> [auth-type]\n", argv[0]);
        fprintf(stderr, "\nAuthentication types:\n");
        fprintf(stderr, "  plain      - SASL/PLAIN (default)\n");
        fprintf(stderr, "  scram256   - SASL/SCRAM-SHA-256\n");
        fprintf(stderr, "  scram512   - SASL/SCRAM-SHA-512\n");
        fprintf(stderr, "  gssapi     - SASL/GSSAPI (Kerberos)\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  %s localhost:9092 myuser mypassword\n", argv[0]);
        fprintf(stderr, "  %s localhost:9092 admin secret123 scram256\n", argv[0]);
        return 1;
    }
    
    const char *broker = argv[1];
    const char *username = argv[2];
    const char *password = argv[3];
    const char *auth_type_str = (argc > 4) ? argv[4] : "plain";
    
    /* Determine authentication type */
    KAFKA_AUTH_TYPE auth_type = KAFKA_AUTH_SASL_PLAIN;
    if (strcmp(auth_type_str, "scram256") == 0) {
        auth_type = KAFKA_AUTH_SASL_SCRAM_SHA256;
    } else if (strcmp(auth_type_str, "scram512") == 0) {
        auth_type = KAFKA_AUTH_SASL_SCRAM_SHA512;
    } else if (strcmp(auth_type_str, "gssapi") == 0) {
        auth_type = KAFKA_AUTH_SASL_GSSAPI;
    }
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create application */
    APPLICATION *app = application_create("Kafka Auth Demo", 1);
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
    
    application_set_kafka_client(app, kafka);
    
    /* Configure consumer with authentication */
    KAFKA_CONSUMER_CONFIG consumer_config;
    kafka_consumer_config_default(&consumer_config, broker, "auth-demo-group");
    strcpy(consumer_config.auto_offset_reset, "earliest");
    
    /* Enable authentication */
    consumer_config.auth_type = auth_type;
    strncpy(consumer_config.sasl_username, username, sizeof(consumer_config.sasl_username) - 1);
    strncpy(consumer_config.sasl_password, password, sizeof(consumer_config.sasl_password) - 1);
    
    printf("Kafka Authentication Configuration:\n");
    printf("  Broker: %s\n", broker);
    printf("  Username: %s\n", username);
    printf("  Auth Type: %s\n", auth_type_str);
    printf("\n");
    
    /* Register consumer */
    printf("Registering authenticated consumer...\n");
    if (kafka_consumer_register(kafka, "authenticated-events", &consumer_config,
                               handle_secure_message, NULL) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register consumer\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Configure producer with authentication */
    KAFKA_PRODUCER_CONFIG producer_config;
    kafka_producer_config_default(&producer_config, broker);
    producer_config.acks = -1;
    
    /* Enable authentication for producer */
    producer_config.auth_type = auth_type;
    strncpy(producer_config.sasl_username, username, sizeof(producer_config.sasl_username) - 1);
    strncpy(producer_config.sasl_password, password, sizeof(producer_config.sasl_password) - 1);
    
    printf("Initializing authenticated producer...\n");
    if (kafka_producer_init(kafka, &producer_config) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to initialize producer\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Start Kafka client */
    printf("Starting Kafka client...\n");
    if (kafka_client_start(kafka) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start Kafka client\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║   Kafka Authentication Demo Running                ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("🔐 Authentication: %s\n", auth_type_str);
    printf("👤 Username: %s\n", username);
    printf("📡 Broker: %s\n", broker);
    printf("📥 Consuming: authenticated-events\n");
    printf("📤 Producer: Ready\n");
    printf("\n");
    printf("Press Ctrl+C to stop...\n");
    printf("════════════════════════════════════════════════════\n\n");
    
    /* Produce authenticated messages every 10 seconds */
    int counter = 0;
    while (app_running) {
        sleep(10);
        
        if (!app_running) break;
        
        counter++;
        char payload[256];
        snprintf(payload, sizeof(payload),
                "{\"event\":\"secure_event\",\"id\":%d,\"user\":\"%s\",\"auth\":\"%s\"}",
                counter, username, auth_type_str);
        
        kafka_produce_string(kafka, "authenticated-events", username, payload);
        printf("✓ Authenticated message sent: %s\n", payload);
    }
    
    printf("\nStopping Kafka client...\n");
    kafka_client_stop(kafka);
    
    kafka_client_destroy(kafka);
    application_destroy(app);
    
    printf("Application stopped successfully\n");
    return 0;
}
