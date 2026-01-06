#include "framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Example module context */
typedef struct {
    int counter;
    char data[256];
} MyModuleContext;

/* Example service context */
typedef struct {
    int request_count;
    char service_data[512];
} MyServiceContext;

/* Module callback implementations */
int my_module_init(void *module_context, void *app_context)
{
    (void)app_context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "MyModule: Initializing...");
    MyModuleContext *ctx = (MyModuleContext*)module_context;
    if (ctx) {
        ctx->counter = 0;
        strcpy(ctx->data, "Module initialized");
    }
    return FRAMEWORK_SUCCESS;
}

int my_module_start(void *module_context, void *app_context)
{
    (void)app_context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "MyModule: Starting...");
    MyModuleContext *ctx = (MyModuleContext*)module_context;
    if (ctx) {
        ctx->counter++;
        framework_log(LOG_LEVEL_DEBUG, "MyModule: Counter = %d", ctx->counter);
    }
    return FRAMEWORK_SUCCESS;
}

int my_module_stop(void *module_context, void *app_context)
{
    (void)module_context; /* Unused */
    (void)app_context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "MyModule: Stopping...");
    return FRAMEWORK_SUCCESS;
}

int my_module_cleanup(void *module_context, void *app_context)
{
    (void)module_context; /* Unused */
    (void)app_context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "MyModule: Cleaning up...");
    return FRAMEWORK_SUCCESS;
}

/* Service callback implementations */
int my_service_init(void *service_context, void *app_context)
{
    (void)app_context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "MyService: Initializing...");
    MyServiceContext *ctx = (MyServiceContext*)service_context;
    if (ctx) {
        ctx->request_count = 0;
        strcpy(ctx->service_data, "Service ready");
    }
    return FRAMEWORK_SUCCESS;
}

int my_service_start(void *service_context, void *app_context)
{
    (void)service_context; /* Unused */
    (void)app_context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "MyService: Starting...");
    return FRAMEWORK_SUCCESS;
}

int my_service_stop(void *service_context, void *app_context)
{
    (void)service_context; /* Unused */
    (void)app_context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "MyService: Stopping...");
    return FRAMEWORK_SUCCESS;
}

int my_service_cleanup(void *service_context, void *app_context)
{
    (void)service_context; /* Unused */
    (void)app_context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "MyService: Cleaning up...");
    return FRAMEWORK_SUCCESS;
}

int my_service_handler(void *service_context, SERVICE_REQUEST *request, SERVICE_RESPONSE *response)
{
    MyServiceContext *ctx = (MyServiceContext*)service_context;
    
    framework_log(LOG_LEVEL_INFO, "MyService: Handling request for operation '%s'", 
                 request->operation);
    
    if (ctx) {
        ctx->request_count++;
    }
    
    /* Handle different operations */
    if (strcmp(request->operation, "get_info") == 0) {
        response->status_code = FRAMEWORK_SUCCESS;
        snprintf(response->message, sizeof(response->message), 
                "Service info: %d requests processed", ctx ? ctx->request_count : 0);
        response->data = ctx ? ctx->service_data : NULL;
        response->data_size = ctx ? strlen(ctx->service_data) : 0;
    }
    else if (strcmp(request->operation, "echo") == 0) {
        response->status_code = FRAMEWORK_SUCCESS;
        snprintf(response->message, sizeof(response->message), "Echo response");
        response->data = request->data;
        response->data_size = request->data_size;
    }
    else if (strcmp(request->operation, "reset") == 0) {
        if (ctx) {
            ctx->request_count = 0;
        }
        response->status_code = FRAMEWORK_SUCCESS;
        snprintf(response->message, sizeof(response->message), "Counter reset");
    }
    else {
        response->status_code = FRAMEWORK_ERROR_INVALID;
        snprintf(response->message, sizeof(response->message), 
                "Unknown operation: %s", request->operation);
        return FRAMEWORK_ERROR_INVALID;
    }
    
    return FRAMEWORK_SUCCESS;
}

/* Application initialization callback */
void app_init_callback(void *context)
{
    (void)context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "Application initialization callback executed");
}

/* Application cleanup callback */
void app_cleanup_callback(void *context)
{
    (void)context; /* Unused */
    framework_log(LOG_LEVEL_INFO, "Application cleanup callback executed");
}

int main(int argc, char *argv[])
{
    (void)argc; /* Unused */
    (void)argv; /* Unused */
    
    int result;
    
    /* Initialize the framework */
    framework_init();
    framework_set_log_level(LOG_LEVEL_DEBUG);
    
    printf("\n=== Hanuman Framework Demo Application ===\n\n");
    
    /* Create application */
    APPLICATION *app = application_create("DemoApp", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Register application-level callbacks */
    application_register_init_function(app, "app_init", app_init_callback, NULL);
    application_register_cleanup_function(app, "app_cleanup", app_cleanup_callback, NULL);
    
    /* Create and configure a module */
    MODULE *my_module = module_create("MyModule", 1);
    if (!my_module) {
        fprintf(stderr, "Failed to create module\n");
        application_destroy(app);
        return 1;
    }
    
    MyModuleContext *mod_ctx = (MyModuleContext*)calloc(1, sizeof(MyModuleContext));
    module_set_context(my_module, mod_ctx);
    module_set_init_callback(my_module, my_module_init);
    module_set_start_callback(my_module, my_module_start);
    module_set_stop_callback(my_module, my_module_stop);
    module_set_cleanup_callback(my_module, my_module_cleanup);
    
    /* Register module with application */
    result = application_register_module(app, my_module);
    if (result != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register module\n");
        module_destroy(my_module);
        application_destroy(app);
        return 1;
    }
    
    /* Create and configure a service controller */
    SERVICE_CONTROLLER *my_service = service_controller_create("MyService", 
                                                                "Example service controller", 1);
    if (!my_service) {
        fprintf(stderr, "Failed to create service\n");
        application_destroy(app);
        return 1;
    }
    
    MyServiceContext *svc_ctx = (MyServiceContext*)calloc(1, sizeof(MyServiceContext));
    service_controller_set_context(my_service, svc_ctx);
    service_controller_set_init_callback(my_service, my_service_init);
    service_controller_set_start_callback(my_service, my_service_start);
    service_controller_set_stop_callback(my_service, my_service_stop);
    service_controller_set_cleanup_callback(my_service, my_service_cleanup);
    service_controller_set_handler(my_service, my_service_handler);
    
    /* Register operations */
    service_controller_register_operation(my_service, "get_info");
    service_controller_register_operation(my_service, "echo");
    service_controller_register_operation(my_service, "reset");
    
    /* Register service with application */
    result = application_register_service(app, my_service);
    if (result != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to register service\n");
        service_controller_destroy(my_service);
        application_destroy(app);
        return 1;
    }
    
    /* Initialize the application (this initializes all modules and services) */
    printf("\n--- Initializing Application ---\n");
    result = application_initialize(app);
    if (result != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to initialize application\n");
        application_destroy(app);
        return 1;
    }
    
    /* Start the application */
    printf("\n--- Starting Application ---\n");
    result = application_start(app);
    if (result != FRAMEWORK_SUCCESS) {
        fprintf(stderr, "Failed to start application\n");
        application_cleanup(app);
        application_destroy(app);
        return 1;
    }
    
    /* Demonstrate service invocation */
    printf("\n--- Testing Service Invocations ---\n");
    
    SERVICE_REQUEST request;
    SERVICE_RESPONSE response;
    
    /* Test 1: get_info operation */
    strcpy(request.operation, "get_info");
    request.data = NULL;
    request.data_size = 0;
    request.context = NULL;
    
    result = application_invoke_service(app, "MyService", &request, &response);
    if (result == FRAMEWORK_SUCCESS) {
        printf("Service response: [%d] %s\n", response.status_code, response.message);
    }
    
    /* Test 2: echo operation */
    strcpy(request.operation, "echo");
    char echo_data[] = "Hello from client!";
    request.data = echo_data;
    request.data_size = strlen(echo_data);
    
    result = application_invoke_service(app, "MyService", &request, &response);
    if (result == FRAMEWORK_SUCCESS) {
        printf("Service response: [%d] %s (data: %.*s)\n", 
               response.status_code, response.message, 
               (int)response.data_size, (char*)response.data);
    }
    
    /* Test 3: reset operation */
    strcpy(request.operation, "reset");
    request.data = NULL;
    request.data_size = 0;
    
    result = application_invoke_service(app, "MyService", &request, &response);
    if (result == FRAMEWORK_SUCCESS) {
        printf("Service response: [%d] %s\n", response.status_code, response.message);
    }
    
    /* Test 4: invalid operation */
    strcpy(request.operation, "invalid_op");
    request.data = NULL;
    request.data_size = 0;
    
    result = application_invoke_service(app, "MyService", &request, &response);
    printf("Service response: [%d] %s\n", response.status_code, response.message);
    
    /* Simulate running for a bit */
    printf("\n--- Application Running ---\n");
    sleep(1);
    
    /* Stop the application */
    printf("\n--- Stopping Application ---\n");
    application_stop(app);
    
    /* Cleanup */
    printf("\n--- Cleaning Up ---\n");
    application_cleanup(app);
    
    /* Free contexts */
    free(mod_ctx);
    free(svc_ctx);
    
    /* Destroy application */
    application_destroy(app);
    
    /* Shutdown framework */
    framework_shutdown();
    
    printf("\n=== Demo Complete ===\n\n");
    
    return 0;
}
