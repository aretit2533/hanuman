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

/* Single handler for all events from multiple topics */
void handle_all_events(KAFKA_MESSAGE *message, void *user_data)
{
    (void)user_data;
    
    const char *key = kafka_message_get_key_string(message);
    const char *payload = kafka_message_get_payload_string(message);
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("📨 Message from topic: %s\n", message->topic);
    printf("   Partition: %d | Offset: %ld\n", message->partition, (long)message->offset);
    if (key) {
        printf("   Key: %s\n", key);
    }
    printf("   Payload: %s\n", payload ? payload : "(null)");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
}

/* Separate handler for monitoring topics */
void handle_monitoring_events(KAFKA_MESSAGE *message, void *user_data)
{
    (void)user_data;
    
    const char *payload = kafka_message_get_payload_string(message);
    
    printf("📊 [MONITORING] %s: %s\n", message->topic, payload ? payload : "(null)");
}

int main(void)
{
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create application */
    APPLICATION *app = application_create("Multi-Topic Kafka Demo", 1);
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
    kafka_consumer_config_default(&consumer_config, "localhost:9092", "multi-topic-group");
    strcpy(consumer_config.auto_offset_reset, "earliest");
    
    /* Register consumer for multiple business event topics with single handler */
    printf("Registering multi-topic consumer for business events...\n");
    const char *business_topics[] = {
        "user-events",
        "order-events",
        "payment-events",
        "inventory-events"
    };
    
    if (kafka_consumer_register_multi(kafka, business_topics, 4, &consumer_config,
                                     handle_all_events, NULL) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register multi-topic consumer\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Register separate consumer for monitoring topics */
    printf("Registering multi-topic consumer for monitoring...\n");
    const char *monitoring_topics[] = {
        "metrics",
        "logs",
        "alerts"
    };
    
    if (kafka_consumer_register_multi(kafka, monitoring_topics, 3, &consumer_config,
                                     handle_monitoring_events, NULL) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register monitoring consumer\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* You can still register single-topic consumers */
    printf("Registering single-topic consumer for notifications...\n");
    if (kafka_consumer_register(kafka, "notifications", &consumer_config,
                               handle_all_events, NULL) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register notifications consumer\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Initialize producer */
    KAFKA_PRODUCER_CONFIG producer_config;
    kafka_producer_config_default(&producer_config, "localhost:9092");
    producer_config.acks = -1;
    
    if (kafka_producer_init(kafka, &producer_config) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to initialize Kafka producer\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    /* Start Kafka client */
    printf("\nStarting Kafka client...\n");
    if (kafka_client_start(kafka) != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start Kafka client\n");
        kafka_client_destroy(kafka);
        application_destroy(app);
        return 1;
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     Multi-Topic Kafka Consumer Demo Running              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("📥 Business Events Consumer:\n");
    printf("   ├─ user-events\n");
    printf("   ├─ order-events\n");
    printf("   ├─ payment-events\n");
    printf("   └─ inventory-events\n");
    printf("\n");
    printf("📊 Monitoring Consumer:\n");
    printf("   ├─ metrics\n");
    printf("   ├─ logs\n");
    printf("   └─ alerts\n");
    printf("\n");
    printf("🔔 Single Topic Consumer:\n");
    printf("   └─ notifications\n");
    printf("\n");
    printf("📤 Producer: Ready to send test messages\n");
    printf("\n");
    printf("Press Ctrl+C to stop...\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    /* Produce test messages to different topics every 10 seconds */
    int counter = 0;
    while (app_running) {
        sleep(10);
        
        if (!app_running) break;
        
        counter++;
        char payload[256];
        
        /* Send to user-events */
        snprintf(payload, sizeof(payload), 
                "{\"event\":\"user_login\",\"user_id\":%d,\"timestamp\":%d}",
                counter, counter * 1000);
        kafka_produce_string(kafka, "user-events", "user-key", payload);
        printf("✓ Sent to user-events\n");
        
        /* Send to order-events */
        snprintf(payload, sizeof(payload),
                "{\"event\":\"order_placed\",\"order_id\":%d,\"amount\":%.2f}",
                counter, counter * 99.99);
        kafka_produce_string(kafka, "order-events", "order-key", payload);
        printf("✓ Sent to order-events\n");
        
        /* Send to metrics */
        snprintf(payload, sizeof(payload),
                "{\"cpu\":%.1f,\"memory\":%.1f,\"timestamp\":%d}",
                45.5 + (counter % 10), 67.8 + (counter % 10), counter);
        kafka_produce_string(kafka, "metrics", NULL, payload);
        printf("✓ Sent to metrics\n");
        
        /* Send to notifications */
        snprintf(payload, sizeof(payload),
                "{\"type\":\"info\",\"message\":\"Test notification %d\"}",
                counter);
        kafka_produce_string(kafka, "notifications", NULL, payload);
        printf("✓ Sent to notifications\n\n");
    }
    
    printf("\nStopping Kafka client...\n");
    kafka_client_stop(kafka);
    
    /* Cleanup */
    kafka_client_destroy(kafka);
    application_destroy(app);
    
    printf("Application stopped successfully\n");
    return 0;
}
