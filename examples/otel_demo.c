/*
 * OpenTelemetry Demo
 * Demonstrates distributed tracing, metrics, and structured logging using
 * the Hanuman framework's OpenTelemetry integration (src/otel.cpp, wrapping
 * open-telemetry/opentelemetry-cpp).
 *
 * OpenTelemetry is wired into the application lifecycle using the generic
 * init/cleanup callback registration already provided by application.h -
 * no changes to the framework's core were required.
 */

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "application.h"
#include "framework.h"
#include "otel.h"

/* ---------------------------------------------------------------------- */
/* Wire OpenTelemetry into the application lifecycle via generic callbacks */
/* ---------------------------------------------------------------------- */

static OTEL_CONFIG g_otel_config;

static void otel_init_callback(void *context)
{
    (void)context;
    if (otel_init(&g_otel_config) != 0)
    {
        fprintf(stderr, "Failed to initialize OpenTelemetry SDK\n");
    }
}

static void otel_cleanup_callback(void *context)
{
    (void)context;
    otel_shutdown();
}

/* ---------------------------------------------------------------------- */
/* Simulated request handler instrumented with a trace span, a counter,   */
/* and a histogram (request latency), plus correlated log records.       */
/* ---------------------------------------------------------------------- */

static void handle_request(const char *endpoint, int simulated_status)
{
    static OTEL_COUNTER *request_counter = NULL;
    static OTEL_HISTOGRAM *latency_histogram = NULL;

    if (!request_counter)
    {
        request_counter = otel_counter_create("http.server.requests", "Total HTTP requests", "1");
    }
    if (!latency_histogram)
    {
        latency_histogram =
            otel_histogram_create("http.server.duration", "HTTP request duration", "ms");
    }

    OTEL_SPAN *span = otel_span_start(endpoint, OTEL_SPAN_KIND_SERVER, NULL);

    OTEL_ATTRIBUTE endpoint_attr = {"http.route", OTEL_ATTR_STRING, {.string_value = endpoint}};
    otel_span_set_attribute(span, &endpoint_attr);

    otel_log(OTEL_SEVERITY_INFO, "request.start", "Handling incoming request", span);

    /* Simulate work */
    usleep(10000 + (rand() % 40000));

    OTEL_ATTRIBUTE status_attr = {"http.status_code", OTEL_ATTR_INT64, {.int64_value = simulated_status}};
    otel_span_set_attribute(span, &status_attr);

    OTEL_ATTRIBUTE metric_attrs[] = {
        {"endpoint", OTEL_ATTR_STRING, {.string_value = endpoint}},
        {"status", OTEL_ATTR_INT64, {.int64_value = simulated_status}},
    };
    otel_counter_add(request_counter, 1.0, metric_attrs, 2);
    otel_histogram_record(latency_histogram, 12.5, metric_attrs, 2);

    if (simulated_status >= 500)
    {
        otel_span_add_event(span, "error.occurred");
        otel_span_set_status(span, OTEL_STATUS_ERROR, "internal server error");
        otel_log(OTEL_SEVERITY_ERROR, "request.error", "Request failed", span);
    }
    else
    {
        otel_span_set_status(span, OTEL_STATUS_OK, NULL);
        otel_log(OTEL_SEVERITY_INFO, "request.end", "Request completed successfully", span);
    }

    char trace_id[33];
    char span_id[17];
    if (otel_span_get_trace_id(span, trace_id, sizeof(trace_id)) == 0 &&
        otel_span_get_span_id(span, span_id, sizeof(span_id)) == 0)
    {
        printf("[demo] %s -> status=%d trace_id=%s span_id=%s\n", endpoint, simulated_status,
               trace_id, span_id);
    }

    otel_span_end(span);
}

/* Nested spans demonstrating parent/child relationships */
static void handle_checkout_flow(void)
{
    OTEL_SPAN *root = otel_span_start("checkout", OTEL_SPAN_KIND_SERVER, NULL);

    OTEL_SPAN *validate = otel_span_start("validate_cart", OTEL_SPAN_KIND_INTERNAL, root);
    usleep(5000);
    otel_span_set_status(validate, OTEL_STATUS_OK, NULL);
    otel_span_end(validate);

    OTEL_SPAN *charge = otel_span_start("charge_payment", OTEL_SPAN_KIND_CLIENT, root);
    usleep(15000);
    otel_span_set_status(charge, OTEL_STATUS_OK, NULL);
    otel_span_end(charge);

    otel_span_set_status(root, OTEL_STATUS_OK, NULL);
    otel_span_end(root);

    printf("[demo] checkout -> validate_cart, charge_payment (nested spans)\n");
}

int main(int argc, char *argv[])
{
    const char *exporter_name = "ostream";
    if (argc > 1)
    {
        exporter_name = argv[1];
    }

    g_otel_config.service_name = "otel-demo";
    g_otel_config.service_version = "1.0.0";
    g_otel_config.otlp_endpoint = (argc > 2) ? argv[2] : "http://localhost:4318";

    if (strcmp(exporter_name, "otlp") == 0)
    {
        g_otel_config.exporter = OTEL_EXPORTER_OTLP_HTTP;
        printf("=== OpenTelemetry Demo (OTLP/HTTP exporter -> %s) ===\n\n",
               g_otel_config.otlp_endpoint);
    }
    else
    {
        g_otel_config.exporter = OTEL_EXPORTER_OSTREAM;
        printf("=== OpenTelemetry Demo (console/ostream exporter) ===\n\n");
    }

    framework_init();

    APPLICATION *app = application_create("OtelDemo", 1);
    if (!app)
    {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }

    /* Register OpenTelemetry init/shutdown using the generic framework
     * lifecycle hooks - no application.c changes required. */
    application_register_init_function(app, "otel_init", otel_init_callback, NULL);
    application_register_cleanup_function(app, "otel_shutdown", otel_cleanup_callback, NULL);

    if (application_initialize(app) != FRAMEWORK_SUCCESS)
    {
        fprintf(stderr, "Failed to initialize application\n");
        application_destroy(app);
        framework_shutdown();
        return 1;
    }
    application_start(app);

    printf("Simulating instrumented requests...\n\n");

    handle_request("/api/users", 200);
    handle_request("/api/orders", 200);
    handle_request("/api/payments", 503);
    handle_request("/api/users/42", 200);
    handle_checkout_flow();

    printf("\nFlushing telemetry...\n");

    application_stop(app);
    application_cleanup(app);
    application_destroy(app);
    framework_shutdown();

    printf("Done.\n");
    return 0;
}
