#include "mongo_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Simple logging functions */
#define log_error(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define log_info(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
typedef enum { LOG_LEVEL_INFO } log_level_t;
static inline void logger_init(log_level_t level) { (void)level; }

#define BUFFER_SIZE 8192

void print_separator(void) {
    printf("\n%s\n", "========================================");
}

void demo_basic_crud(MONGO_CLIENT *client) {
    printf("\n=== Basic CRUD Operations ===\n");
    
    /* Get collection handle */
    MONGO_COLLECTION *users = mongo_client_get_collection(client, NULL, "users");
    if (!users) {
        log_error("Failed to get users collection");
        return;
    }
    
    /* Insert a document */
    printf("\n1. Inserting a document...\n");
    char inserted_id[64] = {0};
    const char *doc = "{\"name\":\"John Doe\",\"age\":30,\"email\":\"john@example.com\",\"city\":\"New York\"}";
    
    if (mongo_insert_one(users, doc, inserted_id, sizeof(inserted_id)) == 0) {
        printf("   ✓ Document inserted with ID: %s\n", inserted_id);
    } else {
        printf("   ✗ Failed to insert document\n");
    }
    
    /* Insert multiple documents */
    printf("\n2. Inserting multiple documents...\n");
    const char *docs[] = {
        "{\"name\":\"Jane Smith\",\"age\":28,\"email\":\"jane@example.com\",\"city\":\"Los Angeles\"}",
        "{\"name\":\"Bob Johnson\",\"age\":35,\"email\":\"bob@example.com\",\"city\":\"Chicago\"}",
        "{\"name\":\"Alice Williams\",\"age\":25,\"email\":\"alice@example.com\",\"city\":\"New York\"}"
    };
    
    int inserted = mongo_insert_many(users, docs, 3);
    if (inserted > 0) {
        printf("   ✓ Inserted %d documents\n", inserted);
    } else {
        printf("   ✗ Failed to insert documents\n");
    }
    
    /* Count documents */
    printf("\n3. Counting documents...\n");
    int64_t count = mongo_count(users, NULL);
    printf("   Total documents: %lld\n", (long long)count);
    
    /* Find one document */
    printf("\n4. Finding a document by name...\n");
    char result[BUFFER_SIZE];
    if (mongo_find_one(users, "{\"name\":\"Jane Smith\"}", result, sizeof(result)) == 0) {
        printf("   Found: %s\n", result);
    } else {
        printf("   ✗ Document not found\n");
    }
    
    /* Find all documents with age >= 30 */
    printf("\n5. Finding users with age >= 30...\n");
    MONGO_CURSOR *cursor = mongo_find(users, "{\"age\":{\"$gte\":30}}", NULL);
    if (cursor) {
        int i = 0;
        while (mongo_cursor_next(cursor, result, sizeof(result)) == 1) {
            printf("   [%d] %s\n", ++i, result);
        }
        mongo_cursor_destroy(cursor);
    }
    
    /* Update a document */
    printf("\n6. Updating a document...\n");
    if (mongo_update(users, "{\"name\":\"John Doe\"}", 
                    "{\"$set\":{\"age\":31,\"status\":\"updated\"}}", NULL) == 0) {
        printf("   ✓ Document updated\n");
        
        /* Verify update */
        if (mongo_find_one(users, "{\"name\":\"John Doe\"}", result, sizeof(result)) == 0) {
            printf("   Updated document: %s\n", result);
        }
    }
    
    /* Delete a document */
    printf("\n7. Deleting a document...\n");
    if (mongo_delete(users, "{\"name\":\"Bob Johnson\"}", NULL) == 0) {
        printf("   ✓ Document deleted\n");
        count = mongo_count(users, NULL);
        printf("   Remaining documents: %lld\n", (long long)count);
    }
    
    mongo_collection_destroy(users);
}

void demo_query_options(MONGO_CLIENT *client) {
    printf("\n=== Query Options (Sort, Limit, Skip) ===\n");
    
    MONGO_COLLECTION *users = mongo_client_get_collection(client, NULL, "users");
    if (!users) return;
    
    /* Sort by age descending, limit to 2 */
    printf("\nTop 2 users by age (descending):\n");
    MONGO_QUERY_OPTIONS opts = {
        .limit = 2,
        .skip = 0,
        .sort = "{\"age\":-1}",
        .projection = "{\"name\":1,\"age\":1,\"_id\":0}"
    };
    
    MONGO_CURSOR *cursor = mongo_find(users, "{}", &opts);
    if (cursor) {
        char result[BUFFER_SIZE];
        int i = 0;
        while (mongo_cursor_next(cursor, result, sizeof(result)) == 1) {
            printf("   [%d] %s\n", ++i, result);
        }
        mongo_cursor_destroy(cursor);
    }
    
    mongo_collection_destroy(users);
}

void demo_aggregation(MONGO_CLIENT *client) {
    printf("\n=== Aggregation Pipeline ===\n");
    
    MONGO_COLLECTION *users = mongo_client_get_collection(client, NULL, "users");
    if (!users) return;
    
    /* Group by city and count */
    printf("\nUsers grouped by city:\n");
    const char *pipeline = "["
        "{\"$group\":{"
            "\"_id\":\"$city\","
            "\"count\":{\"$sum\":1},"
            "\"avgAge\":{\"$avg\":\"$age\"}"
        "}},"
        "{\"$sort\":{\"count\":-1}}"
    "]";
    
    MONGO_CURSOR *cursor = mongo_aggregate(users, pipeline);
    if (cursor) {
        char result[BUFFER_SIZE];
        while (mongo_cursor_next(cursor, result, sizeof(result)) == 1) {
            printf("   %s\n", result);
        }
        mongo_cursor_destroy(cursor);
    }
    
    mongo_collection_destroy(users);
}

void demo_indexes(MONGO_CLIENT *client) {
    printf("\n=== Index Management ===\n");
    
    MONGO_COLLECTION *users = mongo_client_get_collection(client, NULL, "users");
    if (!users) return;
    
    /* Create a unique index on email */
    printf("\nCreating unique index on email field...\n");
    if (mongo_create_index(users, "{\"email\":1}", true) == 0) {
        printf("   ✓ Index created successfully\n");
    } else {
        printf("   ✗ Failed to create index (may already exist)\n");
    }
    
    /* Create a compound index */
    printf("\nCreating compound index on city and age...\n");
    if (mongo_create_index(users, "{\"city\":1,\"age\":-1}", false) == 0) {
        printf("   ✓ Compound index created\n");
    } else {
        printf("   ✗ Failed to create compound index\n");
    }
    
    mongo_collection_destroy(users);
}

void demo_database_operations(MONGO_CLIENT *client) {
    printf("\n=== Database Operations ===\n");
    
    /* List databases */
    printf("\nListing all databases:\n");
    char result[BUFFER_SIZE];
    if (mongo_list_databases(client, result, sizeof(result)) == 0) {
        printf("   %s\n", result);
    }
    
    /* List collections */
    printf("\nListing collections in 'testdb':\n");
    if (mongo_list_collections(client, "testdb", result, sizeof(result)) == 0) {
        printf("   %s\n", result);
    }
}

void cleanup_demo_data(MONGO_CLIENT *client) {
    printf("\n=== Cleanup ===\n");
    
    MONGO_COLLECTION *users = mongo_client_get_collection(client, NULL, "users");
    if (users) {
        printf("Dropping users collection...\n");
        mongo_drop_collection(users);
        mongo_collection_destroy(users);
        printf("   ✓ Collection dropped\n");
    }
}

int main(int argc, char *argv[]) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║          Hanuman Framework - MongoDB Demo                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* Initialize logger */
    logger_init(LOG_LEVEL_INFO);
    
    /* Get MongoDB URI from command line or use default */
    const char *mongo_uri = "mongodb://localhost:27017";
    if (argc > 1) {
        mongo_uri = argv[1];
    }
    
    printf("\nConnecting to MongoDB: %s\n", mongo_uri);
    
    /* Create MongoDB configuration */
    MONGO_CONFIG config;
    mongo_config_default(&config, mongo_uri, "testdb");
    
    /* Create MongoDB client */
    MONGO_CLIENT *client = mongo_client_create(&config);
    if (!client) {
        log_error("Failed to create MongoDB client");
        return 1;
    }
    
    /* Test connection */
    printf("Testing connection...\n");
    if (mongo_client_ping(client)) {
        printf("✓ Connected successfully!\n");
    } else {
        log_error("Failed to connect to MongoDB");
        mongo_client_destroy(client);
        return 1;
    }
    
    print_separator();
    
    /* Run demos */
    demo_basic_crud(client);
    print_separator();
    
    demo_query_options(client);
    print_separator();
    
    demo_aggregation(client);
    print_separator();
    
    demo_indexes(client);
    print_separator();
    
    demo_database_operations(client);
    print_separator();
    
    /* Cleanup */
    cleanup_demo_data(client);
    print_separator();
    
    /* Destroy client */
    mongo_client_destroy(client);
    
    printf("\n✓ Demo completed successfully!\n\n");
    printf("Usage: %s [mongodb://host:port]\n", argv[0]);
    printf("Example: %s mongodb://localhost:27017\n\n", argv[0]);
    
    return 0;
}
