#ifndef FRAMEWORK_H
#define FRAMEWORK_H

/* Main framework header - includes all components */
#include "application.h"
#include "module.h"
#include "service_controller.h"

/* Framework version */
#define FRAMEWORK_VERSION_MAJOR 1
#define FRAMEWORK_VERSION_MINOR 0
#define FRAMEWORK_VERSION_PATCH 0
#define FRAMEWORK_VERSION ((FRAMEWORK_VERSION_MAJOR << 16) | (FRAMEWORK_VERSION_MINOR << 8) | FRAMEWORK_VERSION_PATCH)

/* Framework initialization */
int framework_init(void);
void framework_shutdown(void);

/* Utility functions */
const char* framework_get_version_string(void);
int framework_get_version(void);

/* Error codes */
#define FRAMEWORK_SUCCESS           0
#define FRAMEWORK_ERROR_NULL_PTR    -1
#define FRAMEWORK_ERROR_INVALID     -2
#define FRAMEWORK_ERROR_MEMORY      -3
#define FRAMEWORK_ERROR_NOT_FOUND   -4
#define FRAMEWORK_ERROR_EXISTS      -5
#define FRAMEWORK_ERROR_STATE       -6
#define FRAMEWORK_ERROR_DEPENDENCY  -7
#define FRAMEWORK_ERROR_CALLBACK    -8

/* Logging levels */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} LOG_LEVEL;

/* Logging functions */
void framework_log(LOG_LEVEL level, const char *format, ...);
void framework_set_log_level(LOG_LEVEL level);

#endif /* FRAMEWORK_H */
