#ifndef MODULE_H
#define MODULE_H

#include <stddef.h>

/* Forward declaration */
typedef struct _application_ APPLICATION;

/* Module state */
typedef enum {
    MODULE_STATE_UNINITIALIZED = 0,
    MODULE_STATE_INITIALIZED,
    MODULE_STATE_STARTED,
    MODULE_STATE_STOPPED,
    MODULE_STATE_ERROR
} MODULE_STATE;

/* Module callbacks */
typedef int (*module_init_fn)(void *module_context, void *app_context);
typedef int (*module_start_fn)(void *module_context, void *app_context);
typedef int (*module_stop_fn)(void *module_context, void *app_context);
typedef int (*module_cleanup_fn)(void *module_context, void *app_context);

/* Module structure */
typedef struct _module_
{
    char name[256];
    int version;
    MODULE_STATE state;
    
    void *context;  /* Module-specific context data */
    
    /* Lifecycle callbacks */
    module_init_fn init;
    module_start_fn start;
    module_stop_fn stop;
    module_cleanup_fn cleanup;
    
    /* Dependencies */
    char **dependencies;
    size_t dependency_count;
    
    /* Reference to parent application */
    APPLICATION *app;
} MODULE;

/* Module registration and lifecycle */
MODULE* module_create(const char *name, int version);
void module_destroy(MODULE *mod);

void module_set_init_callback(MODULE *mod, module_init_fn callback);
void module_set_start_callback(MODULE *mod, module_start_fn callback);
void module_set_stop_callback(MODULE *mod, module_stop_fn callback);
void module_set_cleanup_callback(MODULE *mod, module_cleanup_fn callback);

int module_add_dependency(MODULE *mod, const char *dep_name);
void module_set_context(MODULE *mod, void *context);
void* module_get_context(MODULE *mod);

int module_initialize(MODULE *mod, void *app_context);
int module_start(MODULE *mod, void *app_context);
int module_stop(MODULE *mod, void *app_context);
int module_cleanup(MODULE *mod, void *app_context);

const char* module_get_state_string(MODULE_STATE state);

/* Application-level module management */
int application_register_module(APPLICATION *app, MODULE *mod);
MODULE* application_get_module(APPLICATION *app, const char *name);
int application_initialize_modules(APPLICATION *app);
int application_start_modules(APPLICATION *app);
int application_stop_modules(APPLICATION *app);

#endif /* MODULE_H */
