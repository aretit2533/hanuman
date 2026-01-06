#include "framework.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

static int framework_initialized = 0;
static LOG_LEVEL current_log_level = LOG_LEVEL_INFO;

int framework_init(void)
{
    if (framework_initialized) {
        return FRAMEWORK_SUCCESS;
    }
    
    framework_log(LOG_LEVEL_INFO, "Initializing Hanuman Framework v%s", framework_get_version_string());
    framework_initialized = 1;
    return FRAMEWORK_SUCCESS;
}

void framework_shutdown(void)
{
    if (!framework_initialized) {
        return;
    }
    
    framework_log(LOG_LEVEL_INFO, "Shutting down Hanuman Framework");
    framework_initialized = 0;
}

const char* framework_get_version_string(void)
{
    static char version_string[32];
    snprintf(version_string, sizeof(version_string), "%d.%d.%d", 
             FRAMEWORK_VERSION_MAJOR, FRAMEWORK_VERSION_MINOR, FRAMEWORK_VERSION_PATCH);
    return version_string;
}

int framework_get_version(void)
{
    return FRAMEWORK_VERSION;
}

void framework_set_log_level(LOG_LEVEL level)
{
    current_log_level = level;
}

static const char* get_log_level_string(LOG_LEVEL level)
{
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "DEBUG";
        case LOG_LEVEL_INFO:    return "INFO";
        case LOG_LEVEL_WARNING: return "WARNING";
        case LOG_LEVEL_ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

void framework_log(LOG_LEVEL level, const char *format, ...)
{
    if (level < current_log_level) {
        return;
    }
    
    time_t now;
    time(&now);
    char time_str[64];
    struct tm *tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(stderr, "[%s] [%s] ", time_str, get_log_level_string(level));
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}
