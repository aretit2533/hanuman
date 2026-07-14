/*
 * OpenTelemetry C++ SDK Wrapper
 *
 * Implements the C API declared in otel.h on top of
 * open-telemetry/opentelemetry-cpp (https://github.com/open-telemetry/opentelemetry-cpp).
 *
 * This file is compiled as C++ and exposes a pure C ABI (extern "C") so the
 * rest of the (C99) Hanuman framework can link against it without requiring
 * a C++ toolchain for consumers.
 */

#include "otel.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/* --- OpenTelemetry API -------------------------------------------------- */
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"

/* --- OpenTelemetry SDK --------------------------------------------------- */
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/view/view_registry_factory.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"

/* --- Exporters ------------------------------------------------------------ */
#include "opentelemetry/exporters/ostream/log_record_exporter_factory.h"
#include "opentelemetry/exporters/ostream/metric_exporter_factory.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h"

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace logs_api = opentelemetry::logs;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace resource_sdk = opentelemetry::sdk::resource;
namespace otlp = opentelemetry::exporter::otlp;
namespace nostd = opentelemetry::nostd;

/*
 * ============================================================================
 * Opaque handle definitions
 * ============================================================================
 */
struct _otel_span_
{
    nostd::shared_ptr<trace_api::Span> span;
};

struct _otel_counter_
{
    nostd::unique_ptr<metrics_api::Counter<double>> instrument;
};

struct _otel_histogram_
{
    nostd::unique_ptr<metrics_api::Histogram<double>> instrument;
};

struct _otel_updowncounter_
{
    nostd::unique_ptr<metrics_api::UpDownCounter<double>> instrument;
};

/*
 * ============================================================================
 * Global SDK state
 * ============================================================================
 */
namespace
{

std::atomic<bool> g_initialized{false};
std::string g_service_name = "hanuman-service";

std::shared_ptr<trace_sdk::TracerProvider> g_tracer_provider_sdk;
nostd::shared_ptr<trace_api::Tracer> g_tracer;

std::shared_ptr<metrics_sdk::MeterProvider> g_meter_provider_sdk;
nostd::shared_ptr<metrics_api::Meter> g_meter;

std::shared_ptr<logs_sdk::LoggerProvider> g_logger_provider_sdk;
nostd::shared_ptr<logs_api::Logger> g_logger;

std::mutex g_instrument_mutex;
std::unordered_map<std::string, OTEL_COUNTER *> g_counters;
std::unordered_map<std::string, OTEL_HISTOGRAM *> g_histograms;
std::unordered_map<std::string, OTEL_UPDOWN_COUNTER *> g_updowncounters;

std::string BuildOtlpUrl(const char *base, const char *path)
{
    std::string result = (base && base[0]) ? base : "http://localhost:4318";
    if (!result.empty() && result.back() == '/')
    {
        result.pop_back();
    }
    result += path;
    return result;
}

opentelemetry::common::AttributeValue ToAttributeValue(const OTEL_ATTRIBUTE &attr)
{
    switch (attr.type)
    {
        case OTEL_ATTR_INT64:
            return opentelemetry::common::AttributeValue(attr.value.int64_value);
        case OTEL_ATTR_DOUBLE:
            return opentelemetry::common::AttributeValue(attr.value.double_value);
        case OTEL_ATTR_BOOL:
            return opentelemetry::common::AttributeValue(attr.value.bool_value);
        case OTEL_ATTR_STRING:
        default:
            return opentelemetry::common::AttributeValue(
                nostd::string_view(attr.value.string_value ? attr.value.string_value : ""));
    }
}

using AttrPair = std::pair<nostd::string_view, opentelemetry::common::AttributeValue>;

std::vector<AttrPair> BuildAttributeVector(const OTEL_ATTRIBUTE *attributes, size_t count)
{
    std::vector<AttrPair> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        result.emplace_back(nostd::string_view(attributes[i].key ? attributes[i].key : ""),
                             ToAttributeValue(attributes[i]));
    }
    return result;
}

logs_api::Severity ToApiSeverity(OTEL_SEVERITY severity)
{
    switch (severity)
    {
        case OTEL_SEVERITY_TRACE:
            return logs_api::Severity::kTrace;
        case OTEL_SEVERITY_DEBUG:
            return logs_api::Severity::kDebug;
        case OTEL_SEVERITY_WARN:
            return logs_api::Severity::kWarn;
        case OTEL_SEVERITY_ERROR:
            return logs_api::Severity::kError;
        case OTEL_SEVERITY_FATAL:
            return logs_api::Severity::kFatal;
        case OTEL_SEVERITY_INFO:
        default:
            return logs_api::Severity::kInfo;
    }
}

trace_api::SpanKind ToApiSpanKind(OTEL_SPAN_KIND kind)
{
    switch (kind)
    {
        case OTEL_SPAN_KIND_SERVER:
            return trace_api::SpanKind::kServer;
        case OTEL_SPAN_KIND_CLIENT:
            return trace_api::SpanKind::kClient;
        case OTEL_SPAN_KIND_PRODUCER:
            return trace_api::SpanKind::kProducer;
        case OTEL_SPAN_KIND_CONSUMER:
            return trace_api::SpanKind::kConsumer;
        case OTEL_SPAN_KIND_INTERNAL:
        default:
            return trace_api::SpanKind::kInternal;
    }
}

}  // namespace

/*
 * ============================================================================
 * SDK Lifecycle
 * ============================================================================
 */
extern "C" int otel_init(const OTEL_CONFIG *config)
{
    if (!config || !config->service_name)
    {
        return -1;
    }

    if (g_initialized.load())
    {
        return 0; /* already initialized */
    }

    g_service_name = config->service_name;

    resource_sdk::ResourceAttributes resource_attributes = {
        {resource_sdk::SemanticConventions::kServiceName, config->service_name},
        {resource_sdk::SemanticConventions::kServiceVersion,
         config->service_version ? config->service_version : "0.0.0"},
    };
    resource_sdk::Resource resource = resource_sdk::Resource::Create(resource_attributes);

    /* ---------------------- Tracing ---------------------- */
    {
        std::unique_ptr<trace_sdk::SpanExporter> exporter;
        if (config->exporter == OTEL_EXPORTER_OTLP_HTTP)
        {
            otlp::OtlpHttpExporterOptions options;
            options.url = BuildOtlpUrl(config->otlp_endpoint, "/v1/traces");
            exporter = otlp::OtlpHttpExporterFactory::Create(options);
        }
        else
        {
            exporter = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
        }

        auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
        g_tracer_provider_sdk =
            trace_sdk::TracerProviderFactory::Create(std::move(processor), resource);

        std::shared_ptr<trace_api::TracerProvider> api_provider = g_tracer_provider_sdk;
        trace_api::Provider::SetTracerProvider(api_provider);
        g_tracer = api_provider->GetTracer(g_service_name, config->service_version);
    }

    /* ---------------------- Metrics ---------------------- */
    {
        std::unique_ptr<metrics_sdk::PushMetricExporter> exporter;
        if (config->exporter == OTEL_EXPORTER_OTLP_HTTP)
        {
            otlp::OtlpHttpMetricExporterOptions options;
            options.url = BuildOtlpUrl(config->otlp_endpoint, "/v1/metrics");
            exporter = otlp::OtlpHttpMetricExporterFactory::Create(options);
        }
        else
        {
            exporter = opentelemetry::exporter::metrics::OStreamMetricExporterFactory::Create();
        }

        metrics_sdk::PeriodicExportingMetricReaderOptions reader_options;
        reader_options.export_interval_millis = std::chrono::milliseconds(5000);
        reader_options.export_timeout_millis = std::chrono::milliseconds(2500);

        auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(
            std::move(exporter), reader_options);

        g_meter_provider_sdk =
            metrics_sdk::MeterProviderFactory::Create(metrics_sdk::ViewRegistryFactory::Create(), resource);
        g_meter_provider_sdk->AddMetricReader(std::move(reader));

        std::shared_ptr<metrics_api::MeterProvider> api_provider = g_meter_provider_sdk;
        metrics_api::Provider::SetMeterProvider(api_provider);
        g_meter = api_provider->GetMeter(g_service_name, config->service_version);
    }

    /* ---------------------- Logs ---------------------- */
    {
        std::unique_ptr<logs_sdk::LogRecordExporter> exporter;
        if (config->exporter == OTEL_EXPORTER_OTLP_HTTP)
        {
            otlp::OtlpHttpLogRecordExporterOptions options;
            options.url = BuildOtlpUrl(config->otlp_endpoint, "/v1/logs");
            exporter = otlp::OtlpHttpLogRecordExporterFactory::Create(options);
        }
        else
        {
            exporter = opentelemetry::exporter::logs::OStreamLogRecordExporterFactory::Create();
        }

        auto processor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
        g_logger_provider_sdk =
            logs_sdk::LoggerProviderFactory::Create(std::move(processor), resource);

        std::shared_ptr<logs_api::LoggerProvider> api_provider = g_logger_provider_sdk;
        logs_api::Provider::SetLoggerProvider(api_provider);
        g_logger = api_provider->GetLogger(g_service_name);
    }

    g_initialized.store(true);
    return 0;
}

extern "C" void otel_shutdown(void)
{
    if (!g_initialized.load())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_instrument_mutex);
        for (auto &kv : g_counters) delete kv.second;
        for (auto &kv : g_histograms) delete kv.second;
        for (auto &kv : g_updowncounters) delete kv.second;
        g_counters.clear();
        g_histograms.clear();
        g_updowncounters.clear();
    }

    if (g_tracer_provider_sdk) g_tracer_provider_sdk->ForceFlush();
    if (g_meter_provider_sdk) g_meter_provider_sdk->ForceFlush();
    if (g_logger_provider_sdk) g_logger_provider_sdk->ForceFlush();

    /* Note: each provider's destructor (triggered by .reset() below) already
     * calls Shutdown() internally, so we don't call it explicitly here to
     * avoid a harmless-but-noisy "Shutdown can be invoked only once" log. */

    g_tracer = nostd::shared_ptr<trace_api::Tracer>(nullptr);
    g_meter = nostd::shared_ptr<metrics_api::Meter>(nullptr);
    g_logger = nostd::shared_ptr<logs_api::Logger>(nullptr);

    {
        std::shared_ptr<trace_api::TracerProvider> none;
        trace_api::Provider::SetTracerProvider(none);
    }
    {
        std::shared_ptr<metrics_api::MeterProvider> none;
        metrics_api::Provider::SetMeterProvider(none);
    }
    {
        std::shared_ptr<logs_api::LoggerProvider> none;
        logs_api::Provider::SetLoggerProvider(none);
    }

    g_tracer_provider_sdk.reset();
    g_meter_provider_sdk.reset();
    g_logger_provider_sdk.reset();

    g_initialized.store(false);
}

/*
 * ============================================================================
 * Tracing API
 * ============================================================================
 */
extern "C" OTEL_SPAN *otel_span_start(const char *name, OTEL_SPAN_KIND kind, OTEL_SPAN *parent)
{
    if (!g_tracer || !name)
    {
        return nullptr;
    }

    trace_api::StartSpanOptions options;
    options.kind = ToApiSpanKind(kind);

    if (parent && parent->span)
    {
        opentelemetry::context::Context ctx;
        options.parent = trace_api::SetSpan(ctx, parent->span);
    }

    OTEL_SPAN *result = new OTEL_SPAN();
    result->span = g_tracer->StartSpan(name, options);
    return result;
}

extern "C" void otel_span_set_attribute(OTEL_SPAN *span, const OTEL_ATTRIBUTE *attribute)
{
    if (!span || !span->span || !attribute || !attribute->key)
    {
        return;
    }
    span->span->SetAttribute(attribute->key, ToAttributeValue(*attribute));
}

extern "C" void otel_span_add_event(OTEL_SPAN *span, const char *name)
{
    if (!span || !span->span || !name)
    {
        return;
    }
    span->span->AddEvent(name);
}

extern "C" void otel_span_set_status(OTEL_SPAN *span, OTEL_STATUS_CODE status, const char *description)
{
    if (!span || !span->span)
    {
        return;
    }

    trace_api::StatusCode code = trace_api::StatusCode::kUnset;
    if (status == OTEL_STATUS_OK) code = trace_api::StatusCode::kOk;
    else if (status == OTEL_STATUS_ERROR) code = trace_api::StatusCode::kError;

    span->span->SetStatus(code, description ? description : "");
}

extern "C" void otel_span_end(OTEL_SPAN *span)
{
    if (!span)
    {
        return;
    }
    if (span->span)
    {
        span->span->End();
    }
    delete span;
}

extern "C" int otel_span_get_trace_id(OTEL_SPAN *span, char *buffer, size_t buffer_size)
{
    if (!span || !span->span || !buffer || buffer_size < 33)
    {
        return -1;
    }
    char hex[32];
    span->span->GetContext().trace_id().ToLowerBase16(hex);
    std::memcpy(buffer, hex, 32);
    buffer[32] = '\0';
    return 0;
}

extern "C" int otel_span_get_span_id(OTEL_SPAN *span, char *buffer, size_t buffer_size)
{
    if (!span || !span->span || !buffer || buffer_size < 17)
    {
        return -1;
    }
    char hex[16];
    span->span->GetContext().span_id().ToLowerBase16(hex);
    std::memcpy(buffer, hex, 16);
    buffer[16] = '\0';
    return 0;
}

/*
 * ============================================================================
 * Metrics API
 * ============================================================================
 */
extern "C" OTEL_COUNTER *otel_counter_create(const char *name, const char *description, const char *unit)
{
    if (!g_meter || !name)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_instrument_mutex);
    auto it = g_counters.find(name);
    if (it != g_counters.end())
    {
        return it->second;
    }

    OTEL_COUNTER *counter = new OTEL_COUNTER();
    counter->instrument =
        g_meter->CreateDoubleCounter(name, description ? description : "", unit ? unit : "");
    g_counters[name] = counter;
    return counter;
}

extern "C" void otel_counter_add(OTEL_COUNTER *counter, double value, const OTEL_ATTRIBUTE *attributes,
                                  size_t attribute_count)
{
    if (!counter || !counter->instrument)
    {
        return;
    }
    if (attribute_count == 0 || !attributes)
    {
        counter->instrument->Add(value);
        return;
    }
    auto attrs = BuildAttributeVector(attributes, attribute_count);
    counter->instrument->Add(value, opentelemetry::common::KeyValueIterableView<std::vector<AttrPair>>(attrs));
}

extern "C" OTEL_UPDOWN_COUNTER *otel_updowncounter_create(const char *name, const char *description,
                                                           const char *unit)
{
    if (!g_meter || !name)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_instrument_mutex);
    auto it = g_updowncounters.find(name);
    if (it != g_updowncounters.end())
    {
        return it->second;
    }

    OTEL_UPDOWN_COUNTER *counter = new OTEL_UPDOWN_COUNTER();
    counter->instrument =
        g_meter->CreateDoubleUpDownCounter(name, description ? description : "", unit ? unit : "");
    g_updowncounters[name] = counter;
    return counter;
}

extern "C" void otel_updowncounter_add(OTEL_UPDOWN_COUNTER *counter, double value,
                                       const OTEL_ATTRIBUTE *attributes, size_t attribute_count)
{
    if (!counter || !counter->instrument)
    {
        return;
    }
    if (attribute_count == 0 || !attributes)
    {
        counter->instrument->Add(value);
        return;
    }
    auto attrs = BuildAttributeVector(attributes, attribute_count);
    counter->instrument->Add(value, opentelemetry::common::KeyValueIterableView<std::vector<AttrPair>>(attrs));
}

extern "C" OTEL_HISTOGRAM *otel_histogram_create(const char *name, const char *description, const char *unit)
{
    if (!g_meter || !name)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_instrument_mutex);
    auto it = g_histograms.find(name);
    if (it != g_histograms.end())
    {
        return it->second;
    }

    OTEL_HISTOGRAM *histogram = new OTEL_HISTOGRAM();
    histogram->instrument =
        g_meter->CreateDoubleHistogram(name, description ? description : "", unit ? unit : "");
    g_histograms[name] = histogram;
    return histogram;
}

extern "C" void otel_histogram_record(OTEL_HISTOGRAM *histogram, double value,
                                       const OTEL_ATTRIBUTE *attributes, size_t attribute_count)
{
    if (!histogram || !histogram->instrument)
    {
        return;
    }

    opentelemetry::context::Context context;
    if (attribute_count == 0 || !attributes)
    {
        histogram->instrument->Record(value, context);
        return;
    }
    auto attrs = BuildAttributeVector(attributes, attribute_count);
    histogram->instrument->Record(
        value, opentelemetry::common::KeyValueIterableView<std::vector<AttrPair>>(attrs), context);
}

/*
 * ============================================================================
 * Logging API
 * ============================================================================
 */
extern "C" void otel_log(OTEL_SEVERITY severity, const char *name, const char *message, OTEL_SPAN *span)
{
    if (!g_logger || !message)
    {
        return;
    }

    (void)name; /* reserved for future EventId support */

    if (span && span->span)
    {
        trace_api::SpanContext ctx = span->span->GetContext();
        g_logger->EmitLogRecord(ToApiSeverity(severity), nostd::string_view(message), ctx);
    }
    else
    {
        g_logger->EmitLogRecord(ToApiSeverity(severity), nostd::string_view(message));
    }
}
