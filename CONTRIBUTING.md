# Contributing to Hanuman Framework

Thanks for your interest in contributing! This document covers everything
you need to build, test, and submit changes to the framework.

## Table of Contents

- [Getting Started](#getting-started)
- [Project Structure](#project-structure)
- [Building the Framework](#building-the-framework)
- [Coding Conventions](#coding-conventions)
- [Testing Your Changes](#testing-your-changes)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Reporting Bugs](#reporting-bugs)

## Getting Started

1. Fork the repository and clone your fork:
   ```bash
   git clone https://github.com/<your-username>/hanuman.git
   cd hanuman
   ```
2. Install dependencies (see [README.md](README.md#dependencies) for the
   full list, or use the [Dockerfile](Dockerfile) for a fully containerized
   toolchain - see below).
3. Build and run the test demos:
   ```bash
   make debug
   make run-server
   ```

## Project Structure

Source is organized by function under `src/` and `src/include/` (mirrored):

```
src/core/       application.c, module.c, service_controller.c, framework.c
src/http/       http_server.c, http_route.c, http2.c, http_client.c
src/kafka/      kafka_client.c
src/mongo/      mongo_client.c
src/json/       json_parser.c
src/realtime/   websocket.c, socketio.c
src/otel/       otel.cpp (OpenTelemetry C++ wrapper - see below)
```

Every module's header folder (`src/include/<module>/`) is automatically
added to the include path by the Makefile, so example and source files use
plain `#include "http_server.h"` regardless of which module owns the header
- please keep following this convention rather than adding new `-I` flags.

Documentation mirrors the same layout under `docs/` (`docs/http/`,
`docs/kafka/`, `docs/json/`, `docs/core/`), plus `docs/API.md`,
`docs/QUICKSTART.md`, and `docs/QUICK_REFERENCE.md` for cross-cutting
reference material.

## Building the Framework

```bash
make debug      # debug build (-g -O0), used by all example/demo targets
make release    # optimized build (-O2) - currently broken, see NOTE below
make clean      # remove build artifacts
make help       # list all available targets
```

> **Known issue:** `make release` currently fails to compile because GCC's
> `-Wformat-truncation`/`-Wstringop-truncation` checks (only triggered at
> `-O2`) are escalated to hard errors by `-Werror` in a few `strncpy()`
> call sites (`src/http/http_server.c`, `src/mongo/mongo_client.c`,
> `src/realtime/websocket.c`). If you'd like to help fix this, it's a good
> first contribution - see the `NOTE` comment above the `release` target in
> the [Makefile](Makefile) for the exact locations. Until then, use
> `make debug` for all local development and testing.

### Building with Docker

If you'd rather not install the toolchain locally (including building
OpenTelemetry C++ from source), use the provided multi-stage
[Dockerfile](Dockerfile):

```bash
docker build -t hanuman-framework .                       # runtime image
docker build --target sdk -t hanuman-framework:sdk .      # dev/SDK image
```

See the "Building with Docker" section in [README.md](README.md) for details.

### OpenTelemetry (src/otel/otel.cpp)

This is the one part of the codebase written in C++ instead of C99, because
it wraps the official `opentelemetry-cpp` SDK. If you're not touching
tracing/metrics/logging, you can safely ignore this file. If you are:
read the ABI note at the top of `src/otel/otel.cpp` and in the Makefile's
`OTEL_CXXFLAGS` comment before making changes - there's a documented,
easy-to-hit ABI mismatch trap (`-DOPENTELEMETRY_STL_VERSION=2017`) that
causes silent segfaults if missed.

## Coding Conventions

- **C99**, compiled with `-Wall -Wextra -Werror` - your code must compile
  warning-free.
- Follow the existing style in the file/module you're editing (brace
  placement, naming, comment style) rather than introducing a new one.
- Public API structs/functions use the existing naming patterns (e.g.
  `HTTP_SERVER`, `http_server_create()`, `FRAMEWORK_SUCCESS`).
- Use `strncpy`/`snprintf` with explicit buffer-size limits and NUL
  termination for all fixed-size buffers - this codebase deals with
  untrusted network input (HTTP requests, Kafka messages, WebSocket frames),
  so buffer safety is a hard requirement, not a style preference.
- New source files go in the appropriate `src/<module>/` folder (or a new
  one, if you're adding a genuinely new subsystem) with a matching header in
  `src/include/<module>/`, and must be added to `SOURCES` in the
  [Makefile](Makefile).
- New example/demo applications go in `examples/` and get their own
  `$(CC) ... -o build/<name>` rule in the Makefile, plus (optionally) a
  `run-<name>` convenience target.

## Testing Your Changes

There's no formal unit test suite yet; correctness is validated by building
and exercising the demo applications:

```bash
make debug
./build/http_server_app &            # then curl/browse http://localhost:8080
./build/kafka_demo                   # requires a running Kafka broker
./build/mongo_demo                   # requires a running MongoDB instance
./build/websocket_echo_server &       # then wscat -c ws://localhost:8080/ws
./build/otel_demo                    # prints traces/metrics/logs to stdout
```

Shell-based integration tests are also provided for specific features:

```bash
./test_concurrent.sh   # EPOLL concurrent connection handling
./test_http2.sh        # HTTP/2 protocol support
```

Please manually verify the demo(s) relevant to your change still build and
run correctly before opening a pull request. If you're fixing a bug, add or
update a demo/example that exercises the fix where practical.

## Submitting a Pull Request

1. Create a topic branch off `main`: `git checkout -b fix/short-description`.
2. Make your changes, following the conventions above.
3. Run `make debug` and confirm it builds warning-free.
4. Update relevant documentation under `docs/<module>/` or `README.md` if
   your change affects a public API, build process, or feature list.
5. Write a clear commit message describing *what* changed and *why*.
6. Open a pull request against `main`, describing the change, how you tested
   it, and any relevant context.

Please keep pull requests focused on a single change - separate unrelated
refactors, formatting changes, and feature work into their own PRs.

## Reporting Bugs

Open an issue on GitHub with:
- Your OS/distro and compiler version (`gcc --version`)
- Exact steps to reproduce (which `make` target, which demo, what input)
- The full error output
- What you expected to happen instead

For security vulnerabilities, please **do not** open a public issue - see
[SECURITY.md](SECURITY.md) for responsible disclosure instructions.

## Code of Conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md). By
participating, you are expected to uphold this code.
