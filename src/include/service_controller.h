#ifndef SERVICE_CONTROLLER_H
#define SERVICE_CONTROLLER_H

#include <stddef.h>

/* Forward declaration */
typedef struct _application_ APPLICATION;

/* Service state */
typedef enum {
    SERVICE_STATE_UNINITIALIZED = 0,
    SERVICE_STATE_INITIALIZED,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_STOPPED,
    SERVICE_STATE_ERROR
} SERVICE_STATE;

/* Service request/response structures */
typedef struct _service_request_
{
    char operation[128];
    void *data;
    size_t data_size;
    void *context;
} SERVICE_REQUEST;

typedef struct _service_response_
{
    int status_code;
    char message[256];
    void *data;
    size_t data_size;
} SERVICE_RESPONSE;

/* Service callbacks */
typedef int (*service_init_fn)(void *service_context, void *app_context);
typedef int (*service_start_fn)(void *service_context, void *app_context);
typedef int (*service_stop_fn)(void *service_context, void *app_context);
typedef int (*service_cleanup_fn)(void *service_context, void *app_context);
typedef int (*service_handler_fn)(void *service_context, SERVICE_REQUEST *request, SERVICE_RESPONSE *response);

/* Service controller structure */
typedef struct _service_controller_
{
    char name[256];
    char description[512];
    int version;
    SERVICE_STATE state;
    
    void *context;  /* Service-specific context data */
    
    /* Lifecycle callbacks */
    service_init_fn init;
    service_start_fn start;
    service_stop_fn stop;
    service_cleanup_fn cleanup;
    
    /* Request handler */
    service_handler_fn handler;
    
    /* Service endpoints/operations */
    char **operations;
    size_t operation_count;
    size_t operation_capacity;
    
    /* Reference to parent application */
    APPLICATION *app;
} SERVICE_CONTROLLER;

/* Service controller creation and lifecycle */
SERVICE_CONTROLLER* service_controller_create(const char *name, const char *description, int version);
void service_controller_destroy(SERVICE_CONTROLLER *service);

void service_controller_set_init_callback(SERVICE_CONTROLLER *service, service_init_fn callback);
void service_controller_set_start_callback(SERVICE_CONTROLLER *service, service_start_fn callback);
void service_controller_set_stop_callback(SERVICE_CONTROLLER *service, service_stop_fn callback);
void service_controller_set_cleanup_callback(SERVICE_CONTROLLER *service, service_cleanup_fn callback);
void service_controller_set_handler(SERVICE_CONTROLLER *service, service_handler_fn handler);

int service_controller_register_operation(SERVICE_CONTROLLER *service, const char *operation);
void service_controller_set_context(SERVICE_CONTROLLER *service, void *context);
void* service_controller_get_context(SERVICE_CONTROLLER *service);

int service_controller_initialize(SERVICE_CONTROLLER *service, void *app_context);
int service_controller_start(SERVICE_CONTROLLER *service, void *app_context);
int service_controller_stop(SERVICE_CONTROLLER *service, void *app_context);
int service_controller_cleanup(SERVICE_CONTROLLER *service, void *app_context);

int service_controller_handle_request(SERVICE_CONTROLLER *service, SERVICE_REQUEST *request, SERVICE_RESPONSE *response);

const char* service_controller_get_state_string(SERVICE_STATE state);

/* Application-level service management */
int application_register_service(APPLICATION *app, SERVICE_CONTROLLER *service);
SERVICE_CONTROLLER* application_get_service(APPLICATION *app, const char *name);
int application_initialize_services(APPLICATION *app);
int application_start_services(APPLICATION *app);
int application_stop_services(APPLICATION *app);

/* Service invocation */
int application_invoke_service(APPLICATION *app, const char *service_name, SERVICE_REQUEST *request, SERVICE_RESPONSE *response);

#endif /* SERVICE_CONTROLLER_H */
