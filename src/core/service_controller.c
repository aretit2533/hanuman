#define _POSIX_C_SOURCE 200809L
#include "service_controller.h"
#include "application.h"
#include "framework.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_OPERATION_CAPACITY 10

SERVICE_CONTROLLER* service_controller_create(const char *name, const char *description, int version)
{
    if (!name) {
        framework_log(LOG_LEVEL_ERROR, "Service name cannot be NULL");
        return NULL;
    }
    
    SERVICE_CONTROLLER *service = (SERVICE_CONTROLLER*)calloc(1, sizeof(SERVICE_CONTROLLER));
    if (!service) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate memory for service controller");
        return NULL;
    }
    
    strncpy(service->name, name, sizeof(service->name) - 1);
    if (description) {
        strncpy(service->description, description, sizeof(service->description) - 1);
    }
    service->version = version;
    service->state = SERVICE_STATE_UNINITIALIZED;
    service->context = NULL;
    service->app = NULL;
    
    service->init = NULL;
    service->start = NULL;
    service->stop = NULL;
    service->cleanup = NULL;
    service->handler = NULL;
    
    service->operation_capacity = INITIAL_OPERATION_CAPACITY;
    service->operations = (char**)calloc(service->operation_capacity, sizeof(char*));
    if (!service->operations) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate memory for operations");
        free(service);
        return NULL;
    }
    service->operation_count = 0;
    
    framework_log(LOG_LEVEL_DEBUG, "Service controller '%s' (v%d) created", name, version);
    return service;
}

void service_controller_destroy(SERVICE_CONTROLLER *service)
{
    if (!service) return;
    
    framework_log(LOG_LEVEL_DEBUG, "Destroying service controller '%s'", service->name);
    
    if (service->operations) {
        for (size_t i = 0; i < service->operation_count; i++) {
            free(service->operations[i]);
        }
        free(service->operations);
    }
    
    free(service);
}

void service_controller_set_init_callback(SERVICE_CONTROLLER *service, service_init_fn callback)
{
    if (service) {
        service->init = callback;
    }
}

void service_controller_set_start_callback(SERVICE_CONTROLLER *service, service_start_fn callback)
{
    if (service) {
        service->start = callback;
    }
}

void service_controller_set_stop_callback(SERVICE_CONTROLLER *service, service_stop_fn callback)
{
    if (service) {
        service->stop = callback;
    }
}

void service_controller_set_cleanup_callback(SERVICE_CONTROLLER *service, service_cleanup_fn callback)
{
    if (service) {
        service->cleanup = callback;
    }
}

void service_controller_set_handler(SERVICE_CONTROLLER *service, service_handler_fn handler)
{
    if (service) {
        service->handler = handler;
    }
}

int service_controller_register_operation(SERVICE_CONTROLLER *service, const char *operation)
{
    if (!service || !operation) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    /* Check if operation already registered */
    for (size_t i = 0; i < service->operation_count; i++) {
        if (strcmp(service->operations[i], operation) == 0) {
            framework_log(LOG_LEVEL_WARNING, "Operation '%s' already registered in service '%s'", 
                         operation, service->name);
            return FRAMEWORK_ERROR_EXISTS;
        }
    }
    
    /* Resize array if needed */
    if (service->operation_count >= service->operation_capacity) {
        size_t new_capacity = service->operation_capacity * 2;
        char **new_operations = (char**)realloc(service->operations, 
                                                new_capacity * sizeof(char*));
        if (!new_operations) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        service->operations = new_operations;
        service->operation_capacity = new_capacity;
    }
    
    service->operations[service->operation_count] = strdup(operation);
    if (!service->operations[service->operation_count]) {
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    service->operation_count++;
    framework_log(LOG_LEVEL_DEBUG, "Operation '%s' registered in service '%s'", 
                 operation, service->name);
    return FRAMEWORK_SUCCESS;
}

void service_controller_set_context(SERVICE_CONTROLLER *service, void *context)
{
    if (service) {
        service->context = context;
    }
}

void* service_controller_get_context(SERVICE_CONTROLLER *service)
{
    if (!service) return NULL;
    return service->context;
}

int service_controller_initialize(SERVICE_CONTROLLER *service, void *app_context)
{
    if (!service) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (service->state != SERVICE_STATE_UNINITIALIZED) {
        framework_log(LOG_LEVEL_WARNING, "Service '%s' already initialized", service->name);
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Initializing service '%s'", service->name);
    
    if (service->init) {
        int result = service->init(service->context, app_context);
        if (result != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Service '%s' initialization failed", service->name);
            service->state = SERVICE_STATE_ERROR;
            return result;
        }
    }
    
    service->state = SERVICE_STATE_INITIALIZED;
    framework_log(LOG_LEVEL_INFO, "Service '%s' initialized", service->name);
    return FRAMEWORK_SUCCESS;
}

int service_controller_start(SERVICE_CONTROLLER *service, void *app_context)
{
    if (!service) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (service->state != SERVICE_STATE_INITIALIZED && service->state != SERVICE_STATE_STOPPED) {
        framework_log(LOG_LEVEL_ERROR, "Service '%s' cannot be started from state %s", 
                     service->name, service_controller_get_state_string(service->state));
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Starting service '%s'", service->name);
    
    if (service->start) {
        int result = service->start(service->context, app_context);
        if (result != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Service '%s' start failed", service->name);
            service->state = SERVICE_STATE_ERROR;
            return result;
        }
    }
    
    service->state = SERVICE_STATE_RUNNING;
    framework_log(LOG_LEVEL_INFO, "Service '%s' started", service->name);
    return FRAMEWORK_SUCCESS;
}

int service_controller_stop(SERVICE_CONTROLLER *service, void *app_context)
{
    if (!service) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (service->state != SERVICE_STATE_RUNNING) {
        framework_log(LOG_LEVEL_WARNING, "Service '%s' not running", service->name);
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Stopping service '%s'", service->name);
    
    if (service->stop) {
        int result = service->stop(service->context, app_context);
        if (result != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Service '%s' stop failed", service->name);
            service->state = SERVICE_STATE_ERROR;
            return result;
        }
    }
    
    service->state = SERVICE_STATE_STOPPED;
    framework_log(LOG_LEVEL_INFO, "Service '%s' stopped", service->name);
    return FRAMEWORK_SUCCESS;
}

int service_controller_cleanup(SERVICE_CONTROLLER *service, void *app_context)
{
    if (!service) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    framework_log(LOG_LEVEL_INFO, "Cleaning up service '%s'", service->name);
    
    if (service->cleanup) {
        int result = service->cleanup(service->context, app_context);
        if (result != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Service '%s' cleanup failed", service->name);
            return result;
        }
    }
    
    service->state = SERVICE_STATE_UNINITIALIZED;
    framework_log(LOG_LEVEL_INFO, "Service '%s' cleaned up", service->name);
    return FRAMEWORK_SUCCESS;
}

int service_controller_handle_request(SERVICE_CONTROLLER *service, SERVICE_REQUEST *request, SERVICE_RESPONSE *response)
{
    if (!service || !request || !response) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (service->state != SERVICE_STATE_RUNNING) {
        framework_log(LOG_LEVEL_ERROR, "Service '%s' not running", service->name);
        response->status_code = FRAMEWORK_ERROR_STATE;
        snprintf(response->message, sizeof(response->message), "Service not running");
        return FRAMEWORK_ERROR_STATE;
    }
    
    if (!service->handler) {
        framework_log(LOG_LEVEL_ERROR, "Service '%s' has no handler", service->name);
        response->status_code = FRAMEWORK_ERROR_CALLBACK;
        snprintf(response->message, sizeof(response->message), "No handler registered");
        return FRAMEWORK_ERROR_CALLBACK;
    }
    
    framework_log(LOG_LEVEL_DEBUG, "Service '%s' handling request for operation '%s'", 
                 service->name, request->operation);
    
    return service->handler(service->context, request, response);
}

const char* service_controller_get_state_string(SERVICE_STATE state)
{
    switch (state) {
        case SERVICE_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case SERVICE_STATE_INITIALIZED: return "INITIALIZED";
        case SERVICE_STATE_RUNNING: return "RUNNING";
        case SERVICE_STATE_STOPPED: return "STOPPED";
        case SERVICE_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/* Application-level service management */
int application_register_service(APPLICATION *app, SERVICE_CONTROLLER *service)
{
    if (!app || !service) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    /* Check if service already exists */
    for (size_t i = 0; i < app->service_count; i++) {
        if (strcmp(app->services[i]->name, service->name) == 0) {
            framework_log(LOG_LEVEL_ERROR, "Service '%s' already registered", service->name);
            return FRAMEWORK_ERROR_EXISTS;
        }
    }
    
    /* Resize array if needed */
    if (app->service_count >= app->service_capacity) {
        size_t new_capacity = app->service_capacity * 2;
        SERVICE_CONTROLLER **new_services = (SERVICE_CONTROLLER**)realloc(app->services, 
                                                                           new_capacity * sizeof(SERVICE_CONTROLLER*));
        if (!new_services) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        app->services = new_services;
        app->service_capacity = new_capacity;
    }
    
    app->services[app->service_count++] = service;
    service->app = app;
    
    framework_log(LOG_LEVEL_INFO, "Service '%s' registered to application '%s'", 
                 service->name, app->name);
    return FRAMEWORK_SUCCESS;
}

SERVICE_CONTROLLER* application_get_service(APPLICATION *app, const char *name)
{
    if (!app || !name) {
        return NULL;
    }
    
    for (size_t i = 0; i < app->service_count; i++) {
        if (strcmp(app->services[i]->name, name) == 0) {
            return app->services[i];
        }
    }
    
    return NULL;
}

int application_initialize_services(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    framework_log(LOG_LEVEL_INFO, "Initializing %zu services", app->service_count);
    
    for (size_t i = 0; i < app->service_count; i++) {
        int result = service_controller_initialize(app->services[i], app->context);
        if (result != FRAMEWORK_SUCCESS) {
            return result;
        }
    }
    
    return FRAMEWORK_SUCCESS;
}

int application_start_services(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    framework_log(LOG_LEVEL_INFO, "Starting %zu services", app->service_count);
    
    for (size_t i = 0; i < app->service_count; i++) {
        int result = service_controller_start(app->services[i], app->context);
        if (result != FRAMEWORK_SUCCESS) {
            return result;
        }
    }
    
    return FRAMEWORK_SUCCESS;
}

int application_stop_services(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    framework_log(LOG_LEVEL_INFO, "Stopping %zu services", app->service_count);
    
    /* Stop services in reverse order */
    for (int i = (int)app->service_count - 1; i >= 0; i--) {
        service_controller_stop(app->services[i], app->context);
    }
    
    return FRAMEWORK_SUCCESS;
}

int application_invoke_service(APPLICATION *app, const char *service_name, 
                               SERVICE_REQUEST *request, SERVICE_RESPONSE *response)
{
    if (!app || !service_name || !request || !response) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    SERVICE_CONTROLLER *service = application_get_service(app, service_name);
    if (!service) {
        framework_log(LOG_LEVEL_ERROR, "Service '%s' not found", service_name);
        response->status_code = FRAMEWORK_ERROR_NOT_FOUND;
        snprintf(response->message, sizeof(response->message), "Service not found");
        return FRAMEWORK_ERROR_NOT_FOUND;
    }
    
    return service_controller_handle_request(service, request, response);
}
