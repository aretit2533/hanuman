#ifndef MONGO_CLIENT_H
#define MONGO_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct _mongo_client_ MONGO_CLIENT;
typedef struct _mongo_collection_ MONGO_COLLECTION;
typedef struct _mongo_cursor_ MONGO_CURSOR;
typedef struct _mongo_async_handle_ MONGO_ASYNC_HANDLE;

/**
 * MongoDB Async Operation Result
 */
typedef struct _mongo_async_result_ {
    int success;                    /* 1 if operation succeeded, 0 otherwise */
    int error_code;                 /* Error code if operation failed */
    char error_message[256];        /* Error message if operation failed */
    char *data;                     /* Result data (JSON string for queries, ID for inserts) */
    size_t data_size;               /* Size of result data */
    int64_t affected_count;         /* Number of documents affected (insert/update/delete) */
    void *user_data;                /* User data passed to callback */
    MONGO_ASYNC_HANDLE *handle;     /* Handle to the async operation (can be used to destroy) */
} MONGO_ASYNC_RESULT;

/**
 * Async operation callback function
 * Called when an async operation completes
 * @param result Result of the async operation
 */
typedef void (*mongo_async_callback)(MONGO_ASYNC_RESULT *result);

/**
 * MongoDB Connection Configuration
 */
typedef struct _mongo_config_ {
    char uri[512];                  /* MongoDB connection URI */
    char database[128];             /* Default database name */
    int connection_timeout_ms;      /* Connection timeout in milliseconds */
    int socket_timeout_ms;          /* Socket timeout in milliseconds */
    int max_pool_size;              /* Maximum connection pool size */
    int min_pool_size;              /* Minimum connection pool size */
    bool ssl_enabled;               /* Enable SSL/TLS */
    char ssl_ca_file[512];          /* CA certificate file path */
    char ssl_cert_file[512];        /* Client certificate file path */
    char ssl_key_file[512];         /* Client private key file path */
} MONGO_CONFIG;

/**
 * MongoDB Query Options
 */
typedef struct _mongo_query_options_ {
    int limit;                      /* Maximum number of documents to return */
    int skip;                       /* Number of documents to skip */
    const char *sort;               /* Sort specification (BSON JSON string) */
    const char *projection;         /* Field projection (BSON JSON string) */
} MONGO_QUERY_OPTIONS;

/**
 * MongoDB Update Options
 */
typedef struct _mongo_update_options_ {
    bool upsert;                    /* Insert if document doesn't exist */
    bool multi;                     /* Update multiple documents */
} MONGO_UPDATE_OPTIONS;

/**
 * MongoDB Delete Options
 */
typedef struct _mongo_delete_options_ {
    bool multi;                     /* Delete multiple documents */
} MONGO_DELETE_OPTIONS;

/* ============================================================================
 * Client Management
 * ========================================================================== */

/**
 * Create a default MongoDB configuration
 * @param config Configuration structure to initialize
 * @param uri MongoDB connection URI (e.g., "mongodb://localhost:27017")
 * @param database Default database name
 */
void mongo_config_default(MONGO_CONFIG *config, const char *uri, const char *database);

/**
 * Create a new MongoDB client
 * @param config MongoDB configuration
 * @return Pointer to created MongoDB client, or NULL on failure
 */
MONGO_CLIENT* mongo_client_create(MONGO_CONFIG *config);

/**
 * Destroy a MongoDB client and free resources
 * @param client The MongoDB client to destroy
 */
void mongo_client_destroy(MONGO_CLIENT *client);

/**
 * Test the connection to the MongoDB server
 * @param client The MongoDB client
 * @return 1 if connected successfully, 0 otherwise
 */
int mongo_client_ping(MONGO_CLIENT *client);

/**
 * Get a collection handle
 * @param client The MongoDB client
 * @param database Database name (NULL to use default from config)
 * @param collection Collection name
 * @return Pointer to collection handle, or NULL on failure
 */
MONGO_COLLECTION* mongo_client_get_collection(MONGO_CLIENT *client, 
                                              const char *database,
                                              const char *collection);

/**
 * Release a collection handle
 * @param collection The collection to release
 */
void mongo_collection_destroy(MONGO_COLLECTION *collection);

/* ============================================================================
 * Document Operations (CRUD)
 * ========================================================================== */

/**
 * Insert a single document
 * @param collection The collection
 * @param document JSON string representing the document
 * @param inserted_id Output buffer for inserted document ID (optional, can be NULL)
 * @param id_size Size of the inserted_id buffer
 * @return 0 on success, error code on failure
 * 
 * Example:
 *   mongo_insert_one(coll, "{\"name\":\"John\",\"age\":30}", id_buf, sizeof(id_buf));
 */
int mongo_insert_one(MONGO_COLLECTION *collection, const char *document,
                     char *inserted_id, size_t id_size);

/**
 * Insert multiple documents
 * @param collection The collection
 * @param documents Array of JSON strings representing documents
 * @param count Number of documents to insert
 * @return Number of documents inserted, or -1 on failure
 * 
 * Example:
 *   const char *docs[] = {"{\"name\":\"John\"}", "{\"name\":\"Jane\"}"};
 *   mongo_insert_many(coll, docs, 2);
 */
int mongo_insert_many(MONGO_COLLECTION *collection, const char **documents, size_t count);

/**
 * Find documents matching a query
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param options Query options (can be NULL for defaults)
 * @return Cursor to iterate results, or NULL on failure
 * 
 * Example:
 *   MONGO_CURSOR *cursor = mongo_find(coll, "{\"age\":{\"$gte\":18}}", NULL);
 */
MONGO_CURSOR* mongo_find(MONGO_COLLECTION *collection, const char *query,
                         MONGO_QUERY_OPTIONS *options);

/**
 * Find a single document
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param result Output buffer for the result document (JSON string)
 * @param result_size Size of the result buffer
 * @return 0 on success, error code on failure
 * 
 * Example:
 *   char result[4096];
 *   mongo_find_one(coll, "{\"_id\":\"123\"}", result, sizeof(result));
 */
int mongo_find_one(MONGO_COLLECTION *collection, const char *query,
                   char *result, size_t result_size);

/**
 * Update documents matching a query
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param update JSON string representing the update operations
 * @param options Update options (can be NULL for defaults)
 * @return Number of documents modified, or -1 on failure
 * 
 * Example:
 *   mongo_update(coll, "{\"name\":\"John\"}", "{\"$set\":{\"age\":31}}", NULL);
 */
int mongo_update(MONGO_COLLECTION *collection, const char *query,
                 const char *update, MONGO_UPDATE_OPTIONS *options);

/**
 * Replace a single document
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param replacement JSON string representing the replacement document
 * @param upsert If true, insert if document doesn't exist
 * @return Number of documents modified, or -1 on failure
 */
int mongo_replace_one(MONGO_COLLECTION *collection, const char *query,
                      const char *replacement, bool upsert);

/**
 * Delete documents matching a query
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param options Delete options (can be NULL for defaults)
 * @return Number of documents deleted, or -1 on failure
 * 
 * Example:
 *   mongo_delete(coll, "{\"age\":{\"$lt\":18}}", NULL);
 */
int mongo_delete(MONGO_COLLECTION *collection, const char *query,
                 MONGO_DELETE_OPTIONS *options);

/**
 * Count documents matching a query
 * @param collection The collection
 * @param query JSON string representing the query filter (NULL or "{}" for all)
 * @return Number of documents, or -1 on failure
 */
int64_t mongo_count(MONGO_COLLECTION *collection, const char *query);

/* ============================================================================
 * Cursor Operations
 * ========================================================================== */

/**
 * Get the next document from a cursor
 * @param cursor The cursor
 * @param result Output buffer for the document (JSON string)
 * @param result_size Size of the result buffer
 * @return 1 if document retrieved, 0 if no more documents, -1 on error
 */
int mongo_cursor_next(MONGO_CURSOR *cursor, char *result, size_t result_size);

/**
 * Check if cursor has more documents
 * @param cursor The cursor
 * @return 1 if more documents available, 0 otherwise
 */
int mongo_cursor_has_next(MONGO_CURSOR *cursor);

/**
 * Destroy a cursor and free resources
 * @param cursor The cursor to destroy
 */
void mongo_cursor_destroy(MONGO_CURSOR *cursor);

/* ============================================================================
 * Aggregation Pipeline
 * ========================================================================== */

/**
 * Execute an aggregation pipeline
 * @param collection The collection
 * @param pipeline JSON array string representing the pipeline stages
 * @return Cursor to iterate results, or NULL on failure
 * 
 * Example:
 *   const char *pipeline = "[{\"$match\":{\"age\":{\"$gte\":18}}},{\"$group\":{\"_id\":\"$city\",\"count\":{\"$sum\":1}}}]";
 *   MONGO_CURSOR *cursor = mongo_aggregate(coll, pipeline);
 */
MONGO_CURSOR* mongo_aggregate(MONGO_COLLECTION *collection, const char *pipeline);

/* ============================================================================
 * Index Management
 * ========================================================================== */

/**
 * Create an index on a collection
 * @param collection The collection
 * @param keys JSON string representing index keys and direction
 * @param unique If true, create a unique index
 * @return 0 on success, error code on failure
 * 
 * Example:
 *   mongo_create_index(coll, "{\"email\":1}", true);
 */
int mongo_create_index(MONGO_COLLECTION *collection, const char *keys, bool unique);

/**
 * Drop an index from a collection
 * @param collection The collection
 * @param index_name Name of the index to drop
 * @return 0 on success, error code on failure
 */
int mongo_drop_index(MONGO_COLLECTION *collection, const char *index_name);

/* ============================================================================
 * Bulk Operations
 * ========================================================================== */

/**
 * Execute a bulk write operation
 * @param collection The collection
 * @param operations JSON array string representing bulk operations
 * @return Number of operations executed, or -1 on failure
 * 
 * Example:
 *   const char *ops = "[{\"insertOne\":{\"document\":{\"name\":\"John\"}}},{\"updateOne\":{\"filter\":{\"_id\":1},\"update\":{\"$set\":{\"age\":30}}}}]";
 *   mongo_bulk_write(coll, ops);
 */
int mongo_bulk_write(MONGO_COLLECTION *collection, const char *operations);

/* ============================================================================
 * Database Operations
 * ========================================================================== */

/**
 * List all databases
 * @param client The MongoDB client
 * @param result Output buffer for database list (JSON array string)
 * @param result_size Size of the result buffer
 * @return 0 on success, error code on failure
 */
int mongo_list_databases(MONGO_CLIENT *client, char *result, size_t result_size);

/**
 * List all collections in a database
 * @param client The MongoDB client
 * @param database Database name
 * @param result Output buffer for collection list (JSON array string)
 * @param result_size Size of the result buffer
 * @return 0 on success, error code on failure
 */
int mongo_list_collections(MONGO_CLIENT *client, const char *database,
                           char *result, size_t result_size);

/**
 * Drop a database
 * @param client The MongoDB client
 * @param database Database name
 * @return 0 on success, error code on failure
 */
int mongo_drop_database(MONGO_CLIENT *client, const char *database);

/**
 * Drop a collection
 * @param collection The collection to drop
 * @return 0 on success, error code on failure
 */
int mongo_drop_collection(MONGO_COLLECTION *collection);

/* ============================================================================
 * Asynchronous Operations
 * ========================================================================== */

/**
 * Insert a document asynchronously
 * @param collection The collection
 * @param document JSON string representing the document
 * @param callback Callback function to call when operation completes
 * @param user_data User data to pass to callback
 * @return Async handle for tracking operation, or NULL on failure
 * 
 * Example:
 *   void on_insert(MONGO_ASYNC_RESULT *result) {
 *       if (result->success) printf("Inserted: %s\n", result->data);
 *   }
 *   mongo_insert_one_async(coll, "{\"name\":\"John\"}", on_insert, NULL);
 */
MONGO_ASYNC_HANDLE* mongo_insert_one_async(MONGO_COLLECTION *collection,
                                           const char *document,
                                           mongo_async_callback callback,
                                           void *user_data);

/**
 * Find documents asynchronously
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param options Query options (can be NULL for defaults)
 * @param callback Callback function to call for each document found
 * @param user_data User data to pass to callback
 * @return Async handle for tracking operation, or NULL on failure
 * 
 * Note: Callback will be called once per document found, then once more with success=0 to signal completion
 */
MONGO_ASYNC_HANDLE* mongo_find_async(MONGO_COLLECTION *collection,
                                     const char *query,
                                     MONGO_QUERY_OPTIONS *options,
                                     mongo_async_callback callback,
                                     void *user_data);

/**
 * Find a single document asynchronously
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param callback Callback function to call when document found
 * @param user_data User data to pass to callback
 * @return Async handle for tracking operation, or NULL on failure
 */
MONGO_ASYNC_HANDLE* mongo_find_one_async(MONGO_COLLECTION *collection,
                                         const char *query,
                                         mongo_async_callback callback,
                                         void *user_data);

/**
 * Update documents asynchronously
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param update JSON string representing the update operations
 * @param options Update options (can be NULL for defaults)
 * @param callback Callback function to call when operation completes
 * @param user_data User data to pass to callback
 * @return Async handle for tracking operation, or NULL on failure
 */
MONGO_ASYNC_HANDLE* mongo_update_async(MONGO_COLLECTION *collection,
                                       const char *query,
                                       const char *update,
                                       MONGO_UPDATE_OPTIONS *options,
                                       mongo_async_callback callback,
                                       void *user_data);

/**
 * Delete documents asynchronously
 * @param collection The collection
 * @param query JSON string representing the query filter
 * @param options Delete options (can be NULL for defaults)
 * @param callback Callback function to call when operation completes
 * @param user_data User data to pass to callback
 * @return Async handle for tracking operation, or NULL on failure
 */
MONGO_ASYNC_HANDLE* mongo_delete_async(MONGO_COLLECTION *collection,
                                       const char *query,
                                       MONGO_DELETE_OPTIONS *options,
                                       mongo_async_callback callback,
                                       void *user_data);

/**
 * Count documents asynchronously
 * @param collection The collection
 * @param query JSON string representing the query filter (NULL or "{}" for all)
 * @param callback Callback function to call when operation completes
 * @param user_data User data to pass to callback
 * @return Async handle for tracking operation, or NULL on failure
 */
MONGO_ASYNC_HANDLE* mongo_count_async(MONGO_COLLECTION *collection,
                                      const char *query,
                                      mongo_async_callback callback,
                                      void *user_data);

/**
 * Execute aggregation pipeline asynchronously
 * @param collection The collection
 * @param pipeline JSON array string representing the pipeline stages
 * @param callback Callback function to call for each result document
 * @param user_data User data to pass to callback
 * @return Async handle for tracking operation, or NULL on failure
 */
MONGO_ASYNC_HANDLE* mongo_aggregate_async(MONGO_COLLECTION *collection,
                                          const char *pipeline,
                                          mongo_async_callback callback,
                                          void *user_data);

/**
 * Wait for an async operation to complete
 * @param handle The async handle
 * @return 1 if operation completed successfully, 0 otherwise
 */
int mongo_async_wait(MONGO_ASYNC_HANDLE *handle);

/**
 * Check if an async operation is complete
 * @param handle The async handle
 * @return 1 if complete, 0 if still running
 */
int mongo_async_is_complete(MONGO_ASYNC_HANDLE *handle);

/**
 * Cancel an async operation (best effort)
 * @param handle The async handle
 * @return 0 on success, error code on failure
 */
int mongo_async_cancel(MONGO_ASYNC_HANDLE *handle);

/**
 * Destroy an async handle and free resources
 * @param handle The async handle to destroy
 */
void mongo_async_handle_destroy(MONGO_ASYNC_HANDLE *handle);

#endif /* MONGO_CLIENT_H */
