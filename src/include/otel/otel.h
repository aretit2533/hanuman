#ifndef OTEL_H
#define OTEL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ============================================================================
 * OpenTelemetry C API
 * Wraps open-telemetry/opentelemetry-cpp to provide tracing, metrics, and
 * logging instrumentation for the Hanuman framework.
 * ============================================================================
 */

/* Forward declarations (opaque handles) */
typedef struct _otel_span_ OTEL_SPAN;
typedef struct _otel_counter_ OTEL_COUNTER;
typedef struct _otel_histogram_ OTEL_HISTOGRAM;
typedef struct _otel_updowncounter_ OTEL_UPDOWN_COUNTER;

/* Exporter type for traces, metrics, and logs */
typedef enum {
    OTEL_EXPORTER_OSTREAM = 0,   /* Print to stdout (default, no network needed) */
    OTEL_EXPORTER_OTLP_HTTP = 1  /* Send via OTLP/HTTP to a collector */
} OTEL_EXPORTER_TYPE;

/* Span kind (mirrors OpenTelemetry semantic conventions) */
typedef enum {
    OTEL_SPAN_KIND_INTERNAL = 0,
    OTEL_SPAN_KIND_SERVER = 1,
    OTEL_SPAN_KIND_CLIENT = 2,
    OTEL_SPAN_KIND_PRODUCER = 3,
    OTEL_SPAN_KIND_CONSUMER = 4
} OTEL_SPAN_KIND;

/* Span status */
typedef enum {
    OTEL_STATUS_UNSET = 0,
    OTEL_STATUS_OK = 1,
    OTEL_STATUS_ERROR = 2
} OTEL_STATUS_CODE;

/* Log severity (mirrors OpenTelemetry log severity numbers) */
typedef enum {
    OTEL_SEVERITY_TRACE = 1,
    OTEL_SEVERITY_DEBUG = 5,
    OTEL_SEVERITY_INFO = 9,
    OTEL_SEVERITY_WARN = 13,
    OTEL_SEVERITY_ERROR = 17,
    OTEL_SEVERITY_FATAL = 21
} OTEL_SEVERITY;

/* Attribute value (simple key/value used to tag spans, metrics and logs) */
typedef enum {
    OTEL_ATTR_STRING = 0,
    OTEL_ATTR_INT64 = 1,
    OTEL_ATTR_DOUBLE = 2,
    OTEL_ATTR_BOOL = 3
} OTEL_ATTR_TYPE;

typedef struct _otel_attribute_ {
    const char *key;
    OTEL_ATTR_TYPE type;
    union {
        const char *string_value;
        int64_t int64_value;
        double double_value;
        bool bool_value;
    } value;
} OTEL_ATTRIBUTE;

/* Global SDK configuration */
typedef struct _otel_config_ {
    const char *service_name;       /* Resource attribute: service.name */
    const char *service_version;    /* Resource attribute: service.version */
    OTEL_EXPORTER_TYPE exporter;    /* Exporter used for traces/metrics/logs */
    const char *otlp_endpoint;      /* Base URL when exporter == OTEL_EXPORTER_OTLP_HTTP */
} OTEL_CONFIG;

/*
 * ============================================================================
 * SDK Lifecycle
 * ============================================================================
 */

/* Initialize the OpenTelemetry SDK (tracer, meter, and logger providers) */
int otel_init(const OTEL_CONFIG *config);

/* Flush and shutdown all providers. Safe to call multiple times. */
void otel_shutdown(void);

/*
 * ============================================================================
 * Tracing API
 * ============================================================================
 */

/* Start a new span. If parent is NULL, span becomes a root or uses the
 * currently active span (if any) as its parent. Returns NULL on failure. */
OTEL_SPAN* otel_span_start(const char *name, OTEL_SPAN_KIND kind, OTEL_SPAN *parent);

/* Set a single attribute on a span */
void otel_span_set_attribute(OTEL_SPAN *span, const OTEL_ATTRIBUTE *attribute);

/* Add an event (log line attached to the span timeline) */
void otel_span_add_event(OTEL_SPAN *span, const char *name);

/* Set the final status of the span */
void otel_span_set_status(OTEL_SPAN *span, OTEL_STATUS_CODE status, const char *description);

/* End the span (records duration and exports it) */
void otel_span_end(OTEL_SPAN *span);

/* Get the current span's trace ID / span ID as lowercase hex strings.
 * Buffers must be at least 33 and 17 bytes respectively (trace=32 hex
 * chars, span=16 hex chars, plus NUL terminator). Returns 0 on success. */
int otel_span_get_trace_id(OTEL_SPAN *span, char *buffer, size_t buffer_size);
int otel_span_get_span_id(OTEL_SPAN *span, char *buffer, size_t buffer_size);

/*
 * ============================================================================
 * Metrics API
 * ============================================================================
 */

/* Create/lookup a monotonic counter instrument (e.g. request count) */
OTEL_COUNTER* otel_counter_create(const char *name, const char *description, const char *unit);
void otel_counter_add(OTEL_COUNTER *counter, double value, const OTEL_ATTRIBUTE *attributes, size_t attribute_count);

/* Create/lookup an up/down counter instrument (e.g. active connections) */
OTEL_UPDOWN_COUNTER* otel_updowncounter_create(const char *name, const char *description, const char *unit);
void otel_updowncounter_add(OTEL_UPDOWN_COUNTER *counter, double value, const OTEL_ATTRIBUTE *attributes, size_t attribute_count);

/* Create/lookup a histogram instrument (e.g. request latency) */
OTEL_HISTOGRAM* otel_histogram_create(const char *name, const char *description, const char *unit);
void otel_histogram_record(OTEL_HISTOGRAM *histogram, double value, const OTEL_ATTRIBUTE *attributes, size_t attribute_count);

/*
 * ============================================================================
 * Logging API
 * ============================================================================
 */

/* Emit a log record through the OpenTelemetry logs pipeline. If span is
 * non-NULL, the log record is correlated with that span's trace/span IDs. */
void otel_log(OTEL_SEVERITY severity, const char *name, const char *message, OTEL_SPAN *span);

#ifdef __cplusplus
}
#endif

#endif /* OTEL_H */
