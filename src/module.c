#define _POSIX_C_SOURCE 200809L
#include "module.h"
#include "application.h"
#include "framework.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MODULE* module_create(const char *name, int version)
{
    if (!name) {
        framework_log(LOG_LEVEL_ERROR, "Module name cannot be NULL");
        return NULL;
    }
    
    MODULE *mod = (MODULE*)calloc(1, sizeof(MODULE));
    if (!mod) {
        framework_log(LOG_LEVEL_ERROR, "Failed to allocate memory for module");
        return NULL;
    }
    
    strncpy(mod->name, name, sizeof(mod->name) - 1);
    mod->version = version;
    mod->state = MODULE_STATE_UNINITIALIZED;
    mod->context = NULL;
    mod->app = NULL;
    
    mod->init = NULL;
    mod->start = NULL;
    mod->stop = NULL;
    mod->cleanup = NULL;
    
    mod->dependencies = NULL;
    mod->dependency_count = 0;
    
    framework_log(LOG_LEVEL_DEBUG, "Module '%s' (v%d) created", name, version);
    return mod;
}

void module_destroy(MODULE *mod)
{
    if (!mod) return;
    
    framework_log(LOG_LEVEL_DEBUG, "Destroying module '%s'", mod->name);
    
    if (mod->dependencies) {
        for (size_t i = 0; i < mod->dependency_count; i++) {
            free(mod->dependencies[i]);
        }
        free(mod->dependencies);
    }
    
    free(mod);
}

void module_set_init_callback(MODULE *mod, module_init_fn callback)
{
    if (mod) {
        mod->init = callback;
    }
}

void module_set_start_callback(MODULE *mod, module_start_fn callback)
{
    if (mod) {
        mod->start = callback;
    }
}

void module_set_stop_callback(MODULE *mod, module_stop_fn callback)
{
    if (mod) {
        mod->stop = callback;
    }
}

void module_set_cleanup_callback(MODULE *mod, module_cleanup_fn callback)
{
    if (mod) {
        mod->cleanup = callback;
    }
}

int module_add_dependency(MODULE *mod, const char *dep_name)
{
    if (!mod || !dep_name) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    char **new_deps = (char**)realloc(mod->dependencies, 
                                      (mod->dependency_count + 1) * sizeof(char*));
    if (!new_deps) {
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    mod->dependencies = new_deps;
    mod->dependencies[mod->dependency_count] = strdup(dep_name);
    if (!mod->dependencies[mod->dependency_count]) {
        return FRAMEWORK_ERROR_MEMORY;
    }
    
    mod->dependency_count++;
    framework_log(LOG_LEVEL_DEBUG, "Module '%s' depends on '%s'", mod->name, dep_name);
    return FRAMEWORK_SUCCESS;
}

void module_set_context(MODULE *mod, void *context)
{
    if (mod) {
        mod->context = context;
    }
}

void* module_get_context(MODULE *mod)
{
    if (!mod) return NULL;
    return mod->context;
}

int module_initialize(MODULE *mod, void *app_context)
{
    if (!mod) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (mod->state != MODULE_STATE_UNINITIALIZED) {
        framework_log(LOG_LEVEL_WARNING, "Module '%s' already initialized", mod->name);
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Initializing module '%s'", mod->name);
    
    if (mod->init) {
        int result = mod->init(mod->context, app_context);
        if (result != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Module '%s' initialization failed", mod->name);
            mod->state = MODULE_STATE_ERROR;
            return result;
        }
    }
    
    mod->state = MODULE_STATE_INITIALIZED;
    framework_log(LOG_LEVEL_INFO, "Module '%s' initialized", mod->name);
    return FRAMEWORK_SUCCESS;
}

int module_start(MODULE *mod, void *app_context)
{
    if (!mod) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (mod->state != MODULE_STATE_INITIALIZED && mod->state != MODULE_STATE_STOPPED) {
        framework_log(LOG_LEVEL_ERROR, "Module '%s' cannot be started from state %s", 
                     mod->name, module_get_state_string(mod->state));
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Starting module '%s'", mod->name);
    
    if (mod->start) {
        int result = mod->start(mod->context, app_context);
        if (result != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Module '%s' start failed", mod->name);
            mod->state = MODULE_STATE_ERROR;
            return result;
        }
    }
    
    mod->state = MODULE_STATE_STARTED;
    framework_log(LOG_LEVEL_INFO, "Module '%s' started", mod->name);
    return FRAMEWORK_SUCCESS;
}

int module_stop(MODULE *mod, void *app_context)
{
    if (!mod) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    if (mod->state != MODULE_STATE_STARTED) {
        framework_log(LOG_LEVEL_WARNING, "Module '%s' not running", mod->name);
        return FRAMEWORK_ERROR_STATE;
    }
    
    framework_log(LOG_LEVEL_INFO, "Stopping module '%s'", mod->name);
    
    if (mod->stop) {
        int result = mod->stop(mod->context, app_context);
        if (result != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Module '%s' stop failed", mod->name);
            mod->state = MODULE_STATE_ERROR;
            return result;
        }
    }
    
    mod->state = MODULE_STATE_STOPPED;
    framework_log(LOG_LEVEL_INFO, "Module '%s' stopped", mod->name);
    return FRAMEWORK_SUCCESS;
}

int module_cleanup(MODULE *mod, void *app_context)
{
    if (!mod) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    framework_log(LOG_LEVEL_INFO, "Cleaning up module '%s'", mod->name);
    
    if (mod->cleanup) {
        int result = mod->cleanup(mod->context, app_context);
        if (result != FRAMEWORK_SUCCESS) {
            framework_log(LOG_LEVEL_ERROR, "Module '%s' cleanup failed", mod->name);
            return result;
        }
    }
    
    mod->state = MODULE_STATE_UNINITIALIZED;
    framework_log(LOG_LEVEL_INFO, "Module '%s' cleaned up", mod->name);
    return FRAMEWORK_SUCCESS;
}

const char* module_get_state_string(MODULE_STATE state)
{
    switch (state) {
        case MODULE_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case MODULE_STATE_INITIALIZED: return "INITIALIZED";
        case MODULE_STATE_STARTED: return "STARTED";
        case MODULE_STATE_STOPPED: return "STOPPED";
        case MODULE_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/* Application-level module management */
int application_register_module(APPLICATION *app, MODULE *mod)
{
    if (!app || !mod) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    /* Check if module already exists */
    for (size_t i = 0; i < app->module_count; i++) {
        if (strcmp(app->modules[i]->name, mod->name) == 0) {
            framework_log(LOG_LEVEL_ERROR, "Module '%s' already registered", mod->name);
            return FRAMEWORK_ERROR_EXISTS;
        }
    }
    
    /* Resize array if needed */
    if (app->module_count >= app->module_capacity) {
        size_t new_capacity = app->module_capacity * 2;
        MODULE **new_modules = (MODULE**)realloc(app->modules, new_capacity * sizeof(MODULE*));
        if (!new_modules) {
            return FRAMEWORK_ERROR_MEMORY;
        }
        app->modules = new_modules;
        app->module_capacity = new_capacity;
    }
    
    app->modules[app->module_count++] = mod;
    mod->app = app;
    
    framework_log(LOG_LEVEL_INFO, "Module '%s' registered to application '%s'", 
                 mod->name, app->name);
    return FRAMEWORK_SUCCESS;
}

MODULE* application_get_module(APPLICATION *app, const char *name)
{
    if (!app || !name) {
        return NULL;
    }
    
    for (size_t i = 0; i < app->module_count; i++) {
        if (strcmp(app->modules[i]->name, name) == 0) {
            return app->modules[i];
        }
    }
    
    return NULL;
}

int application_initialize_modules(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    framework_log(LOG_LEVEL_INFO, "Initializing %zu modules", app->module_count);
    
    for (size_t i = 0; i < app->module_count; i++) {
        MODULE *mod = app->modules[i];
        
        /* Check dependencies */
        for (size_t j = 0; j < mod->dependency_count; j++) {
            MODULE *dep = application_get_module(app, mod->dependencies[j]);
            if (!dep) {
                framework_log(LOG_LEVEL_ERROR, "Module '%s' dependency '%s' not found", 
                            mod->name, mod->dependencies[j]);
                return FRAMEWORK_ERROR_DEPENDENCY;
            }
            if (dep->state == MODULE_STATE_UNINITIALIZED) {
                framework_log(LOG_LEVEL_ERROR, "Module '%s' dependency '%s' not initialized", 
                            mod->name, mod->dependencies[j]);
                return FRAMEWORK_ERROR_DEPENDENCY;
            }
        }
        
        int result = module_initialize(mod, app->context);
        if (result != FRAMEWORK_SUCCESS) {
            return result;
        }
    }
    
    return FRAMEWORK_SUCCESS;
}

int application_start_modules(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    framework_log(LOG_LEVEL_INFO, "Starting %zu modules", app->module_count);
    
    for (size_t i = 0; i < app->module_count; i++) {
        int result = module_start(app->modules[i], app->context);
        if (result != FRAMEWORK_SUCCESS) {
            return result;
        }
    }
    
    return FRAMEWORK_SUCCESS;
}

int application_stop_modules(APPLICATION *app)
{
    if (!app) {
        return FRAMEWORK_ERROR_NULL_PTR;
    }
    
    framework_log(LOG_LEVEL_INFO, "Stopping %zu modules", app->module_count);
    
    /* Stop modules in reverse order */
    for (int i = (int)app->module_count - 1; i >= 0; i--) {
        module_stop(app->modules[i], app->context);
    }
    
    return FRAMEWORK_SUCCESS;
}
