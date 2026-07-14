# Hanuman Framework API Reference

**High-performance C runtime framework**

## Table of Contents

1. [Framework Functions](#framework-functions)
2. [Application Functions](#application-functions)
3. [Module Functions](#module-functions)
4. [Service Controller Functions](#service-controller-functions)
5. [Data Structures](#data-structures)
6. [Constants and Enums](#constants-and-enums)

---

## Framework Functions

### `int framework_init(void)`
Initialize the framework. Must be called before any other framework operations.

**Returns:** `FRAMEWORK_SUCCESS` on success

**Example:**
```c
framework_init();
```

---

### `void framework_shutdown(void)`
Shutdown the framework and cleanup global resources.

**Example:**
```c
framework_shutdown();
```

---

### `const char* framework_get_version_string(void)`
Get the framework version as a string.

**Returns:** Version string (e.g., "1.0.0")

---

### `int framework_get_version(void)`
Get the framework version as an integer.

**Returns:** Version number packed as (major << 16 | minor << 8 | patch)

---

### `void framework_log(LOG_LEVEL level, const char *format, ...)`
Log a message with the specified log level.

**Parameters:**
- `level` - Log level (DEBUG, INFO, WARNING, ERROR)
- `format` - Printf-style format string
- `...` - Format arguments

**Example:**
```c
framework_log(LOG_LEVEL_INFO, "User %s logged in", username);
```

---

### `void framework_set_log_level(LOG_LEVEL level)`
Set the minimum log level for output.

**Parameters:**
- `level` - Minimum log level to display

---

## Application Functions

### `APPLICATION* application_create(const char *name, int version)`
Create a new application instance.

**Parameters:**
- `name` - Application name (max 255 chars)
- `version` - Application version number

**Returns:** Pointer to APPLICATION or NULL on failure

**Example:**
```c
APPLICATION *app = application_create("MyApp", 1);
```

---

### `void application_destroy(APPLICATION *app)`
Destroy an application and free all resources.

**Parameters:**
- `app` - Application to destroy

**Note:** Also destroys all registered modules and services

---

### `int application_initialize(APPLICATION *app)`
Initialize the application and all registered modules and services.

**Parameters:**
- `app` - Application to initialize

**Returns:** `FRAMEWORK_SUCCESS` or error code

**Note:** Must be called before `application_start()`

---

### `int application_start(APPLICATION *app)`
Start the application and all modules/services.

**Parameters:**
- `app` - Application to start

**Returns:** `FRAMEWORK_SUCCESS` or error code

**Note:** Application must be initialized first

---

### `int application_stop(APPLICATION *app)`
Stop the application and all modules/services.

**Parameters:**
- `app` - Application to stop

**Returns:** `FRAMEWORK_SUCCESS` or error code

---

### `int application_cleanup(APPLICATION *app)`
Cleanup the application after stopping.

**Parameters:**
- `app` - Application to cleanup

**Returns:** `FRAMEWORK_SUCCESS` or error code

---

### `int application_register_init_function(APPLICATION *app, const char *name, void (*func)(void *context), void *handle)`
Register a function to be called during application initialization.

**Parameters:**
- `app` - Application instance
- `name` - Function name (for logging)
- `func` - Callback function
- `handle` - Context data passed to callback

**Returns:** `FRAMEWORK_SUCCESS` or error code

---

### `int application_register_cleanup_function(APPLICATION *app, const char *name, void (*func)(void *context), void *handle)`
Register a function to be called during application cleanup.

**Parameters:**
- `app` - Application instance
- `name` - Function name (for logging)
- `func` - Callback function
- `handle` - Context data passed to callback

**Returns:** `FRAMEWORK_SUCCESS` or error code

---

### `int application_register_module(APPLICATION *app, MODULE *mod)`
Register a module with the application.

**Parameters:**
- `app` - Application instance
- `mod` - Module to register

**Returns:** `FRAMEWORK_SUCCESS` or error code

---

### `MODULE* application_get_module(APPLICATION *app, const char *name)`
Get a registered module by name.

**Parameters:**
- `app` - Application instance
- `name` - Module name

**Returns:** Pointer to MODULE or NULL if not found

---

### `int application_register_service(APPLICATION *app, SERVICE_CONTROLLER *service)`
Register a service controller with the application.

**Parameters:**
- `app` - Application instance
- `service` - Service controller to register

**Returns:** `FRAMEWORK_SUCCESS` or error code

---

### `SERVICE_CONTROLLER* application_get_service(APPLICATION *app, const char *name)`
Get a registered service by name.

**Parameters:**
- `app` - Application instance
- `name` - Service name

**Returns:** Pointer to SERVICE_CONTROLLER or NULL if not found

---

### `int application_invoke_service(APPLICATION *app, const char *service_name, SERVICE_REQUEST *request, SERVICE_RESPONSE *response)`
Invoke a service with a request.

**Parameters:**
- `app` - Application instance
- `service_name` - Name of service to invoke
- `request` - Service request structure
- `response` - Service response structure (output)

**Returns:** `FRAMEWORK_SUCCESS` or error code

---

### `void* application_get_context(APPLICATION *app)`
Get the application context data.

**Parameters:**
- `app` - Application instance

**Returns:** Context pointer or NULL

---

### `void application_set_context(APPLICATION *app, void *context)`
Set the application context data.

**Parameters:**
- `app` - Application instance
- `context` - Context data pointer

---

## Module Functions

### `MODULE* module_create(const char *name, int version)`
Create a new module.

**Parameters:**
- `name` - Module name (max 255 chars)
- `version` - Module version number

**Returns:** Pointer to MODULE or NULL on failure

---

### `void module_destroy(MODULE *mod)`
Destroy a module and free resources.

**Parameters:**
- `mod` - Module to destroy

---

### `void module_set_init_callback(MODULE *mod, module_init_fn callback)`
Set the module initialization callback.

**Parameters:**
- `mod` - Module instance
- `callback` - Init callback function

**Callback signature:**
```c
int callback(void *module_context, void *app_context)
```

---

### `void module_set_start_callback(MODULE *mod, module_start_fn callback)`
Set the module start callback.

**Parameters:**
- `mod` - Module instance
- `callback` - Start callback function

---

### `void module_set_stop_callback(MODULE *mod, module_stop_fn callback)`
Set the module stop callback.

**Parameters:**
- `mod` - Module instance
- `callback` - Stop callback function

---

### `void module_set_cleanup_callback(MODULE *mod, module_cleanup_fn callback)`
Set the module cleanup callback.

**Parameters:**
- `mod` - Module instance
- `callback` - Cleanup callback function

---

### `int module_add_dependency(MODULE *mod, const char *dep_name)`
Add a module dependency.

**Parameters:**
- `mod` - Module instance
- `dep_name` - Name of required module

**Returns:** `FRAMEWORK_SUCCESS` or error code

**Note:** Dependencies must be registered before the dependent module is initialized

---

### `void module_set_context(MODULE *mod, void *context)`
Set module-specific context data.

**Parameters:**
- `mod` - Module instance
- `context` - Context data pointer

---

### `void* module_get_context(MODULE *mod)`
Get module-specific context data.

**Parameters:**
- `mod` - Module instance

**Returns:** Context pointer or NULL

---

## Service Controller Functions

### `SERVICE_CONTROLLER* service_controller_create(const char *name, const char *description, int version)`
Create a new service controller.

**Parameters:**
- `name` - Service name (max 255 chars)
- `description` - Service description (max 511 chars)
- `version` - Service version number

**Returns:** Pointer to SERVICE_CONTROLLER or NULL on failure

---

### `void service_controller_destroy(SERVICE_CONTROLLER *service)`
Destroy a service controller and free resources.

**Parameters:**
- `service` - Service controller to destroy

---

### `void service_controller_set_handler(SERVICE_CONTROLLER *service, service_handler_fn handler)`
Set the service request handler.

**Parameters:**
- `service` - Service controller instance
- `handler` - Request handler function

**Handler signature:**
```c
int handler(void *service_context, SERVICE_REQUEST *request, SERVICE_RESPONSE *response)
```

---

### `int service_controller_register_operation(SERVICE_CONTROLLER *service, const char *operation)`
Register an operation that the service can handle.

**Parameters:**
- `service` - Service controller instance
- `operation` - Operation name

**Returns:** `FRAMEWORK_SUCCESS` or error code

---

### `void service_controller_set_init_callback(SERVICE_CONTROLLER *service, service_init_fn callback)`
Set the service initialization callback.

**Parameters:**
- `service` - Service controller instance
- `callback` - Init callback function

---

### `void service_controller_set_start_callback(SERVICE_CONTROLLER *service, service_start_fn callback)`
Set the service start callback.

---

### `void service_controller_set_stop_callback(SERVICE_CONTROLLER *service, service_stop_fn callback)`
Set the service stop callback.

---

### `void service_controller_set_cleanup_callback(SERVICE_CONTROLLER *service, service_cleanup_fn callback)`
Set the service cleanup callback.

---

### `void service_controller_set_context(SERVICE_CONTROLLER *service, void *context)`
Set service-specific context data.

**Parameters:**
- `service` - Service controller instance
- `context` - Context data pointer

---

### `void* service_controller_get_context(SERVICE_CONTROLLER *service)`
Get service-specific context data.

**Parameters:**
- `service` - Service controller instance

**Returns:** Context pointer or NULL

---

## Data Structures

### `SERVICE_REQUEST`
```c
typedef struct _service_request_ {
    char operation[128];     // Operation name
    void *data;             // Request data
    size_t data_size;       // Size of request data
    void *context;          // Request context
} SERVICE_REQUEST;
```

### `SERVICE_RESPONSE`
```c
typedef struct _service_response_ {
    int status_code;        // Response status code
    char message[256];      // Response message
    void *data;            // Response data
    size_t data_size;      // Size of response data
} SERVICE_RESPONSE;
```

---

## Constants and Enums

### Error Codes
```c
#define FRAMEWORK_SUCCESS           0   // Operation successful
#define FRAMEWORK_ERROR_NULL_PTR   -1   // Null pointer error
#define FRAMEWORK_ERROR_INVALID    -2   // Invalid parameter
#define FRAMEWORK_ERROR_MEMORY     -3   // Memory allocation error
#define FRAMEWORK_ERROR_NOT_FOUND  -4   // Resource not found
#define FRAMEWORK_ERROR_EXISTS     -5   // Resource already exists
#define FRAMEWORK_ERROR_STATE      -6   // Invalid state
#define FRAMEWORK_ERROR_DEPENDENCY -7   // Dependency error
#define FRAMEWORK_ERROR_CALLBACK   -8   // Callback error
```

### Log Levels
```c
typedef enum {
    LOG_LEVEL_DEBUG = 0,    // Detailed debug information
    LOG_LEVEL_INFO,         // Informational messages
    LOG_LEVEL_WARNING,      // Warning messages
    LOG_LEVEL_ERROR         // Error messages
} LOG_LEVEL;
```

### Module States
```c
typedef enum {
    MODULE_STATE_UNINITIALIZED = 0,
    MODULE_STATE_INITIALIZED,
    MODULE_STATE_STARTED,
    MODULE_STATE_STOPPED,
    MODULE_STATE_ERROR
} MODULE_STATE;
```

### Service States
```c
typedef enum {
    SERVICE_STATE_UNINITIALIZED = 0,
    SERVICE_STATE_INITIALIZED,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_STOPPED,
    SERVICE_STATE_ERROR
} SERVICE_STATE;
```

---

## Callback Function Types

### Module Callbacks
```c
typedef int (*module_init_fn)(void *module_context, void *app_context);
typedef int (*module_start_fn)(void *module_context, void *app_context);
typedef int (*module_stop_fn)(void *module_context, void *app_context);
typedef int (*module_cleanup_fn)(void *module_context, void *app_context);
```

### Service Controller Callbacks
```c
typedef int (*service_init_fn)(void *service_context, void *app_context);
typedef int (*service_start_fn)(void *service_context, void *app_context);
typedef int (*service_stop_fn)(void *service_context, void *app_context);
typedef int (*service_cleanup_fn)(void *service_context, void *app_context);
typedef int (*service_handler_fn)(void *service_context, SERVICE_REQUEST *request, SERVICE_RESPONSE *response);
```

---

## Thread Safety

**Note:** This framework is **not thread-safe** by default. If you need to use it in a multi-threaded environment, you must implement your own synchronization mechanisms (mutexes, locks, etc.) around framework calls.

---

## Memory Management

- The framework uses `malloc`, `calloc`, `realloc`, and `free` for dynamic memory allocation
- Applications are responsible for freeing context structures they allocate
- Modules and services are freed when the application is destroyed
- Always check for NULL returns from creation functions
