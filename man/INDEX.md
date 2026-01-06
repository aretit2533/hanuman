# Equinox Framework Man Pages - Complete Index

## Overview

Complete Linux manual pages documentation for the Equinox Framework C library. All APIs are documented following standard UNIX man page conventions.

## Quick Start

```bash
# View overview
man 7 equinox

# View specific API
man 3 http_server
```

## Complete Man Page List

### Section 7 - Miscellaneous

| Man Page | Description | Size |
|----------|-------------|------|
| **equinox(7)** | Framework overview, architecture, features | 4.8K |

### Section 3 - Library Functions

| Man Page | Description | Functions | Size |
|----------|-------------|-----------|------|
| **application(3)** | Application lifecycle management | 10 functions | 4.3K |
| **module(3)** | Modular component system | 11 functions | 4.0K |
| **service_controller(3)** | Service endpoint management | 11 functions | 5.0K |
| **http_server(3)** | HTTP/1.1 and HTTP/2 server | 14+ functions | 6.3K |
| **http_route(3)** | HTTP route matching and parameters | 4 functions | 4.0K |
| **framework(3)** | Core initialization and logging | 4 functions | 3.8K |

**Total:** 7 man pages covering 50+ functions

## Installation

### Install Framework with Man Pages

```bash
sudo make install
```

This installs:
- Library to `/usr/local/lib/libequinox.a`
- Headers to `/usr/local/include/equinox/`
- Man pages to `/usr/share/man/man3/` and `/usr/share/man/man7/`

### Install Only Man Pages

```bash
cd man
sudo make install
```

### Uninstall

```bash
sudo make uninstall
```

## Function Quick Reference

### Application Functions (application.3)

```
application_create()           - Create application instance
application_destroy()          - Destroy application
application_initialize()       - Initialize application
application_start()            - Start application
application_stop()             - Stop application
application_add_module()       - Register module
application_add_service()      - Register service
application_get_module()       - Get module by name
application_get_name()         - Get application name
application_get_version()      - Get version number
```

### Module Functions (module.3)

```
module_create()               - Create module instance
module_destroy()              - Destroy module
module_set_init()             - Set init callback
module_set_start()            - Set start callback
module_set_stop()             - Set stop callback
module_set_cleanup()          - Set cleanup callback
module_add_dependency()       - Add module dependency
module_init()                 - Initialize module
module_start()                - Start module
module_stop()                 - Stop module
module_cleanup()              - Cleanup module
module_get_name()             - Get module name
module_get_state()            - Get module state
```

### Service Controller Functions (service_controller.3)

```
service_controller_create()                - Create service
service_controller_destroy()               - Destroy service
service_controller_register_operation()    - Register operation handler
service_controller_process_request()       - Process service request
service_controller_set_init()              - Set init callback
service_controller_set_start()             - Set start callback
service_controller_set_stop()              - Set stop callback
service_controller_get_name()              - Get service name
service_request_create()                   - Create request
service_request_destroy()                  - Destroy request
service_response_create()                  - Create response
service_response_destroy()                 - Destroy response
```

### HTTP Server Functions (http_server.3)

```
Server Management:
  http_server_create()                     - Create HTTP server
  http_server_destroy()                    - Destroy server
  http_server_start()                      - Start server
  http_server_stop()                       - Stop server
  http_server_run()                        - Run event loop (blocking)

Route Management:
  http_server_add_route()                  - Register route handler

Request Functions:
  http_request_get_header()                - Get request header
  http_request_get_path_param()            - Get path parameter
  http_request_get_query_param()           - Get query parameter

Response Functions:
  http_response_create()                   - Create response
  http_response_destroy()                  - Destroy response
  http_response_set_status()               - Set HTTP status
  http_response_add_header()               - Add response header
  http_response_set_text()                 - Set text body
  http_response_set_json()                 - Set JSON body
```

### HTTP Route Functions (http_route.3)

```
http_route_create()           - Create route with pattern
http_route_destroy()          - Destroy route
http_route_matches()          - Test if route matches path
http_route_extract_params()   - Extract path parameters
```

### Framework Functions (framework.3)

```
framework_init()              - Initialize framework
framework_shutdown()          - Shutdown framework
framework_log()               - Write log message
framework_set_log_level()     - Set logging level
```

## Man Page Sections

Each man page includes:

1. **NAME** - Brief description
2. **SYNOPSIS** - Function prototypes and includes
3. **DESCRIPTION** - Detailed function descriptions
4. **RETURN VALUE** - Return values and error codes
5. **EXAMPLE** - Working code example
6. **NOTES** - Implementation notes
7. **SEE ALSO** - Cross-references
8. **AUTHOR** - Author information

## Usage Examples

### Basic Search

```bash
# Search for all equinox functions
man -k equinox

# Search for HTTP functions
man -k http_

# Search for specific term
apropos "path parameter"
```

### View Specific Sections

```bash
# View EXAMPLE section
man 3 http_server | sed -n '/^EXAMPLE/,/^[A-Z]/p'

# View SYNOPSIS only
man 3 application | sed -n '/^SYNOPSIS/,/^DESCRIPTION/p'
```

### Cross-Reference Navigation

Man pages include SEE ALSO sections with related pages:

```bash
# Start with overview
man 7 equinox

# Then follow references to specific APIs
man 3 application
man 3 http_server
```

## Documentation Coverage

| Component | Functions | Examples | Man Page |
|-----------|-----------|----------|----------|
| Application | 10 | Yes | ✓ |
| Module | 13 | Yes | ✓ |
| Service | 12 | Yes | ✓ |
| HTTP Server | 14+ | Yes | ✓ |
| HTTP Route | 4 | Yes | ✓ |
| Framework | 4 | Yes | ✓ |
| Overview | - | Yes | ✓ |

**Coverage:** 100% of public APIs documented

## Additional Resources

- **README.md** - Project overview
- **man/README.md** - Man page documentation
- **man/MANPAGE_GUIDE.md** - Detailed usage guide
- **HTTP_SERVER.md** - Extended HTTP server docs
- **PATH_PARAMETERS.md** - Path parameter guide
- **API.md** - Complete API reference

## Standards Compliance

Man pages follow:
- POSIX man page conventions
- GNU groff format
- Standard man macro package
- Linux man-db system

## Accessibility

Man pages are accessible via:
- Terminal: `man` command
- Text: `man -Tascii`
- HTML: `man -Thtml`
- PDF: `man -Tps | ps2pdf`
- Info: `man -Tinfo` (if supported)

## Maintenance

When updating the API:

1. Update corresponding `.3` man page
2. Update cross-references in SEE ALSO
3. Update examples if needed
4. Rebuild and test: `groff -man -wall -z file.3`
5. Reinstall: `sudo make install`

## Support

For issues with man pages:
- Check formatting: `groff -man -wall -z man/*.3`
- Verify installation: `man -w http_server`
- Rebuild index: `sudo mandb`

## License

Man pages are part of the Equinox Framework and follow the same license as the framework itself.

---

**Last Updated:** December 19, 2025  
**Framework Version:** 1.0  
**Man Page Count:** 7 pages, 50+ functions
