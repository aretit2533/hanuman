#include "mongo_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Explicit function declaration */
extern int usleep(unsigned int usec);

/* Simple logging functions */
#define log_error(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define log_info(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)

static volatile int operation_complete = 0;
static volatile int doc_count = 0;

void on_insert_complete(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("✓ Async insert successful! ID: %s\n", result->data ? result->data : "N/A");
    } else {
        printf("✗ Async insert failed\n");
    }
    operation_complete = 1;
}

void on_find_document(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        doc_count++;
        printf("  [%d] %s\n", doc_count, result->data);
    } else {
        /* End of stream */
        printf("✓ Async find complete. Found %d documents\n", doc_count);
        operation_complete = 1;
        doc_count = 0;
    }
}

void on_find_one_complete(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("✓ Async find_one successful:\n  %s\n", result->data);
    } else {
        printf("✗ Document not found\n");
    }
    operation_complete = 1;
}

void on_update_complete(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("✓ Async update successful! Affected: %lld\n", 
               (long long)result->affected_count);
    } else {
        printf("✗ Async update failed\n");
    }
    operation_complete = 1;
}

void on_delete_complete(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("✓ Async delete successful! Deleted: %lld\n", 
               (long long)result->affected_count);
    } else {
        printf("✗ Async delete failed\n");
    }
    operation_complete = 1;
}

void on_count_complete(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("✓ Async count complete: %lld documents\n", 
               (long long)result->affected_count);
    } else {
        printf("✗ Async count failed\n");
    }
    operation_complete = 1;
}

void on_aggregate_document(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        doc_count++;
        printf("  [%d] %s\n", doc_count, result->data);
    } else {
        /* End of stream */
        printf("✓ Async aggregation complete. Found %d groups\n", doc_count);
        operation_complete = 1;
        doc_count = 0;
    }
}

void wait_for_completion(void) {
    while (!operation_complete) {
        usleep(100000); /* 100ms */
    }
    operation_complete = 0;
}

int main(int argc, char *argv[]) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     Hanuman Framework - MongoDB Async Demo                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* Get MongoDB URI from command line or use default */
    const char *mongo_uri = "mongodb://localhost:27017";
    if (argc > 1) {
        mongo_uri = argv[1];
    }
    
    printf("\nConnecting to MongoDB: %s\n", mongo_uri);
    
    /* Create MongoDB client */
    MONGO_CONFIG config;
    mongo_config_default(&config, mongo_uri, "asyncdb");
    
    MONGO_CLIENT *client = mongo_client_create(&config);
    if (!client) {
        log_error("Failed to create MongoDB client");
        return 1;
    }
    
    /* Test connection */
    if (!mongo_client_ping(client)) {
        log_error("Failed to connect to MongoDB");
        mongo_client_destroy(client);
        return 1;
    }
    printf("✓ Connected successfully!\n\n");
    
    /* Get collection */
    MONGO_COLLECTION *users = mongo_client_get_collection(client, NULL, "async_users");
    if (!users) {
        log_error("Failed to get collection");
        mongo_client_destroy(client);
        return 1;
    }
    
    printf("═══ Asynchronous Operations Demo ═══\n\n");
    
    /* 1. Async Insert */
    printf("1. Async Insert One:\n");
    MONGO_ASYNC_HANDLE *handle = mongo_insert_one_async(
        users,
        "{\"name\":\"Alice\",\"age\":28,\"city\":\"Seattle\"}",
        on_insert_complete,
        NULL
    );
    if (handle) {
        wait_for_completion();
        mongo_async_handle_destroy(handle);
    }
    printf("\n");
    
    /* Insert more documents synchronously for demo */
    const char *docs[] = {
        "{\"name\":\"Bob\",\"age\":35,\"city\":\"New York\"}",
        "{\"name\":\"Charlie\",\"age\":42,\"city\":\"Seattle\"}",
        "{\"name\":\"Diana\",\"age\":30,\"city\":\"New York\"}"
    };
    mongo_insert_many(users, docs, 3);
    printf("(Inserted 3 more documents synchronously for demo)\n\n");
    
    /* 2. Async Find */
    printf("2. Async Find (age >= 30):\n");
    handle = mongo_find_async(
        users,
        "{\"age\":{\"$gte\":30}}",
        NULL,
        on_find_document,
        NULL
    );
    if (handle) {
        wait_for_completion();
        mongo_async_handle_destroy(handle);
    }
    printf("\n");
    
    /* 3. Async Find One */
    printf("3. Async Find One (name = 'Alice'):\n");
    handle = mongo_find_one_async(
        users,
        "{\"name\":\"Alice\"}",
        on_find_one_complete,
        NULL
    );
    if (handle) {
        wait_for_completion();
        mongo_async_handle_destroy(handle);
    }
    printf("\n");
    
    /* 4. Async Update */
    printf("4. Async Update (set Alice's age to 29):\n");
    handle = mongo_update_async(
        users,
        "{\"name\":\"Alice\"}",
        "{\"$set\":{\"age\":29}}",
        NULL,
        on_update_complete,
        NULL
    );
    if (handle) {
        wait_for_completion();
        mongo_async_handle_destroy(handle);
    }
    printf("\n");
    
    /* 5. Async Count */
    printf("5. Async Count (all documents):\n");
    handle = mongo_count_async(
        users,
        NULL,
        on_count_complete,
        NULL
    );
    if (handle) {
        wait_for_completion();
        mongo_async_handle_destroy(handle);
    }
    printf("\n");
    
    /* 6. Async Aggregation */
    printf("6. Async Aggregation (group by city):\n");
    const char *pipeline = "["
        "{\"$group\":{"
            "\"_id\":\"$city\","
            "\"count\":{\"$sum\":1},"
            "\"avgAge\":{\"$avg\":\"$age\"}"
        "}},"
        "{\"$sort\":{\"count\":-1}}"
    "]";
    
    handle = mongo_aggregate_async(
        users,
        pipeline,
        on_aggregate_document,
        NULL
    );
    if (handle) {
        wait_for_completion();
        mongo_async_handle_destroy(handle);
    }
    printf("\n");
    
    /* 7. Async Delete */
    printf("7. Async Delete (delete Bob):\n");
    handle = mongo_delete_async(
        users,
        "{\"name\":\"Bob\"}",
        NULL,
        on_delete_complete,
        NULL
    );
    if (handle) {
        wait_for_completion();
        mongo_async_handle_destroy(handle);
    }
    printf("\n");
    
    /* 8. Multiple concurrent async operations */
    printf("8. Concurrent Async Operations:\n");
    printf("   Launching 3 async inserts simultaneously...\n");
    
    MONGO_ASYNC_HANDLE *h1 = mongo_insert_one_async(
        users, "{\"name\":\"Eve\",\"age\":25,\"city\":\"Boston\"}", 
        on_insert_complete, NULL
    );
    MONGO_ASYNC_HANDLE *h2 = mongo_insert_one_async(
        users, "{\"name\":\"Frank\",\"age\":38,\"city\":\"Chicago\"}", 
        on_insert_complete, NULL
    );
    MONGO_ASYNC_HANDLE *h3 = mongo_insert_one_async(
        users, "{\"name\":\"Grace\",\"age\":32,\"city\":\"Miami\"}", 
        on_insert_complete, NULL
    );
    
    /* Wait for all to complete */
    if (h1) {
        while (!mongo_async_is_complete(h1)) usleep(50000);
        mongo_async_handle_destroy(h1);
    }
    if (h2) {
        while (!mongo_async_is_complete(h2)) usleep(50000);
        mongo_async_handle_destroy(h2);
    }
    if (h3) {
        while (!mongo_async_is_complete(h3)) usleep(50000);
        mongo_async_handle_destroy(h3);
    }
    printf("   ✓ All 3 async inserts completed!\n\n");
    
    /* Cleanup */
    printf("═══ Cleanup ═══\n");
    printf("Dropping collection...\n");
    mongo_drop_collection(users);
    mongo_collection_destroy(users);
    mongo_client_destroy(client);
    
    printf("\n✓ Demo completed successfully!\n\n");
    printf("Usage: %s [mongodb://host:port]\n", argv[0]);
    printf("Example: %s mongodb://localhost:27017\n\n", argv[0]);
    
    return 0;
}
