# Hanuman Framework - Quick Start Guide

**High-performance C runtime framework**

## Installation

### Building the Library

```bash
cd equinox-framework
make release          # Build optimized version
# or
make debug           # Build with debug symbols
```

### Using the Library in Your Project

1. **Include the framework header:**
```c
#include <equinox/framework.h>
```

2. **Link against the library:**
```bash
gcc -I/usr/local/include myapp.c -L/usr/local/lib -lequinox -o myapp
```

3. **Or add to your Makefile:**
```makefile
CFLAGS += -I/usr/local/include/equinox
LDFLAGS += -L/usr/local/lib -lequinox
```

## Basic Application Template

```c
#include "framework.h"

int main() {
    // 1. Initialize framework
    framework_init();
    framework_set_log_level(LOG_LEVEL_INFO);
    
    // 2. Create application
    APPLICATION *app = application_create("MyApp", 1);
    
    // 3. Register modules and services (see below)
    // ...
    
    // 4. Initialize and start
    application_initialize(app);
    application_start(app);
    
    // 5. Run your application logic
    // ...
    
    // 6. Cleanup
    application_stop(app);
    application_cleanup(app);
    application_destroy(app);
    framework_shutdown();
    
    return 0;
}
```

## Creating a Module

```c
// 1. Define module context (optional)
typedef struct {
    int data;
    char buffer[256];
} MyModuleContext;

// 2. Define lifecycle callbacks
int my_module_init(void *module_context, void *app_context) {
    MyModuleContext *ctx = (MyModuleContext*)module_context;
    ctx->data = 0;
    return FRAMEWORK_SUCCESS;
}

int my_module_start(void *module_context, void *app_context) {
    // Start module operations
    return FRAMEWORK_SUCCESS;
}

int my_module_stop(void *module_context, void *app_context) {
    // Stop module operations
    return FRAMEWORK_SUCCESS;
}

int my_module_cleanup(void *module_context, void *app_context) {
    // Cleanup resources
    return FRAMEWORK_SUCCESS;
}

// 3. Register module
void register_my_module(APPLICATION *app) {
    MODULE *mod = module_create("MyModule", 1);
    
    // Set context
    MyModuleContext *ctx = calloc(1, sizeof(MyModuleContext));
    module_set_context(mod, ctx);
    
    // Set callbacks
    module_set_init_callback(mod, my_module_init);
    module_set_start_callback(mod, my_module_start);
    module_set_stop_callback(mod, my_module_stop);
    module_set_cleanup_callback(mod, my_module_cleanup);
    
    // Add dependencies (if needed)
    // module_add_dependency(mod, "OtherModule");
    
    // Register with application
    application_register_module(app, mod);
}
```

## Creating a Service Controller

```c
// 1. Define service context (optional)
typedef struct {
    int request_count;
    void *data;
} MyServiceContext;

// 2. Define service handler
int my_service_handler(void *service_context, 
                      SERVICE_REQUEST *request, 
                      SERVICE_RESPONSE *response) {
    MyServiceContext *ctx = (MyServiceContext*)service_context;
    
    if (strcmp(request->operation, "get_status") == 0) {
        ctx->request_count++;
        response->status_code = FRAMEWORK_SUCCESS;
        snprintf(response->message, sizeof(response->message),
                "Requests: %d", ctx->request_count);
        return FRAMEWORK_SUCCESS;
    }
    
    response->status_code = FRAMEWORK_ERROR_INVALID;
    strcpy(response->message, "Unknown operation");
    return FRAMEWORK_ERROR_INVALID;
}

// 3. Define lifecycle callbacks
int my_service_init(void *service_context, void *app_context) {
    MyServiceContext *ctx = (MyServiceContext*)service_context;
    ctx->request_count = 0;
    return FRAMEWORK_SUCCESS;
}

int my_service_start(void *service_context, void *app_context) {
    return FRAMEWORK_SUCCESS;
}

int my_service_stop(void *service_context, void *app_context) {
    return FRAMEWORK_SUCCESS;
}

int my_service_cleanup(void *service_context, void *app_context) {
    return FRAMEWORK_SUCCESS;
}

// 4. Register service
void register_my_service(APPLICATION *app) {
    SERVICE_CONTROLLER *svc = service_controller_create(
        "MyService",
        "Service description",
        1
    );
    
    // Set context
    MyServiceContext *ctx = calloc(1, sizeof(MyServiceContext));
    service_controller_set_context(svc, ctx);
    
    // Set callbacks
    service_controller_set_init_callback(svc, my_service_init);
    service_controller_set_start_callback(svc, my_service_start);
    service_controller_set_stop_callback(svc, my_service_stop);
    service_controller_set_cleanup_callback(svc, my_service_cleanup);
    service_controller_set_handler(svc, my_service_handler);
    
    // Register operations
    service_controller_register_operation(svc, "get_status");
    service_controller_register_operation(svc, "update");
    
    // Register with application
    application_register_service(app, svc);
}
```

## Calling a Service

```c
SERVICE_REQUEST request;
SERVICE_RESPONSE response;

// Prepare request
strcpy(request.operation, "get_status");
request.data = NULL;
request.data_size = 0;

// Invoke service
int result = application_invoke_service(app, "MyService", &request, &response);

if (result == FRAMEWORK_SUCCESS) {
    printf("Service response: [%d] %s\n", 
           response.status_code, response.message);
} else {
    fprintf(stderr, "Service invocation failed: %d\n", result);
}
```

## Common Patterns

### Module with Dependencies

```c
MODULE *base_module = module_create("BaseModule", 1);
application_register_module(app, base_module);

MODULE *dependent_module = module_create("DependentModule", 1);
module_add_dependency(dependent_module, "BaseModule");
application_register_module(app, dependent_module);
```

### Application-level Init/Cleanup

```c
void my_init_callback(void *context) {
    // Called during application initialization
}

void my_cleanup_callback(void *context) {
    // Called during application cleanup
}

application_register_init_function(app, "my_init", my_init_callback, NULL);
application_register_cleanup_function(app, "my_cleanup", my_cleanup_callback, NULL);
```

### Using Application Context

```c
typedef struct {
    char app_name[256];
    int instance_id;
} AppContext;

AppContext *ctx = calloc(1, sizeof(AppContext));
strcpy(ctx->app_name, "MyApp");
ctx->instance_id = 1;

application_set_context(app, ctx);

// Later, in modules/services:
AppContext *ctx = (AppContext*)application_get_context(app);
```

## Error Handling

Always check return values:

```c
int result = application_initialize(app);
if (result != FRAMEWORK_SUCCESS) {
    fprintf(stderr, "Initialization failed: %d\n", result);
    application_destroy(app);
    return 1;
}
```

## Logging

```c
// Set log level
framework_set_log_level(LOG_LEVEL_DEBUG);

// Use in your code
framework_log(LOG_LEVEL_INFO, "Application started");
framework_log(LOG_LEVEL_DEBUG, "Counter value: %d", counter);
framework_log(LOG_LEVEL_WARNING, "Resource limit reached");
framework_log(LOG_LEVEL_ERROR, "Failed to open file: %s", filename);
```

## Best Practices

1. **Always check return values** - Most functions return error codes
2. **Use context structures** - Store module/service-specific data in contexts
3. **Implement all callbacks** - Even if they're empty, provide them for clarity
4. **Free allocated memory** - Clean up contexts when destroying modules/services
5. **Use dependencies** - Declare module dependencies for proper initialization order
6. **Log appropriately** - Use DEBUG for detailed info, INFO for important events
7. **Handle errors gracefully** - Provide meaningful error messages

## Complete Example

See `examples/demo_app.c` for a complete working example.
