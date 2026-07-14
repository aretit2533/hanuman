#define _POSIX_C_SOURCE 200809L
#include "http_route.h"
#include "http_server.h"
#include "framework.h"
#include <stdlib.h>
#include <string.h>

HTTP_ROUTE* http_route_create(HTTP_METHOD method, const char *path,
                              http_route_handler_fn handler, void *user_data)
{
    if (!path || !handler) {
        return NULL;
    }
    
    HTTP_ROUTE *route = (HTTP_ROUTE*)calloc(1, sizeof(HTTP_ROUTE));
    if (!route) {
        return NULL;
    }
    
    route->method = method;
    strncpy(route->path, path, sizeof(route->path) - 1);
    route->handler = handler;
    route->user_data = user_data;
    
    /* Log route registration */
    framework_log(LOG_LEVEL_INFO, "Route registered: %s %s", 
                 http_method_to_string(method), path);
    
    return route;
}

void http_route_destroy(HTTP_ROUTE *route)
{
    if (!route) return;
    free(route);
}

int http_route_matches(HTTP_ROUTE *route, HTTP_METHOD method, const char *path)
{
    if (!route || !path) {
        return 0;
    }
    
    /* Check method */
    if (route->method != method) {
        return 0;
    }
    
    /* Exact match first (optimization for non-parameterized routes) */
    if (strcmp(route->path, path) == 0) {
        return 1;
    }
    
    /* Check if pattern has parameters (contains ':') */
    if (strchr(route->path, ':') == NULL) {
        return 0;  /* No parameters, exact match already failed */
    }
    
    /* Pattern matching with path parameters */
    const char *pattern_ptr = route->path;
    const char *path_ptr = path;
    
    /* Skip leading slash in both if present */
    if (*pattern_ptr == '/') pattern_ptr++;
    if (*path_ptr == '/') path_ptr++;
    
    while (*pattern_ptr && *path_ptr) {
        /* Find end of current segment */
        const char *pattern_end = strchr(pattern_ptr, '/');
        const char *path_end = strchr(path_ptr, '/');
        
        size_t pattern_len = pattern_end ? (size_t)(pattern_end - pattern_ptr) : strlen(pattern_ptr);
        size_t path_len = path_end ? (size_t)(path_end - path_ptr) : strlen(path_ptr);
        
        /* Check if pattern segment is a parameter */
        if (*pattern_ptr == ':') {
            /* Parameter matches any segment */
        } else {
            /* Must match exactly */
            if (pattern_len != path_len || strncmp(pattern_ptr, path_ptr, pattern_len) != 0) {
                return 0;
            }
        }
        
        /* Move to next segment */
        pattern_ptr += pattern_len;
        path_ptr += path_len;
        
        if (*pattern_ptr == '/') pattern_ptr++;
        if (*path_ptr == '/') path_ptr++;
    }
    
    /* Both should be exhausted for a match */
    return (*pattern_ptr == '\0' && *path_ptr == '\0') ? 1 : 0;
}

int http_route_extract_params(HTTP_ROUTE *route, const char *path, HTTP_REQUEST *request)
{
    if (!route || !path || !request) {
        return FRAMEWORK_ERROR_INVALID;
    }
    
    const char *pattern_ptr = route->path;
    const char *path_ptr = path;
    
    /* Skip leading slash in both if present */
    if (*pattern_ptr == '/') pattern_ptr++;
    if (*path_ptr == '/') path_ptr++;
    
    while (*pattern_ptr && *path_ptr) {
        /* Find end of current segment */
        const char *pattern_end = strchr(pattern_ptr, '/');
        const char *path_end = strchr(path_ptr, '/');
        
        size_t pattern_len = pattern_end ? (size_t)(pattern_end - pattern_ptr) : strlen(pattern_ptr);
        size_t path_len = path_end ? (size_t)(path_end - path_ptr) : strlen(path_ptr);
        
        /* Check if pattern segment is a parameter */
        if (*pattern_ptr == ':') {
            /* Extract parameter name and value */
            char param_name[128];
            char param_value[512];
            
            /* Copy parameter name (skip the ':') */
            size_t name_len = pattern_len - 1;
            if (name_len >= sizeof(param_name)) name_len = sizeof(param_name) - 1;
            strncpy(param_name, pattern_ptr + 1, name_len);
            param_name[name_len] = '\0';
            
            /* Copy parameter value */
            if (path_len >= sizeof(param_value)) path_len = sizeof(param_value) - 1;
            strncpy(param_value, path_ptr, path_len);
            param_value[path_len] = '\0';
            
            /* Add parameter */
            int result = http_request_add_path_param(request, param_name, param_value);
            if (result != FRAMEWORK_SUCCESS) {
                return result;
            }
        }
        
        /* Move to next segment */
        pattern_ptr += pattern_len;
        path_ptr += path_len;
        
        if (*pattern_ptr == '/') pattern_ptr++;
        if (*path_ptr == '/') path_ptr++;
    }
    
    return FRAMEWORK_SUCCESS;
}
