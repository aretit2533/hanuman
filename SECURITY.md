# Security Policy

## Supported Versions

Hanuman Framework is currently pre-1.0 and under active development. Only
the latest commit on the `main` branch is supported with security fixes.

| Version         | Supported          |
| ---------------- | ------------------ |
| `main` (latest)  | :white_check_mark: |
| Older commits/tags | :x:               |

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Instead, report it privately using one of the following methods:

1. **Preferred:** Open a [GitHub Security Advisory](https://github.com/aretit2533/hanuman/security/advisories/new)
   for this repository (Security tab -> "Report a vulnerability").

Please include as much detail as possible:

- A description of the vulnerability and its potential impact
- Steps to reproduce (a minimal example is very helpful - e.g. a crafted
  HTTP request, WebSocket frame, JSON payload, or Kafka message)
- The affected file(s)/function(s), if known
- Any suggested mitigation or fix

### What to expect

- **Acknowledgement**: within 3 business days of your report.
- **Status update**: within 10 business days, including an initial
  assessment of severity and expected timeline for a fix.
- **Disclosure**: we will coordinate with you on a disclosure timeline once
  a fix is available. We ask that you do not publicly disclose the issue
  until a fix has been released.

## Scope

This project is a C framework that directly parses untrusted network input
in several places, so the following categories are all in scope:

- **Memory safety**: buffer overflows/underflows, out-of-bounds reads/writes,
  use-after-free, double-free (e.g. in `src/http/`, `src/realtime/websocket.c`,
  `src/realtime/socketio.c`, `src/json/json_parser.c`)
- **Parsing vulnerabilities**: malformed HTTP/1.1 or HTTP/2 requests,
  WebSocket frames, Socket.IO/Engine.IO packets, JSON payloads, or Kafka
  messages that cause crashes, hangs, or memory corruption
- **Authentication/authorization**: issues in Kafka SASL/SCRAM authentication
  (`src/kafka/kafka_client.c`) or MongoDB connection/auth handling
  (`src/mongo/mongo_client.c`)
- **TLS/SSL misconfiguration**: certificate validation bypass or weak
  defaults in the HTTP client (`src/http/http_client.c`), Kafka SSL/SASL
  paths, or MongoDB TLS connections
- **Denial of service**: unbounded memory/CPU consumption from crafted input
  (e.g. WebSocket fragmentation, HTTP/2 frame handling, JSON nesting)
- **Injection**: anywhere untrusted input reaches a format string, shell
  command, or query construction without proper handling

Out of scope: issues that require an attacker to already have local shell
access to the host running the framework, or vulnerabilities purely in
third-party dependencies (librdkafka, MongoDB C driver, OpenSSL,
opentelemetry-cpp) that should be reported upstream - though we'd still
appreciate a heads-up so we can track and update our pinned versions.

## Security Practices in This Codebase

- All example/demo code and library code is compiled with
  `-Wall -Wextra -Werror`, which catches several classes of buffer-safety
  issues (e.g. `-Wstringop-truncation`) at compile time.
- Fixed-size buffers use `strncpy`/`snprintf` with explicit size limits and
  NUL termination throughout the HTTP, WebSocket, Socket.IO, and JSON code.
- The HTTP client (`src/http/http_client.c`) verifies SSL certificates by
  default (`http_client_request_set_verify_ssl`) and only disables
  verification when explicitly requested by the caller.
- Static file serving (`http_server_add_static_path`) includes directory
  traversal protection.

If you find a gap in any of the above, that's exactly the kind of report
we're looking for - please report it as described above.

## Disclosure Credit

With your permission, we're happy to credit you by name/handle in the
release notes or advisory once a fix is published.
