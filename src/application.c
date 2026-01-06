#include "application.h"
#include "module.h"
#include "service_controller.h"
#include "framework.h"
#include "http_server.h"
#include "kafka_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#define INITIAL_CAPACITY 10

APPLICATION* application_create(const char *name, int version)
{
    if (!name) {
        framework_log(LOG_LEVEL_ERROR, "Application name cannot be NULL");
        return NULL;
    }
    
    APPLICATION *app = (APPLICATION*)calloc(1, sizeof(APPLICATION));
    if (!app) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate memory for application");
        return NULL;
    }
    
    strncpy(app->name, name, sizeof(app->name) - 1);
    app->version = version;
    app->framework_version = FRAMEWORK_VERSION;
    app->initialized = 0;
    app->running = 0;
    app->context = NULL;
    app->http_server = NULL;
    app->kafka_client = NULL;
    
    app->init_list = NULL;
    app->cleanup_list = NULL;
    
    /* Initialize module array */
    app->module_capacity = INITIAL_CAPACITY;
    app->modules = (MODULE**)calloc(app->module_capacity, sizeof(MODULE*));
    if (!app->modules) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate memory for modules");
        free(app);
        return NULL;
    }
    app->module_count = 0;
    
    /* Initialize service array */
    app->service_capacity = INITIAL_CAPACITY;
    app->services = (SERVICE_CONTROLLER**)calloc(app->service_capacity, sizeof(SERVICE_CONTROLLER*));
    if (!app->services) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate memory for services");
        free(app->modules);
        free(app);
        return NULL;
    }
    app->service_count = 0;
    
    framework_log(LOG_LEVEL_INFO, "Application '%s' (v%d) created", name, version);
    return app;
}

void application_destroy(APPLICATION *app)
{
    if (!app) return;
    
    framework_log(LOG_LEVEL_INFO, "Destroying application '%s'", app->name);
    
    /* Free function lists */
    FUNCTION_LIST *current = app->init_list;
    while (current) {
        FUNCTION_LIST *next = current->next;
        free(current);
        current = next;
    }
    
    current = app->cleanup_list;
    while (current) {
        FUNCTION_LIST *next = current->next;
        free(current);
        current = next;
    }
    
    /* Free modules */
    if (app->modules) {
        for (size_t i = 0; i < app->module_count; i++) {
            if (app->modules[i]) {
                module_destroy(app->modules[i]);
            }
        }
        free(app->modules);
    }
    
    /* Free services */
    if (app->services) {
        for (size_t i = 0; i < app->service_count; i++) {
            if (app->services[i]) {
                service_controller_destroy(app->services[i]);
            }
        }
        free(app->services);
    }
    
    free(app);
}

int application_register_init_function(APPLICATION *app, const char *name, void (*func)(void *context), void *handle)
{
    if (!app || !func) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    FUNCTION_LIST *node = (FUNCTION_LIST*)calloc(1, sizeof(FUNCTION_LIST));
    if (!node) {
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    if (name) {
        strncpy(node->function_name, name, sizeof(node->function_name) - 1);
    }
    node->func = func;
    node->handle = handle;
    node->next = app->init_list;
    app->init_list = node;
    
    framework_log(LOG_LEVEL_DEBUG, "Registered init function '%s'", name ? name : "unnamed");
    return FRAMEWORK_SUCCESS;
}

int application_register_cleanup_function(APPLICATION *app, const char *name, void (*func)(void *context), void *handle)
{
    if (!app || !func) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    FUNCTION_LIST *node = (FUNCTION_LIST*)calloc(1, sizeof(FUNCTION_LIST));
    if (!node) {
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    if (name) {
        strncpy(node->function_name, name, sizeof(node->function_name) - 1);
    }
    node->func = func;
    node->handle = handle;
    node->next = app->cleanup_list;
    app->cleanup_list = node;
    
    framework_log(LOG_LEVEL_DEBUG, "Registered cleanup function '%s'", name ? name : "unnamed");
    return FRAMEWORK_SUCCESS;
}

int application_initialize(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (app->initialized) {
        framework_log(LOG_LEVEL_WARNING, "Application '%s' already initialized", app->name);
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Initializing application '%s'", app->name);
    
    /* Run init functions */
    FUNCTION_LIST *current = app->init_list;
    while (current) {
        if (current->func) {
            framework_log(LOG_LEVEL_DEBUG, "Calling init function '%s'", current->function_name);
            current->func(current->handle);
        }
        current = current->next;
    }
    
    /* Initialize modules */
    int result = application_initialize_modules(app);
    if (result != FRAMEWORK_SUCCESS) {
        framework_log(LOG_LEVEL_ERROR, "Failed to initialize modules");
        return result;
    }
    
    /* Initialize services */
    result = application_initialize_services(app);
    if (result != FRAMEWORK_SUCCESS) {
        framework_log(LOG_LEVEL_ERROR, "Failed to initialize services");
        return result;
    }
    
    app->initialized = 1;
    framework_log(LOG_LEVEL_INFO, "Application '%s' initialized successfully", app->name);
    return FRAMEWORK_SUCCESS;
}

int application_start(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (!app->initialized) {
        framework_log(LOG_LEVEL_ERROR, "Application '%s' not initialized", app->name);
        return FRAMEWORK_ERROR_STATE;
    }
    
    if (app->running) {
        framework_log(LOG_LEVEL_WARNING, "Application '%s' already running", app->name);
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Starting application '%s'", app->name);
    
    /* Start modules */
    int result = application_start_modules(app);
    if (result != FRAMEWORK_SUCCESS) {
        framework_log(LOG_LEVEL_ERROR, "Failed to start modules");
        return result;
    }
    
    /* Start services */
    result = application_start_services(app);
    if (result != FRAMEWORK_SUCCESS) {
        framework_log(LOG_LEVEL_ERROR, "Failed to start services");
        return result;
    }
    
    app->running = 1;
    framework_log(LOG_LEVEL_INFO, "Application '%s' started successfully", app->name);
    return FRAMEWORK_SUCCESS;
}

int application_stop(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (!app->running) {
        framework_log(LOG_LEVEL_WARNING, "Application '%s' not running", app->name);
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Stopping application '%s'", app->name);
    
    /* Stop services */
    application_stop_services(app);
    
    /* Stop modules */
    application_stop_modules(app);
    
    app->running = 0;
    framework_log(LOG_LEVEL_INFO, "Application '%s' stopped", app->name);
    return FRAMEWORK_SUCCESS;
}

int application_cleanup(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (app->running) {
        application_stop(app);
    }
    
    framework_log(LOG_LEVEL_INFO, "Cleaning up application '%s'", app->name);
    
    /* Run cleanup functions */
    FUNCTION_LIST *current = app->cleanup_list;
    while (current) {
        if (current->func) {
            framework_log(LOG_LEVEL_DEBUG, "Calling cleanup function '%s'", current->function_name);
            current->func(current->handle);
        }
        current = current->next;
    }
    
    app->initialized = 0;
    framework_log(LOG_LEVEL_INFO, "Application '%s' cleaned up", app->name);
    return FRAMEWORK_SUCCESS;
}

void* application_get_context(APPLICATION *app)
{
    if (!app) return NULL;
    return app->context;
}

void application_set_context(APPLICATION *app, void *context)
{
    if (app) {
        app->context = context;
    }
}

/* Global signal handler for graceful shutdown */
static APPLICATION *g_app = NULL;

static void application_signal_handler(int sig)
{
    (void)sig;
    if (g_app) {
        framework_log(LOG_LEVEL_INFO, "Received shutdown signal");
        g_app->running = 0;
        
        /* Stop HTTP server if running */
        if (g_app->http_server) {
            http_server_stop(g_app->http_server);
        }
    }
}

void application_set_http_server(APPLICATION *app, HTTP_SERVER *server)
{
    if (app) {
        app->http_server = server;
    }
}

void application_set_kafka_client(APPLICATION *app, KAFKA_CLIENT *kafka)
{
    if (app) {
        app->kafka_client = kafka;
    }
}

/* Unified event loop - handles both HTTP and Kafka */
int application_run(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (!app->http_server && !app->kafka_client) {
        framework_log(LOG_LEVEL_ERROR, "No HTTP server or Kafka client configured");
        return FRAMEWORK_ERROR_INVALID;
    }
    
    /* Set up signal handlers */
    g_app = app;
    signal(SIGINT, application_signal_handler);
    signal(SIGTERM, application_signal_handler);
    
    app->running = 1;
    
    framework_log(LOG_LEVEL_INFO, "Starting application '%s' event loop", app->name);
    
    /* Start HTTP server if configured */
    if (app->http_server) {
        framework_log(LOG_LEVEL_INFO, "Starting HTTP server...");
        if (http_server_start(app->http_server) != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Failed to start HTTP server");
            app->running = 0;
            return FRAMEWORK_ERROR_STATE;
        }
        framework_log(LOG_LEVEL_INFO, "HTTP server started successfully");
    }
    
    /* Start Kafka client if configured */
    if (app->kafka_client) {
        framework_log(LOG_LEVEL_INFO, "Starting Kafka client...");
        if (kafka_client_start(app->kafka_client) != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Failed to start Kafka client");
            app->running = 0;
            if (app->http_server) {
                http_server_stop(app->http_server);
            }
            return FRAMEWORK_ERROR_STATE;
        }
        framework_log(LOG_LEVEL_INFO, "Kafka client started successfully");
    }
    
    framework_log(LOG_LEVEL_INFO, "Application running - press Ctrl+C to stop");
    
    /* Main event loop */
    if (app->http_server && !app->kafka_client) {
        /* HTTP only - use blocking http_server_run */
        http_server_run(app->http_server);
    } else if (!app->http_server && app->kafka_client) {
        /* Kafka only - simple sleep loop */
        while (app->running) {
            sleep(1);
        }
    } else {
        /* Both HTTP and Kafka - run HTTP in current thread */
        /* Kafka consumers run in their own threads already */
        http_server_run(app->http_server);
    }
    
    /* Cleanup */
    framework_log(LOG_LEVEL_INFO, "Stopping application '%s'", app->name);
    
    if (app->kafka_client) {
        framework_log(LOG_LEVEL_INFO, "Stopping Kafka client...");
        kafka_client_stop(app->kafka_client);
    }
    
    if (app->http_server) {
        framework_log(LOG_LEVEL_INFO, "Stopping HTTP server...");
        http_server_stop(app->http_server);
    }
    
    app->running = 0;
    g_app = NULL;
    
    framework_log(LOG_LEVEL_INFO, "Application stopped successfully");
    return FRAMEWORK_SUCCESS;
}
