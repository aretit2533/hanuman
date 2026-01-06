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

/* Handler for user-events topic */
void handle_user_event(KAFKA_MESSAGE *message, void *user_data)
{
    (void)user_data;
    
    const char *key = kafka_message_get_key_string(message);
    const char *payload = kafka_message_get_payload_string(message);
    
    framework_log(LOG_LEVEL_INFO, "Received user event:");
    framework_log(LOG_LEVEL_INFO, "  Topic: %s", message->topic);
    framework_log(LOG_LEVEL_INFO, "  Partition: %d", message->partition);
    framework_log(LOG_LEVEL_INFO, "  Offset: %lld", message->offset);
    framework_log(LOG_LEVEL_INFO, "  Key: %s", key ? key : "(null)");
    framework_log(LOG_LEVEL_INFO, "  Payload: %s", payload ? payload : "(null)");
    
    /* Process the user event here */
    printf("Processing user event: %s\n", payload ? payload : "(null)");
}

/* Handler for order-events topic */
void handle_order_event(KAFKA_MESSAGE *message, void *user_data)
{
    (void)user_data;
    
    const char *payload = kafka_message_get_payload_string(message);
    
    framework_log(LOG_LEVEL_INFO, "Received order event: %s", 
                 payload ? payload : "(null)");
    
    /* Process the order event here */
    printf("Processing order event: %s\n", payload ? payload : "(null)");
}

/* Handler for notification-events topic */
void handle_notification_event(KAFKA_MESSAGE *message, void *user_data)
{
    (void)user_data;
    
    const char *key = kafka_message_get_key_string(message);
    const char *payload = kafka_message_get_payload_string(message);
    
    framework_log(LOG_LEVEL_INFO, "Received notification [key=%s]: %s",
                 key ? key : "(null)", payload ? payload : "(null)");
    
    /* Process the notification here */
    printf("Sending notification: %s\n", payload ? payload : "(null)");
}

int main(void)
{
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create application */
    APPLICATION *app = application_create("Kafka Demo Application", 1);
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
    
    /* Configure consumer */
    KAFKA_CONSUMER_CONFIG consumer_config;
    kafka_consumer_config_default(&consumer_config, "localhost:9092", "demo-group");
    strcpy(consumer_config.auto_offset_reset, "earliest");  /* Start from beginning */
    
    /* Register consumers for different topics (like registering HTTP routes!) */
    printf("Registering Kafka consumers...\n");
    
    if (kafka_consumer_register(kafka, "user-events", &consumer_config,
                               handle_user_event, NULL) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register consumer for user-events\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    if (kafka_consumer_register(kafka, "order-events", &consumer_config,
                               handle_order_event, NULL) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register consumer for order-events\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    if (kafka_consumer_register(kafka, "notification-events", &consumer_config,
                               handle_notification_event, NULL) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register consumer for notification-events\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Initialize producer */
    KAFKA_PRODUCER_CONFIG producer_config;
    kafka_producer_config_default(&producer_config, "localhost:9092");
    producer_config.acks = -1;  /* Wait for all replicas */
    
    if (kafka_producer_init(kafka, &producer_config) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to initialize Kafka producer\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Start Kafka client (begins consuming) */
    printf("Starting Kafka client...\n");
    if (kafka_client_start(kafka) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start Kafka client\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    printf("\n=======================================================\n");
    printf("Kafka Demo Application Running\n");
    printf("=======================================================\n");
    printf("Consuming from topics:\n");
    printf("  - user-events\n");
    printf("  - order-events\n");
    printf("  - notification-events\n");
    printf("\nProducer ready to send messages\n");
    printf("\nPress Ctrl+C to stop...\n");
    printf("=======================================================\n\n");
    
    /* Produce some test messages every 10 seconds */
    int counter = 0;
    while (app_running) {
        sleep(10);
        
        if (!app_running) break;
        
        counter++;
        
        /* Produce test messages */
        char payload[256];
        
        snprintf(payload, sizeof(payload), 
                "{\"event\":\"login\",\"user_id\":\"user%d\",\"timestamp\":%d}",
                counter, counter * 10);
        kafka_produce_string(kafka, "user-events", "user123", payload);
        printf("Produced message to user-events: %s\n", payload);
        
        snprintf(payload, sizeof(payload),
                "{\"event\":\"order_created\",\"order_id\":%d,\"amount\":%.2f}",
                counter, counter * 99.99);
        kafka_produce_string(kafka, "order-events", NULL, payload);
        printf("Produced message to order-events: %s\n", payload);
        
        snprintf(payload, sizeof(payload),
                "{\"type\":\"email\",\"recipient\":\"user@example.com\",\"message\":\"Test %d\"}",
                counter);
        kafka_produce_string(kafka, "notification-events", "email-notif", payload);
        printf("Produced message to notification-events: %s\n\n", payload);
    }
    
    printf("\nStopping Kafka client...\n");
    kafka_client_stop(kafka);
    
    /* Cleanup */
    kafka_client_destroy(kafka);
    application_destroy(app);
    
    printf("Application stopped successfully\n");
    return 0;
}
