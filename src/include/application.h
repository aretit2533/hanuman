#ifndef APPLICATION_H
#define APPLICATION_H

#include <stddef.h>

/* Forward declarations */
typedef struct _module_ MODULE;
typedef struct _service_controller_ SERVICE_CONTROLLER;
typedef struct _kafka_client_ KAFKA_CLIENT;
typedef struct _http_server_ HTTP_SERVER;

/* Function list for initialization and cleanup */
typedef struct _function_list_
{
    struct _function_list_ *next;
    char function_name[256];
    void *handle;
    void (*func)(void *context);
} FUNCTION_LIST;

/* Application structure */
typedef struct _application_
{
    int framework_version;
    char name[256];
    int version;
    void *context;  /* Application-specific context */
    void *kafka_context;  /* Kafka client context */

    FUNCTION_LIST *init_list;
    FUNCTION_LIST *cleanup_list;
    
    /* Module management */
    MODULE **modules;
    size_t module_count;
    size_t module_capacity;
    
    /* Service controller management */
    SERVICE_CONTROLLER **services;
    size_t service_count;
    size_t service_capacity;
    
    /* Server management */
    HTTP_SERVER *http_server;
    KAFKA_CLIENT *kafka_client;
    
    int initialized;
    int running;
} APPLICATION;

/* Application lifecycle functions */
APPLICATION* application_create(const char *name, int version);
void application_destroy(APPLICATION *app);

int application_register_init_function(APPLICATION *app, const char *name, void (*func)(void *context), void *handle);
int application_register_cleanup_function(APPLICATION *app, const char *name, void (*func)(void *context), void *handle);

int application_initialize(APPLICATION *app);
int application_start(APPLICATION *app);
int application_stop(APPLICATION *app);
int application_cleanup(APPLICATION *app);

void* application_get_context(APPLICATION *app);
void application_set_context(APPLICATION *app, void *context);

/* Unified event loop - runs both HTTP and Kafka */
void application_set_http_server(APPLICATION *app, HTTP_SERVER *server);
void application_set_kafka_client(APPLICATION *app, KAFKA_CLIENT *kafka);
int application_run(APPLICATION *app);

#endif /* APPLICATION_H */
