/*
 * MongoDB Async Handle Management Demo
 * Demonstrates different patterns for managing MONGO_ASYNC_HANDLE lifecycle
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mongo_client.h"

/* Explicit declaration for usleep in C99 mode */
extern int usleep(unsigned int usec);

volatile int external_complete = 0;
volatile int self_cleanup_complete = 0;
volatile int concurrent_count = 0;

/*
 * Pattern 1: External Handle Management
 * - Application keeps track of handle
 * - Must explicitly wait and destroy handle
 * - Good for: Operations that need result synchronously
 */
void on_insert_external(MONGO_ASYNC_RESULT *result) {
    printf("\n[External Pattern] Insert callback\n");
    if (result->success) {
        printf("  ✓ Document inserted successfully\n");
        if (result->data) {
            printf("  Inserted ID: %s\n", result->data);
        }
    } else {
        printf("  ✗ Insert failed: %s\n", result->error_message);
    }
    external_complete = 1;
}

/*
 * Pattern 2: Self-Cleanup in Callback
 * - Callback destroys its own handle using result->handle
 * - Fire-and-forget pattern
 * - Good for: Background operations that don't need synchronization
 */
void on_insert_self_cleanup(MONGO_ASYNC_RESULT *result) {
    printf("\n[Self-Cleanup Pattern] Insert callback\n");
    if (result->success) {
        printf("  ✓ Document inserted successfully\n");
        if (result->data) {
            printf("  Inserted ID: %s\n", result->data);
        }
    } else {
        printf("  ✗ Insert failed: %s\n", result->error_message);
    }
    
    /* Clean up handle within callback - no external tracking needed! */
    mongo_async_handle_destroy(result->handle);
    printf("  Handle destroyed in callback\n");
    
    self_cleanup_complete = 1;
}

/*
 * Pattern 3: Concurrent Operations with Self-Cleanup
 * - Multiple fire-and-forget operations
 * - No handle array to manage
 * - Each callback cleans up its own handle
 */
void on_concurrent_insert(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        printf("  ✓ Concurrent insert %d succeeded\n", concurrent_count + 1);
    } else {
        printf("  ✗ Concurrent insert %d failed: %s\n", 
               concurrent_count + 1, result->error_message);
    }
    
    /* Self-cleanup */
    mongo_async_handle_destroy(result->handle);
    
    concurrent_count++;
}

/*
 * Pattern 4: Streaming with Self-Cleanup
 * - For find/aggregate operations
 * - Process documents as they arrive
 * - Cleanup on end-of-stream signal
 */
void on_find_streaming(MONGO_ASYNC_RESULT *result) {
    if (result->success) {
        /* success=1 means document arrived */
        printf("  Document: %s\n", result->data);
    } else {
        /* success=0 means end of stream */
        printf("  End of stream - cleaning up handle\n");
        mongo_async_handle_destroy(result->handle);
    }
}

int main(void) {
    printf("=== MongoDB Async Handle Management Patterns Demo ===\n\n");
    
    /* Initialize MongoDB client */
    MONGO_CONFIG config = {
        .uri = "mongodb://localhost:27017",
        .database = "testdb"
    };
    
    MONGO_CLIENT *client = mongo_client_create(&config);
    if (!client) {
        printf("Failed to connect to MongoDB\n");
        return 1;
    }
    printf("✓ Connected to MongoDB\n\n");
    
    MONGO_COLLECTION *collection = mongo_client_get_collection(client, "testdb", "handle_demo");
    
    /* Clean up any existing documents */
    mongo_delete(collection, "{}", NULL);
    
    /* ========================================
     * Pattern 1: External Handle Management
     * ======================================== */
    printf("=== Pattern 1: External Handle Management ===\n");
    printf("Use case: When you need to wait for result synchronously\n");
    
    external_complete = 0;
    
    const char *doc1 = "{\"name\":\"Alice\",\"pattern\":\"external\"}";
    MONGO_ASYNC_HANDLE *handle1 = mongo_insert_one_async(
        collection, doc1, on_insert_external, NULL
    );
    
    printf("Handle created: %p\n", (void*)handle1);
    printf("Waiting for operation to complete...\n");
    
    /* Wait for callback to complete */
    while (!external_complete) {
        usleep(10000); /* 10ms */
    }
    
    /* Application is responsible for cleanup */
    mongo_async_handle_destroy(handle1);
    printf("Handle destroyed externally\n");
    
    /* ========================================
     * Pattern 2: Self-Cleanup in Callback
     * ======================================== */
    printf("\n=== Pattern 2: Self-Cleanup in Callback ===\n");
    printf("Use case: Fire-and-forget background operations\n");
    
    self_cleanup_complete = 0;
    
    const char *doc2 = "{\"name\":\"Bob\",\"pattern\":\"self-cleanup\"}";
    
    /* No need to store handle - callback will clean it up */
    mongo_insert_one_async(
        collection, doc2, on_insert_self_cleanup, NULL
    );
    
    printf("Operation started (no handle stored)\n");
    printf("Waiting for callback to complete...\n");
    
    /* Just wait for completion notification */
    while (!self_cleanup_complete) {
        usleep(10000);
    }
    
    printf("Operation complete - callback handled cleanup\n");
    
    /* ========================================
     * Pattern 3: Concurrent Operations
     * ======================================== */
    printf("\n=== Pattern 3: Concurrent Fire-and-Forget Operations ===\n");
    printf("Use case: Multiple background inserts without tracking\n");
    
    concurrent_count = 0;
    
    /* Launch 5 concurrent operations - no handle tracking! */
    for (int i = 0; i < 5; i++) {
        char doc[128];
        snprintf(doc, sizeof(doc), 
                 "{\"name\":\"User%d\",\"pattern\":\"concurrent\"}", i);
        
        mongo_insert_one_async(
            collection, doc, on_concurrent_insert, NULL
        );
    }
    
    printf("5 concurrent operations launched\n");
    printf("Waiting for all to complete...\n");
    
    /* Wait for all to complete */
    while (concurrent_count < 5) {
        usleep(10000);
    }
    
    printf("All concurrent operations complete\n");
    
    /* ========================================
     * Pattern 4: Streaming with Self-Cleanup
     * ======================================== */
    printf("\n=== Pattern 4: Streaming Query with Self-Cleanup ===\n");
    printf("Use case: Process documents as they arrive, cleanup at end\n");
    
    /* Query for all documents */
    MONGO_QUERY_OPTIONS options = {
        .sort = NULL,
        .skip = 0,
        .limit = 0
    };
    
    printf("Querying all documents...\n");
    mongo_find_async(collection, "{}", &options, on_find_streaming, NULL);
    
    /* Give time for streaming to complete */
    printf("Streaming in progress...\n");
    sleep(1);
    
    /* ========================================
     * Pattern Comparison Summary
     * ======================================== */
    printf("\n=== Pattern Comparison ===\n\n");
    
    printf("External Management:\n");
    printf("  Pros: Full control, explicit lifecycle\n");
    printf("  Cons: Must track handles, manual cleanup\n");
    printf("  Code: MONGO_ASYNC_HANDLE *h = mongo_...; wait(); destroy(h);\n\n");
    
    printf("Self-Cleanup:\n");
    printf("  Pros: Simple, no tracking, fire-and-forget\n");
    printf("  Cons: Less control over timing\n");
    printf("  Code: mongo_...(on_callback, NULL); // callback calls destroy(result->handle)\n\n");
    
    printf("Concurrent:\n");
    printf("  Pros: Easy scaling, no handle arrays\n");
    printf("  Cons: Harder to track completion\n");
    printf("  Code: for() { mongo_...(shared_callback, NULL); }\n\n");
    
    printf("Streaming:\n");
    printf("  Pros: Process data incrementally\n");
    printf("  Cons: Multiple callback invocations\n");
    printf("  Code: On success=1: process doc; On success=0: cleanup\n\n");
    
    /* Cleanup */
    mongo_collection_destroy(collection);
    mongo_client_destroy(client);
    
    printf("✓ Demo complete\n");
    return 0;
}
