#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 600
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "mongo_client.h"
#include <mongoc/mongoc.h>

/* Explicit function declarations for POSIX functions */
extern int usleep(unsigned int usec);

/* Simple logging macros */
#define log_error(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define log_info(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define log_debug(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)

/* Internal MongoDB client structure */
struct _mongo_client_ {
    mongoc_client_t *client;
    mongoc_uri_t *uri;
    char default_database[128];
};

/* Internal collection structure */
struct _mongo_collection_ {
    mongoc_collection_t *collection;
    pthread_mutex_t mutex;  /* Protect concurrent access from async operations */
};

/* Internal cursor structure */
struct _mongo_cursor_ {
    mongoc_cursor_t *cursor;
};

/* Async operation types */
typedef enum {
    MONGO_ASYNC_INSERT_ONE,
    MONGO_ASYNC_FIND,
    MONGO_ASYNC_FIND_ONE,
    MONGO_ASYNC_UPDATE,
    MONGO_ASYNC_DELETE,
    MONGO_ASYNC_COUNT,
    MONGO_ASYNC_AGGREGATE
} mongo_async_op_type_t;

/* Internal async handle structure */
struct _mongo_async_handle_ {
    pthread_t thread;
    mongo_async_op_type_t op_type;
    MONGO_COLLECTION *collection;
    char *query;
    char *document;
    char *update;
    char *pipeline;
    MONGO_QUERY_OPTIONS *query_opts;
    MONGO_UPDATE_OPTIONS *update_opts;
    MONGO_DELETE_OPTIONS *delete_opts;
    mongo_async_callback callback;
    void *user_data;
    int complete;
    int cancelled;
    pthread_mutex_t mutex;
    MONGO_ASYNC_RESULT result;
};

/* Global initialization flag */
static int mongo_initialized = 0;

/* Initialize libmongoc (called automatically) */
static void mongo_init(void) {
    if (!mongo_initialized) {
        mongoc_init();
        mongo_initialized = 1;
        log_info("MongoDB client library initialized");
    }
}

/* ============================================================================
 * Configuration and Client Management
 * ========================================================================== */

void mongo_config_default(MONGO_CONFIG *config, const char *uri, const char *database) {
    if (!config) return;
    
    memset(config, 0, sizeof(MONGO_CONFIG));
    
    if (uri) {
        strncpy(config->uri, uri, sizeof(config->uri) - 1);
    } else {
        strncpy(config->uri, "mongodb://localhost:27017", sizeof(config->uri) - 1);
    }
    
    if (database) {
        strncpy(config->database, database, sizeof(config->database) - 1);
    }
    
    config->connection_timeout_ms = 10000;  /* 10 seconds */
    config->socket_timeout_ms = 30000;      /* 30 seconds */
    config->max_pool_size = 100;
    config->min_pool_size = 0;
    config->ssl_enabled = false;
}

MONGO_CLIENT* mongo_client_create(MONGO_CONFIG *config) {
    if (!config) {
        log_error("MongoDB config is NULL");
        return NULL;
    }
    
    mongo_init();
    
    MONGO_CLIENT *client = (MONGO_CLIENT*)malloc(sizeof(MONGO_CLIENT));
    if (!client) {
        log_error("Failed to allocate MongoDB client");
        return NULL;
    }
    
    memset(client, 0, sizeof(MONGO_CLIENT));
    
    /* Parse URI */
    bson_error_t error;
    client->uri = mongoc_uri_new_with_error(config->uri, &error);
    if (!client->uri) {
        log_error("Failed to parse MongoDB URI: %s", error.message);
        free(client);
        return NULL;
    }
    
    /* Set connection options */
    mongoc_uri_set_option_as_int32(client->uri, MONGOC_URI_CONNECTTIMEOUTMS, 
                                   config->connection_timeout_ms);
    mongoc_uri_set_option_as_int32(client->uri, MONGOC_URI_SOCKETTIMEOUTMS,
                                   config->socket_timeout_ms);
    mongoc_uri_set_option_as_int32(client->uri, MONGOC_URI_MAXPOOLSIZE,
                                   config->max_pool_size);
    mongoc_uri_set_option_as_int32(client->uri, MONGOC_URI_MINPOOLSIZE,
                                   config->min_pool_size);
    
    if (config->ssl_enabled) {
        mongoc_uri_set_option_as_bool(client->uri, MONGOC_URI_TLS, true);
        if (config->ssl_ca_file[0]) {
            mongoc_uri_set_option_as_utf8(client->uri, MONGOC_URI_TLSCAFILE, 
                                         config->ssl_ca_file);
        }
        if (config->ssl_cert_file[0]) {
            mongoc_uri_set_option_as_utf8(client->uri, MONGOC_URI_TLSCERTIFICATEKEYFILE,
                                         config->ssl_cert_file);
        }
    }
    
    /* Create client */
    client->client = mongoc_client_new_from_uri(client->uri);
    if (!client->client) {
        log_error("Failed to create MongoDB client");
        mongoc_uri_destroy(client->uri);
        free(client);
        return NULL;
    }
    
    /* Store default database */
    strncpy(client->default_database, config->database, 
            sizeof(client->default_database) - 1);
    
    log_info("MongoDB client created: %s", config->uri);
    return client;
}

void mongo_client_destroy(MONGO_CLIENT *client) {
    if (!client) return;
    
    if (client->client) {
        mongoc_client_destroy(client->client);
    }
    if (client->uri) {
        mongoc_uri_destroy(client->uri);
    }
    
    free(client);
    log_debug("MongoDB client destroyed");
}

int mongo_client_ping(MONGO_CLIENT *client) {
    if (!client || !client->client) return 0;
    
    bson_t *command = BCON_NEW("ping", BCON_INT32(1));
    bson_t reply;
    bson_error_t error;
    
    bool success = mongoc_client_command_simple(client->client, "admin",
                                               command, NULL, &reply, &error);
    
    bson_destroy(command);
    bson_destroy(&reply);
    
    if (!success) {
        log_error("MongoDB ping failed: %s", error.message);
        return 0;
    }
    
    log_debug("MongoDB ping successful");
    return 1;
}

MONGO_COLLECTION* mongo_client_get_collection(MONGO_CLIENT *client,
                                              const char *database,
                                              const char *collection) {
    if (!client || !client->client || !collection) {
        log_error("Invalid parameters for get_collection");
        return NULL;
    }
    
    const char *db = database ? database : client->default_database;
    if (!db || !db[0]) {
        log_error("No database specified");
        return NULL;
    }
    
    MONGO_COLLECTION *coll = (MONGO_COLLECTION*)malloc(sizeof(MONGO_COLLECTION));
    if (!coll) {
        log_error("Failed to allocate collection structure");
        return NULL;
    }
    
    coll->collection = mongoc_client_get_collection(client->client, db, collection);
    if (!coll->collection) {
        log_error("Failed to get collection: %s.%s", db, collection);
        free(coll);
        return NULL;
    }
    
    pthread_mutex_init(&coll->mutex, NULL);
    log_debug("Got collection: %s.%s", db, collection);
    return coll;
}

void mongo_collection_destroy(MONGO_COLLECTION *collection) {
    if (!collection) return;
    
    if (collection->collection) {
        mongoc_collection_destroy(collection->collection);
    }
    pthread_mutex_destroy(&collection->mutex);
    free(collection);
}

/* ============================================================================
 * Document Operations (CRUD)
 * ========================================================================== */

int mongo_insert_one(MONGO_COLLECTION *collection, const char *document,
                     char *inserted_id, size_t id_size) {
    if (!collection || !collection->collection || !document) {
        log_error("Invalid parameters for insert_one");
        return -1;
    }
    
    bson_error_t error;
    bson_t *doc = bson_new_from_json((const uint8_t*)document, -1, &error);
    if (!doc) {
        log_error("Failed to parse JSON document: %s", error.message);
        return -1;
    }
    
    bool success = mongoc_collection_insert_one(collection->collection, doc,
                                               NULL, NULL, &error);
    
    if (success && inserted_id && id_size > 0) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, doc, "_id")) {
            const bson_value_t *oid_value = bson_iter_value(&iter);
            if (oid_value && oid_value->value_type == BSON_TYPE_OID) {
                bson_oid_to_string(&oid_value->value.v_oid, inserted_id);
            }
        }
    }
    
    bson_destroy(doc);
    
    if (!success) {
        log_error("Insert failed: %s", error.message);
        return -1;
    }
    
    log_debug("Document inserted successfully");
    return 0;
}

int mongo_insert_many(MONGO_COLLECTION *collection, const char **documents, size_t count) {
    if (!collection || !collection->collection || !documents || count == 0) {
        log_error("Invalid parameters for insert_many");
        return -1;
    }
    
    bson_t **docs = (bson_t**)malloc(count * sizeof(bson_t*));
    if (!docs) {
        log_error("Failed to allocate document array");
        return -1;
    }
    
    bson_error_t error;
    size_t i;
    
    /* Parse all documents */
    for (i = 0; i < count; i++) {
        docs[i] = bson_new_from_json((const uint8_t*)documents[i], -1, &error);
        if (!docs[i]) {
            log_error("Failed to parse document %zu: %s", i, error.message);
            /* Free previously parsed docs */
            for (size_t j = 0; j < i; j++) {
                bson_destroy(docs[j]);
            }
            free(docs);
            return -1;
        }
    }
    
    bool success = mongoc_collection_insert_many(collection->collection,
                                                (const bson_t**)docs, count,
                                                NULL, NULL, &error);
    
    /* Free all documents */
    for (i = 0; i < count; i++) {
        bson_destroy(docs[i]);
    }
    free(docs);
    
    if (!success) {
        log_error("Insert many failed: %s", error.message);
        return -1;
    }
    
    log_debug("Inserted %zu documents successfully", count);
    return (int)count;
}

MONGO_CURSOR* mongo_find(MONGO_COLLECTION *collection, const char *query,
                         MONGO_QUERY_OPTIONS *options) {
    if (!collection || !collection->collection) {
        log_error("Invalid collection for find");
        return NULL;
    }
    
    bson_error_t error;
    bson_t *filter = query ? 
                     bson_new_from_json((const uint8_t*)query, -1, &error) :
                     bson_new();
    
    if (!filter) {
        log_error("Failed to parse query: %s", error.message);
        return NULL;
    }
    
    bson_t *opts = bson_new();
    if (options) {
        if (options->limit > 0) {
            BSON_APPEND_INT32(opts, "limit", options->limit);
        }
        if (options->skip > 0) {
            BSON_APPEND_INT32(opts, "skip", options->skip);
        }
        if (options->sort) {
            bson_t *sort = bson_new_from_json((const uint8_t*)options->sort, -1, &error);
            if (sort) {
                BSON_APPEND_DOCUMENT(opts, "sort", sort);
                bson_destroy(sort);
            }
        }
        if (options->projection) {
            bson_t *proj = bson_new_from_json((const uint8_t*)options->projection, -1, &error);
            if (proj) {
                BSON_APPEND_DOCUMENT(opts, "projection", proj);
                bson_destroy(proj);
            }
        }
    }
    
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection->collection, filter, opts, NULL);
    
    bson_destroy(filter);
    bson_destroy(opts);
    
    if (!cursor) {
        log_error("Failed to create cursor");
        return NULL;
    }
    
    MONGO_CURSOR *mongo_cursor = (MONGO_CURSOR*)malloc(sizeof(MONGO_CURSOR));
    if (!mongo_cursor) {
        mongoc_cursor_destroy(cursor);
        return NULL;
    }
    
    mongo_cursor->cursor = cursor;
    return mongo_cursor;
}

int mongo_find_one(MONGO_COLLECTION *collection, const char *query,
                   char *result, size_t result_size) {
    if (!collection || !result || result_size == 0) {
        log_error("Invalid parameters for find_one");
        return -1;
    }
    
    MONGO_QUERY_OPTIONS opts = {.limit = 1, .skip = 0, .sort = NULL, .projection = NULL};
    MONGO_CURSOR *cursor = mongo_find(collection, query, &opts);
    
    if (!cursor) {
        return -1;
    }
    
    int ret = mongo_cursor_next(cursor, result, result_size);
    mongo_cursor_destroy(cursor);
    
    return (ret == 1) ? 0 : -1;
}

int mongo_update(MONGO_COLLECTION *collection, const char *query,
                 const char *update, MONGO_UPDATE_OPTIONS *options) {
    if (!collection || !collection->collection || !query || !update) {
        log_error("Invalid parameters for update");
        return -1;
    }
    
    bson_error_t error;
    bson_t *selector = bson_new_from_json((const uint8_t*)query, -1, &error);
    if (!selector) {
        log_error("Failed to parse query: %s", error.message);
        return -1;
    }
    
    bson_t *update_doc = bson_new_from_json((const uint8_t*)update, -1, &error);
    if (!update_doc) {
        log_error("Failed to parse update: %s", error.message);
        bson_destroy(selector);
        return -1;
    }
    
    bson_t *opts = bson_new();
    if (options) {
        if (options->upsert) {
            BSON_APPEND_BOOL(opts, "upsert", true);
        }
    }
    
    bool success;
    if (options && options->multi) {
        success = mongoc_collection_update_many(collection->collection,
                                               selector, update_doc, opts,
                                               NULL, &error);
    } else {
        success = mongoc_collection_update_one(collection->collection,
                                              selector, update_doc, opts,
                                              NULL, &error);
    }
    
    bson_destroy(selector);
    bson_destroy(update_doc);
    bson_destroy(opts);
    
    if (!success) {
        log_error("Update failed: %s", error.message);
        return -1;
    }
    
    return 0;
}

int mongo_replace_one(MONGO_COLLECTION *collection, const char *query,
                      const char *replacement, bool upsert) {
    if (!collection || !collection->collection || !query || !replacement) {
        log_error("Invalid parameters for replace_one");
        return -1;
    }
    
    bson_error_t error;
    bson_t *selector = bson_new_from_json((const uint8_t*)query, -1, &error);
    if (!selector) {
        log_error("Failed to parse query: %s", error.message);
        return -1;
    }
    
    bson_t *replace_doc = bson_new_from_json((const uint8_t*)replacement, -1, &error);
    if (!replace_doc) {
        log_error("Failed to parse replacement: %s", error.message);
        bson_destroy(selector);
        return -1;
    }
    
    bson_t *opts = bson_new();
    if (upsert) {
        BSON_APPEND_BOOL(opts, "upsert", true);
    }
    
    bool success = mongoc_collection_replace_one(collection->collection,
                                                selector, replace_doc, opts,
                                                NULL, &error);
    
    bson_destroy(selector);
    bson_destroy(replace_doc);
    bson_destroy(opts);
    
    if (!success) {
        log_error("Replace failed: %s", error.message);
        return -1;
    }
    
    return 0;
}

int mongo_delete(MONGO_COLLECTION *collection, const char *query,
                 MONGO_DELETE_OPTIONS *options) {
    if (!collection || !collection->collection || !query) {
        log_error("Invalid parameters for delete");
        return -1;
    }
    
    bson_error_t error;
    bson_t *selector = bson_new_from_json((const uint8_t*)query, -1, &error);
    if (!selector) {
        log_error("Failed to parse query: %s", error.message);
        return -1;
    }
    
    bool success;
    if (options && options->multi) {
        success = mongoc_collection_delete_many(collection->collection,
                                               selector, NULL, NULL, &error);
    } else {
        success = mongoc_collection_delete_one(collection->collection,
                                              selector, NULL, NULL, &error);
    }
    
    bson_destroy(selector);
    
    if (!success) {
        log_error("Delete failed: %s", error.message);
        return -1;
    }
    
    return 0;
}

int64_t mongo_count(MONGO_COLLECTION *collection, const char *query) {
    if (!collection || !collection->collection) {
        log_error("Invalid collection for count");
        return -1;
    }
    
    bson_error_t error;
    bson_t *filter = query && query[0] ?
                     bson_new_from_json((const uint8_t*)query, -1, &error) :
                     bson_new();
    
    if (!filter) {
        log_error("Failed to parse query: %s", error.message);
        return -1;
    }
    
    int64_t count = mongoc_collection_count_documents(collection->collection,
                                                     filter, NULL, NULL,
                                                     NULL, &error);
    
    bson_destroy(filter);
    
    if (count < 0) {
        log_error("Count failed: %s", error.message);
    }
    
    return count;
}

/* ============================================================================
 * Cursor Operations
 * ========================================================================== */

int mongo_cursor_next(MONGO_CURSOR *cursor, char *result, size_t result_size) {
    if (!cursor || !cursor->cursor || !result || result_size == 0) {
        return -1;
    }
    
    const bson_t *doc;
    if (mongoc_cursor_next(cursor->cursor, &doc)) {
        char *json = bson_as_canonical_extended_json(doc, NULL);
        if (json) {
            strncpy(result, json, result_size - 1);
            result[result_size - 1] = '\0';
            bson_free(json);
            return 1;
        }
        return -1;
    }
    
    bson_error_t error;
    if (mongoc_cursor_error(cursor->cursor, &error)) {
        log_error("Cursor error: %s", error.message);
        return -1;
    }
    
    return 0;  /* No more documents */
}

int mongo_cursor_has_next(MONGO_CURSOR *cursor) {
    if (!cursor || !cursor->cursor) {
        return 0;
    }
    return mongoc_cursor_more(cursor->cursor);
}

void mongo_cursor_destroy(MONGO_CURSOR *cursor) {
    if (!cursor) return;
    
    if (cursor->cursor) {
        mongoc_cursor_destroy(cursor->cursor);
    }
    free(cursor);
}

/* ============================================================================
 * Aggregation Pipeline
 * ========================================================================== */

MONGO_CURSOR* mongo_aggregate(MONGO_COLLECTION *collection, const char *pipeline) {
    if (!collection || !collection->collection || !pipeline) {
        log_error("Invalid parameters for aggregate");
        return NULL;
    }
    
    bson_error_t error;
    bson_t *pipeline_doc = bson_new_from_json((const uint8_t*)pipeline, -1, &error);
    if (!pipeline_doc) {
        log_error("Failed to parse pipeline: %s", error.message);
        return NULL;
    }
    
    mongoc_cursor_t *cursor = mongoc_collection_aggregate(
        collection->collection, MONGOC_QUERY_NONE, pipeline_doc, NULL, NULL);
    
    bson_destroy(pipeline_doc);
    
    if (!cursor) {
        log_error("Failed to create aggregation cursor");
        return NULL;
    }
    
    MONGO_CURSOR *mongo_cursor = (MONGO_CURSOR*)malloc(sizeof(MONGO_CURSOR));
    if (!mongo_cursor) {
        mongoc_cursor_destroy(cursor);
        return NULL;
    }
    
    mongo_cursor->cursor = cursor;
    return mongo_cursor;
}

/* ============================================================================
 * Index Management
 * ========================================================================== */

int mongo_create_index(MONGO_COLLECTION *collection, const char *keys, bool unique) {
    if (!collection || !collection->collection || !keys) {
        log_error("Invalid parameters for create_index");
        return -1;
    }
    
    bson_error_t error;
    bson_t *keys_doc = bson_new_from_json((const uint8_t*)keys, -1, &error);
    if (!keys_doc) {
        log_error("Failed to parse keys: %s", error.message);
        return -1;
    }
    
    bson_t *opts = bson_new();
    if (unique) {
        BSON_APPEND_BOOL(opts, "unique", true);
    }
    
    bson_t *cmd = BCON_NEW(
        "createIndexes", BCON_UTF8(mongoc_collection_get_name(collection->collection)),
        "indexes", "[",
            "{",
                "key", BCON_DOCUMENT(keys_doc),
                "unique", BCON_BOOL(unique),
            "}",
        "]"
    );
    
    bson_t reply;
    bool success = mongoc_collection_write_command_with_opts(
        collection->collection, cmd, NULL, &reply, &error);
    
    bson_destroy(cmd);
    bson_destroy(keys_doc);
    bson_destroy(opts);
    bson_destroy(&reply);
    
    if (!success) {
        log_error("Create index failed: %s", error.message);
        return -1;
    }
    
    return 0;
}

int mongo_drop_index(MONGO_COLLECTION *collection, const char *index_name) {
    if (!collection || !collection->collection || !index_name) {
        log_error("Invalid parameters for drop_index");
        return -1;
    }
    
    bson_error_t error;
    bool success = mongoc_collection_drop_index(collection->collection,
                                               index_name, &error);
    
    if (!success) {
        log_error("Drop index failed: %s", error.message);
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * Bulk Operations
 * ========================================================================== */

int mongo_bulk_write(MONGO_COLLECTION *collection, const char *operations) {
    if (!collection || !collection->collection || !operations) {
        log_error("Invalid parameters for bulk_write");
        return -1;
    }
    
    /* This is a simplified implementation */
    /* Full implementation would parse the operations array and execute each */
    log_error("Bulk write not fully implemented yet");
    return -1;
}

/* ============================================================================
 * Database Operations
 * ========================================================================== */

int mongo_list_databases(MONGO_CLIENT *client, char *result, size_t result_size) {
    if (!client || !client->client || !result || result_size == 0) {
        log_error("Invalid parameters for list_databases");
        return -1;
    }
    
    bson_error_t error;
    char **names = mongoc_client_get_database_names_with_opts(client->client,
                                                              NULL, &error);
    if (!names) {
        log_error("Failed to list databases: %s", error.message);
        return -1;
    }
    
    result[0] = '[';
    result[1] = '\0';
    size_t pos = 1;
    
    for (int i = 0; names[i]; i++) {
        if (i > 0) {
            strncat(result + pos, ",", result_size - pos - 1);
            pos += 1;
        }
        strncat(result + pos, "\"", result_size - pos - 1);
        pos += 1;
        strncat(result + pos, names[i], result_size - pos - 1);
        pos += strlen(names[i]);
        strncat(result + pos, "\"", result_size - pos - 1);
        pos += 1;
    }
    
    strncat(result + pos, "]", result_size - pos - 1);
    bson_strfreev(names);
    
    return 0;
}

int mongo_list_collections(MONGO_CLIENT *client, const char *database,
                           char *result, size_t result_size) {
    if (!client || !client->client || !database || !result || result_size == 0) {
        log_error("Invalid parameters for list_collections");
        return -1;
    }
    
    bson_error_t error;
    mongoc_database_t *db = mongoc_client_get_database(client->client, database);
    char **names = mongoc_database_get_collection_names_with_opts(db, NULL, &error);
    
    if (!names) {
        log_error("Failed to list collections: %s", error.message);
        mongoc_database_destroy(db);
        return -1;
    }
    
    result[0] = '[';
    result[1] = '\0';
    size_t pos = 1;
    
    for (int i = 0; names[i]; i++) {
        if (i > 0) {
            strncat(result + pos, ",", result_size - pos - 1);
            pos += 1;
        }
        strncat(result + pos, "\"", result_size - pos - 1);
        pos += 1;
        strncat(result + pos, names[i], result_size - pos - 1);
        pos += strlen(names[i]);
        strncat(result + pos, "\"", result_size - pos - 1);
        pos += 1;
    }
    
    strncat(result + pos, "]", result_size - pos - 1);
    bson_strfreev(names);
    mongoc_database_destroy(db);
    
    return 0;
}

int mongo_drop_database(MONGO_CLIENT *client, const char *database) {
    if (!client || !client->client || !database) {
        log_error("Invalid parameters for drop_database");
        return -1;
    }
    
    mongoc_database_t *db = mongoc_client_get_database(client->client, database);
    bson_error_t error;
    
    bool success = mongoc_database_drop(db, &error);
    mongoc_database_destroy(db);
    
    if (!success) {
        log_error("Drop database failed: %s", error.message);
        return -1;
    }
    
    return 0;
}

int mongo_drop_collection(MONGO_COLLECTION *collection) {
    if (!collection || !collection->collection) {
        log_error("Invalid collection for drop");
        return -1;
    }
    
    bson_error_t error;
    bool success = mongoc_collection_drop(collection->collection, &error);
    
    if (!success) {
        log_error("Drop collection failed: %s", error.message);
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * Asynchronous Operations Implementation
 * ========================================================================== */

/* Helper to free async result data */
static void free_async_result_data(MONGO_ASYNC_RESULT *result) {
    if (result && result->data) {
        free(result->data);
        result->data = NULL;
        result->data_size = 0;
    }
}

/* Worker thread for async operations */
static void* mongo_async_worker(void *arg) {
    MONGO_ASYNC_HANDLE *handle = (MONGO_ASYNC_HANDLE*)arg;
    
    pthread_mutex_lock(&handle->mutex);
    if (handle->cancelled) {
        pthread_mutex_unlock(&handle->mutex);
        return NULL;
    }
    pthread_mutex_unlock(&handle->mutex);
    
    memset(&handle->result, 0, sizeof(MONGO_ASYNC_RESULT));
    handle->result.user_data = handle->user_data;
    handle->result.handle = handle;
    
    switch (handle->op_type) {
        case MONGO_ASYNC_INSERT_ONE: {
            char id_buf[64] = {0};
            pthread_mutex_lock(&handle->collection->mutex);
            int ret = mongo_insert_one(handle->collection, handle->document, 
                                      id_buf, sizeof(id_buf));
            pthread_mutex_unlock(&handle->collection->mutex);
            handle->result.success = (ret == 0);
            if (handle->result.success && id_buf[0]) {
                handle->result.data = strdup(id_buf);
                handle->result.data_size = strlen(id_buf);
                handle->result.affected_count = 1;
            }
            break;
        }
        
        case MONGO_ASYNC_FIND: {
            pthread_mutex_lock(&handle->collection->mutex);
            MONGO_CURSOR *cursor = mongo_find(handle->collection, handle->query, 
                                             handle->query_opts);
            pthread_mutex_unlock(&handle->collection->mutex);
            if (cursor) {
                char result_buf[8192];
                while (mongo_cursor_next(cursor, result_buf, sizeof(result_buf)) == 1) {
                    pthread_mutex_lock(&handle->mutex);
                    if (handle->cancelled) {
                        pthread_mutex_unlock(&handle->mutex);
                        mongo_cursor_destroy(cursor);
                        return NULL;
                    }
                    pthread_mutex_unlock(&handle->mutex);
                    
                    /* Call callback for each document */
                    MONGO_ASYNC_RESULT doc_result = {0};
                    doc_result.success = 1;
                    doc_result.data = result_buf;
                    doc_result.data_size = strlen(result_buf);
                    doc_result.user_data = handle->user_data;
                    doc_result.handle = handle;
                    handle->callback(&doc_result);
                }
                mongo_cursor_destroy(cursor);
                handle->result.success = 1;
            }
            break;
        }
        
        case MONGO_ASYNC_FIND_ONE: {
            char *result_buf = (char*)malloc(8192);
            if (result_buf) {
                pthread_mutex_lock(&handle->collection->mutex);
                int ret = mongo_find_one(handle->collection, handle->query, 
                                        result_buf, 8192);
                pthread_mutex_unlock(&handle->collection->mutex);
                handle->result.success = (ret == 0);
                if (handle->result.success) {
                    handle->result.data = result_buf;
                    handle->result.data_size = strlen(result_buf);
                } else {
                    free(result_buf);
                }
            }
            break;
        }
        
        case MONGO_ASYNC_UPDATE: {
            pthread_mutex_lock(&handle->collection->mutex);
            int ret = mongo_update(handle->collection, handle->query, 
                                  handle->update, handle->update_opts);
            pthread_mutex_unlock(&handle->collection->mutex);
            handle->result.success = (ret == 0);
            if (handle->result.success) {
                handle->result.affected_count = 1; /* Could be improved to return actual count */
            }
            break;
        }
        
        case MONGO_ASYNC_DELETE: {
            pthread_mutex_lock(&handle->collection->mutex);
            int ret = mongo_delete(handle->collection, handle->query, 
                                  handle->delete_opts);
            pthread_mutex_unlock(&handle->collection->mutex);
            handle->result.success = (ret == 0);
            if (handle->result.success) {
                handle->result.affected_count = 1; /* Could be improved */
            }
            break;
        }
        
        case MONGO_ASYNC_COUNT: {
            pthread_mutex_lock(&handle->collection->mutex);
            int64_t count = mongo_count(handle->collection, handle->query);
            pthread_mutex_unlock(&handle->collection->mutex);
            handle->result.success = (count >= 0);
            if (handle->result.success) {
                handle->result.affected_count = count;
            }
            break;
        }
        
        case MONGO_ASYNC_AGGREGATE: {
            pthread_mutex_lock(&handle->collection->mutex);
            MONGO_CURSOR *cursor = mongo_aggregate(handle->collection, handle->pipeline);
            pthread_mutex_unlock(&handle->collection->mutex);
            if (cursor) {
                char result_buf[8192];
                while (mongo_cursor_next(cursor, result_buf, sizeof(result_buf)) == 1) {
                    pthread_mutex_lock(&handle->mutex);
                    if (handle->cancelled) {
                        pthread_mutex_unlock(&handle->mutex);
                        mongo_cursor_destroy(cursor);
                        return NULL;
                    }
                    pthread_mutex_unlock(&handle->mutex);
                    
                    /* Call callback for each document */
                    MONGO_ASYNC_RESULT doc_result = {0};
                    doc_result.success = 1;
                    doc_result.data = result_buf;
                    doc_result.data_size = strlen(result_buf);
                    doc_result.user_data = handle->user_data;
                    doc_result.handle = handle;
                    handle->callback(&doc_result);
                }
                mongo_cursor_destroy(cursor);
                handle->result.success = 1;
            }
            break;
        }
    }
    
    pthread_mutex_lock(&handle->mutex);
    handle->complete = 1;
    pthread_mutex_unlock(&handle->mutex);
    
    /* Call final callback if not a streaming operation */
    if (handle->op_type != MONGO_ASYNC_FIND && 
        handle->op_type != MONGO_ASYNC_AGGREGATE && 
        handle->callback) {
        handle->callback(&handle->result);
    } else if (handle->callback) {
        /* Signal end of stream */
        MONGO_ASYNC_RESULT end_result = {0};
        end_result.success = 0; /* 0 indicates end of stream */
        end_result.user_data = handle->user_data;
        end_result.handle = handle;
        handle->callback(&end_result);
    }
    
    return NULL;
}

/* Create async handle helper */
static MONGO_ASYNC_HANDLE* create_async_handle(MONGO_COLLECTION *collection,
                                               mongo_async_op_type_t op_type,
                                               mongo_async_callback callback,
                                               void *user_data) {
    MONGO_ASYNC_HANDLE *handle = (MONGO_ASYNC_HANDLE*)calloc(1, sizeof(MONGO_ASYNC_HANDLE));
    if (!handle) {
        log_error("Failed to allocate async handle");
        return NULL;
    }
    
    handle->collection = collection;
    handle->op_type = op_type;
    handle->callback = callback;
    handle->user_data = user_data;
    handle->complete = 0;
    handle->cancelled = 0;
    pthread_mutex_init(&handle->mutex, NULL);
    
    return handle;
}

MONGO_ASYNC_HANDLE* mongo_insert_one_async(MONGO_COLLECTION *collection,
                                           const char *document,
                                           mongo_async_callback callback,
                                           void *user_data) {
    if (!collection || !document || !callback) {
        log_error("Invalid parameters for async insert");
        return NULL;
    }
    
    MONGO_ASYNC_HANDLE *handle = create_async_handle(collection, MONGO_ASYNC_INSERT_ONE, 
                                                     callback, user_data);
    if (!handle) return NULL;
    
    handle->document = strdup(document);
    
    if (pthread_create(&handle->thread, NULL, mongo_async_worker, handle) != 0) {
        log_error("Failed to create async thread");
        free(handle->document);
        free(handle);
        return NULL;
    }
    
    pthread_detach(handle->thread);
    return handle;
}

MONGO_ASYNC_HANDLE* mongo_find_async(MONGO_COLLECTION *collection,
                                     const char *query,
                                     MONGO_QUERY_OPTIONS *options,
                                     mongo_async_callback callback,
                                     void *user_data) {
    if (!collection || !callback) {
        log_error("Invalid parameters for async find");
        return NULL;
    }
    
    MONGO_ASYNC_HANDLE *handle = create_async_handle(collection, MONGO_ASYNC_FIND,
                                                     callback, user_data);
    if (!handle) return NULL;
    
    handle->query = query ? strdup(query) : strdup("{}");
    if (options) {
        handle->query_opts = (MONGO_QUERY_OPTIONS*)malloc(sizeof(MONGO_QUERY_OPTIONS));
        memcpy(handle->query_opts, options, sizeof(MONGO_QUERY_OPTIONS));
    }
    
    if (pthread_create(&handle->thread, NULL, mongo_async_worker, handle) != 0) {
        log_error("Failed to create async thread");
        free(handle->query);
        free(handle->query_opts);
        free(handle);
        return NULL;
    }
    
    pthread_detach(handle->thread);
    return handle;
}

MONGO_ASYNC_HANDLE* mongo_find_one_async(MONGO_COLLECTION *collection,
                                         const char *query,
                                         mongo_async_callback callback,
                                         void *user_data) {
    if (!collection || !callback) {
        log_error("Invalid parameters for async find_one");
        return NULL;
    }
    
    MONGO_ASYNC_HANDLE *handle = create_async_handle(collection, MONGO_ASYNC_FIND_ONE,
                                                     callback, user_data);
    if (!handle) return NULL;
    
    handle->query = query ? strdup(query) : strdup("{}");
    
    if (pthread_create(&handle->thread, NULL, mongo_async_worker, handle) != 0) {
        log_error("Failed to create async thread");
        free(handle->query);
        free(handle);
        return NULL;
    }
    
    pthread_detach(handle->thread);
    return handle;
}

MONGO_ASYNC_HANDLE* mongo_update_async(MONGO_COLLECTION *collection,
                                       const char *query,
                                       const char *update,
                                       MONGO_UPDATE_OPTIONS *options,
                                       mongo_async_callback callback,
                                       void *user_data) {
    if (!collection || !query || !update || !callback) {
        log_error("Invalid parameters for async update");
        return NULL;
    }
    
    MONGO_ASYNC_HANDLE *handle = create_async_handle(collection, MONGO_ASYNC_UPDATE,
                                                     callback, user_data);
    if (!handle) return NULL;
    
    handle->query = strdup(query);
    handle->update = strdup(update);
    if (options) {
        handle->update_opts = (MONGO_UPDATE_OPTIONS*)malloc(sizeof(MONGO_UPDATE_OPTIONS));
        memcpy(handle->update_opts, options, sizeof(MONGO_UPDATE_OPTIONS));
    }
    
    if (pthread_create(&handle->thread, NULL, mongo_async_worker, handle) != 0) {
        log_error("Failed to create async thread");
        free(handle->query);
        free(handle->update);
        free(handle->update_opts);
        free(handle);
        return NULL;
    }
    
    pthread_detach(handle->thread);
    return handle;
}

MONGO_ASYNC_HANDLE* mongo_delete_async(MONGO_COLLECTION *collection,
                                       const char *query,
                                       MONGO_DELETE_OPTIONS *options,
                                       mongo_async_callback callback,
                                       void *user_data) {
    if (!collection || !query || !callback) {
        log_error("Invalid parameters for async delete");
        return NULL;
    }
    
    MONGO_ASYNC_HANDLE *handle = create_async_handle(collection, MONGO_ASYNC_DELETE,
                                                     callback, user_data);
    if (!handle) return NULL;
    
    handle->query = strdup(query);
    if (options) {
        handle->delete_opts = (MONGO_DELETE_OPTIONS*)malloc(sizeof(MONGO_DELETE_OPTIONS));
        memcpy(handle->delete_opts, options, sizeof(MONGO_DELETE_OPTIONS));
    }
    
    if (pthread_create(&handle->thread, NULL, mongo_async_worker, handle) != 0) {
        log_error("Failed to create async thread");
        free(handle->query);
        free(handle->delete_opts);
        free(handle);
        return NULL;
    }
    
    pthread_detach(handle->thread);
    return handle;
}

MONGO_ASYNC_HANDLE* mongo_count_async(MONGO_COLLECTION *collection,
                                      const char *query,
                                      mongo_async_callback callback,
                                      void *user_data) {
    if (!collection || !callback) {
        log_error("Invalid parameters for async count");
        return NULL;
    }
    
    MONGO_ASYNC_HANDLE *handle = create_async_handle(collection, MONGO_ASYNC_COUNT,
                                                     callback, user_data);
    if (!handle) return NULL;
    
    handle->query = query ? strdup(query) : strdup("{}");
    
    if (pthread_create(&handle->thread, NULL, mongo_async_worker, handle) != 0) {
        log_error("Failed to create async thread");
        free(handle->query);
        free(handle);
        return NULL;
    }
    
    pthread_detach(handle->thread);
    return handle;
}

MONGO_ASYNC_HANDLE* mongo_aggregate_async(MONGO_COLLECTION *collection,
                                          const char *pipeline,
                                          mongo_async_callback callback,
                                          void *user_data) {
    if (!collection || !pipeline || !callback) {
        log_error("Invalid parameters for async aggregate");
        return NULL;
    }
    
    MONGO_ASYNC_HANDLE *handle = create_async_handle(collection, MONGO_ASYNC_AGGREGATE,
                                                     callback, user_data);
    if (!handle) return NULL;
    
    handle->pipeline = strdup(pipeline);
    
    if (pthread_create(&handle->thread, NULL, mongo_async_worker, handle) != 0) {
        log_error("Failed to create async thread");
        free(handle->pipeline);
        free(handle);
        return NULL;
    }
    
    pthread_detach(handle->thread);
    return handle;
}

int mongo_async_wait(MONGO_ASYNC_HANDLE *handle) {
    if (!handle) return 0;
    
    pthread_join(handle->thread, NULL);
    return handle->result.success;
}

int mongo_async_is_complete(MONGO_ASYNC_HANDLE *handle) {
    if (!handle) return 1;
    
    pthread_mutex_lock(&handle->mutex);
    int complete = handle->complete;
    pthread_mutex_unlock(&handle->mutex);
    
    return complete;
}

int mongo_async_cancel(MONGO_ASYNC_HANDLE *handle) {
    if (!handle) return -1;
    
    pthread_mutex_lock(&handle->mutex);
    handle->cancelled = 1;
    pthread_mutex_unlock(&handle->mutex);
    
    return 0;
}

void mongo_async_handle_destroy(MONGO_ASYNC_HANDLE *handle) {
    if (!handle) return;
    
    /* Wait for thread to complete if not detached */
    if (!handle->complete) {
        usleep(10000); /* Give thread time to finish */
    }
    
    pthread_mutex_destroy(&handle->mutex);
    free(handle->query);
    free(handle->document);
    free(handle->update);
    free(handle->pipeline);
    free(handle->query_opts);
    free(handle->update_opts);
    free(handle->delete_opts);
    free_async_result_data(&handle->result);
    free(handle);
}
