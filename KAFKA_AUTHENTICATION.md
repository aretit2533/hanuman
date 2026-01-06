# Kafka Authentication Guide

This guide covers all authentication mechanisms supported by the Equinox Kafka client.

## Overview

The framework supports multiple SASL (Simple Authentication and Security Layer) authentication mechanisms to secure communication with Kafka brokers.

## Supported Authentication Types

| Type | Enum | Description | Use Case |
|------|------|-------------|----------|
| None | `KAFKA_AUTH_NONE` | No authentication | Development only |
| SASL/PLAIN | `KAFKA_AUTH_SASL_PLAIN` | Username/password | Simple authentication |
| SASL/SCRAM-SHA-256 | `KAFKA_AUTH_SASL_SCRAM_SHA256` | Hash-based auth | Recommended for production |
| SASL/SCRAM-SHA-512 | `KAFKA_AUTH_SASL_SCRAM_SHA512` | Stronger hash | High security requirements |
| SASL/GSSAPI | `KAFKA_AUTH_SASL_GSSAPI` | Kerberos | Enterprise environments |
| SASL/OAUTHBEARER | `KAFKA_AUTH_SASL_OAUTHBEARER` | OAuth 2.0 | Modern cloud environments |

## Quick Start Examples

### 1. SASL/PLAIN (Simple Username/Password)

```c
KAFKA_CONSUMER_CONFIG config;
kafka_consumer_config_default(&config, "broker:9092", "my-group");

config.auth_type = KAFKA_AUTH_SASL_PLAIN;
strcpy(config.sasl_username, "myuser");
strcpy(config.sasl_password, "mypassword");

kafka_consumer_register(kafka, "my-topic", &config, handle_message, NULL);
```

### 2. SASL/SCRAM-SHA-256 (Recommended)

```c
KAFKA_CONSUMER_CONFIG config;
kafka_consumer_config_default(&config, "broker:9092", "my-group");

config.auth_type = KAFKA_AUTH_SASL_SCRAM_SHA256;
strcpy(config.sasl_username, "admin");
strcpy(config.sasl_password, "secure-password");

kafka_consumer_register(kafka, "my-topic", &config, handle_message, NULL);
```

### 3. SASL/SCRAM-SHA-512 (Most Secure)

```c
config.auth_type = KAFKA_AUTH_SASL_SCRAM_SHA512;
strcpy(config.sasl_username, "admin");
strcpy(config.sasl_password, "very-secure-password");
```

### 4. Kerberos/GSSAPI (Enterprise)

```c
KAFKA_CONSUMER_CONFIG config;
kafka_consumer_config_default(&config, "broker:9092", "my-group");

config.auth_type = KAFKA_AUTH_SASL_GSSAPI;
strcpy(config.kerberos_service_name, "kafka");
strcpy(config.kerberos_principal, "kafkauser@EXAMPLE.COM");
strcpy(config.kerberos_keytab, "/etc/security/keytabs/kafka.keytab");

kafka_consumer_register(kafka, "my-topic", &config, handle_message, NULL);
```

## Security Protocol Matrix

The framework automatically selects the correct security protocol:

| SSL Enabled | Auth Enabled | Protocol | Port |
|-------------|--------------|----------|------|
| No | No | `plaintext` | 9092 |
| No | Yes | `sasl_plaintext` | 9092 |
| Yes | No | `ssl` | 9093 |
| Yes | Yes | `sasl_ssl` | 9093 |

## Production Configuration (SSL + SCRAM)

Most secure recommended configuration:

```c
KAFKA_CONSUMER_CONFIG config;
kafka_consumer_config_default(&config, "broker:9093", "secure-group");

// Enable SSL/TLS
config.enable_ssl = 1;
strcpy(config.ssl_ca_location, "/etc/kafka/ssl/ca-cert.pem");
strcpy(config.ssl_cert_location, "/etc/kafka/ssl/client-cert.pem");
strcpy(config.ssl_key_location, "/etc/kafka/ssl/client-key.pem");

// Enable SCRAM-SHA-256 authentication
config.auth_type = KAFKA_AUTH_SASL_SCRAM_SHA256;
strcpy(config.sasl_username, "production-user");
strcpy(config.sasl_password, getenv("KAFKA_PASSWORD")); // From environment

kafka_consumer_register(kafka, "critical-topic", &config, handle_message, NULL);
```

## Producer Authentication

Producers use the same authentication configuration:

```c
KAFKA_PRODUCER_CONFIG producer_config;
kafka_producer_config_default(&producer_config, "broker:9093");

// SSL
producer_config.enable_ssl = 1;
strcpy(producer_config.ssl_ca_location, "/etc/kafka/ssl/ca-cert.pem");

// Authentication
producer_config.auth_type = KAFKA_AUTH_SASL_SCRAM_SHA256;
strcpy(producer_config.sasl_username, "producer-user");
strcpy(producer_config.sasl_password, getenv("KAFKA_PASSWORD"));

kafka_producer_init(kafka, &producer_config);
```

## Configuration Fields

### Consumer Configuration
```c
typedef struct _kafka_consumer_config_ {
    // ... basic config ...
    
    // Authentication
    KAFKA_AUTH_TYPE auth_type;      // Authentication mechanism
    char sasl_username[256];        // SASL username
    char sasl_password[256];        // SASL password
    char kerberos_service_name[64]; // Kerberos service (GSSAPI only)
    char kerberos_principal[256];   // Kerberos principal (GSSAPI only)
    char kerberos_keytab[512];      // Kerberos keytab path (GSSAPI only)
} KAFKA_CONSUMER_CONFIG;
```

## Running the Demo

```bash
# Build
make debug

# Run with SASL/PLAIN
./build/kafka_auth_demo localhost:9092 myuser mypassword plain

# Run with SCRAM-SHA-256
./build/kafka_auth_demo localhost:9092 admin secret123 scram256

# Run with SCRAM-SHA-512
./build/kafka_auth_demo localhost:9092 admin secret123 scram512
```

## Best Practices

1. **Never hardcode credentials** - Use environment variables or secret management
2. **Use SCRAM over PLAIN** - SCRAM-SHA-256/512 is more secure
3. **Combine with SSL** - Always use SSL/TLS in production
4. **Rotate credentials** - Implement regular password rotation
5. **Least privilege** - Grant minimum required permissions
6. **Secure storage** - Store credentials in encrypted vaults
7. **Audit logging** - Monitor authentication attempts
8. **Network security** - Use VPNs or private networks

## Environment Variable Example

```bash
#!/bin/bash
# kafka_env.sh
export KAFKA_BROKER="broker.example.com:9093"
export KAFKA_USERNAME="production-user"
export KAFKA_PASSWORD="$(cat /run/secrets/kafka_password)"

./my_kafka_app
```

```c
// In your application
const char *broker = getenv("KAFKA_BROKER");
const char *username = getenv("KAFKA_USERNAME");
const char *password = getenv("KAFKA_PASSWORD");

config.auth_type = KAFKA_AUTH_SASL_SCRAM_SHA256;
strcpy(config.sasl_username, username);
strcpy(config.sasl_password, password);
```

## Troubleshooting

### Authentication Failed
```
Error: Failed to create Kafka consumer: Authentication failed
```
**Solutions:**
- Verify username and password are correct
- Check authentication mechanism matches broker configuration
- Ensure broker allows the SASL mechanism
- Verify network connectivity to broker

### SSL + Auth Issues
```
Error: Failed to set security.protocol
```
**Solutions:**
- Ensure CA certificate is valid and accessible
- Check SSL port (usually 9093 for SASL_SSL)
- Verify broker supports combined SSL + SASL

### Kerberos Issues
```
Error: GSSAPI authentication failed
```
**Solutions:**
- Verify Kerberos principal and keytab
- Check service name matches broker configuration
- Ensure system has valid Kerberos ticket
- Verify DNS resolution for realm

## See Also

- [Kafka Integration Guide](KAFKA_INTEGRATION.md)
- [SSL/TLS Configuration](KAFKA_INTEGRATION.md#ssltls-configuration)
- [Multi-Topic Consumers](KAFKA_INTEGRATION.md#multiple-topic-handlers)
