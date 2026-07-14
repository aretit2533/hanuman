#ifndef HTTP_ROUTE_H
#define HTTP_ROUTE_H

#include "http_server.h"

/* HTTP Route structure */
struct _http_route_ {
    HTTP_METHOD method;
    char path[512];
    http_route_handler_fn handler;
    void *user_data;
};

typedef struct _http_route_ HTTP_ROUTE;

/* Route management functions */
HTTP_ROUTE* http_route_create(HTTP_METHOD method, const char *path, 
                              http_route_handler_fn handler, void *user_data);
void http_route_destroy(HTTP_ROUTE *route);

int http_route_matches(HTTP_ROUTE *route, HTTP_METHOD method, const char *path);
int http_route_extract_params(HTTP_ROUTE *route, const char *path, HTTP_REQUEST *request);

#endif /* HTTP_ROUTE_H */
