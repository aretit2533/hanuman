# Equinox Framework Architecture Diagram

## System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                      CLIENT APPLICATIONS                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐   │
│  │ Browser  │  │   curl   │  │  Mobile  │  │  Other Clients   │   │
│  │HTTP/1.1  │  │  HTTP/2  │  │   App    │  │    (REST API)    │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────────┬─────────┘   │
└───────┼─────────────┼─────────────┼───────────────────┼─────────────┘
        │             │             │                   │
        └─────────────┴─────────────┴───────────────────┘
                              │
                    ┌─────────▼──────────┐
                    │   TCP Connection   │
                    │    Port 8080       │
                    └─────────┬──────────┘
                              │
        ┌─────────────────────▼─────────────────────┐
        │      PROTOCOL DETECTION LAYER             │
        │  (recv with MSG_PEEK - 24 bytes)          │
        │                                            │
        │  Is it "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"?│
        └─────────┬──────────────────────┬───────────┘
                  │                      │
         ┌────────▼────────┐    ┌───────▼────────┐
         │   HTTP/1.1      │    │    HTTP/2      │
         │   DETECTED      │    │   DETECTED     │
         └────────┬────────┘    └───────┬────────┘
                  │                     │
    ┌─────────────▼──────────┐ ┌───────▼─────────────┐
    │   HTTP/1.1 PARSER      │ │   HTTP/2 PARSER     │
    │   (Text Protocol)      │ │  (Binary Protocol)  │
    │                        │ │                     │
    │ • Parse request line   │ │ • Parse frames      │
    │ • Parse headers        │ │ • HPACK decode      │
    │ • Read body            │ │ • Stream mgmt       │
    │ • Query string parse   │ │ • Flow control      │
    └─────────────┬──────────┘ └───────┬─────────────┘
                  │                    │
                  └──────────┬─────────┘
                             │
                 ┌───────────▼───────────┐
                 │  UNIFIED REQUEST      │
                 │  (HTTP_REQUEST)       │
                 │                       │
                 │  • method (enum)      │
                 │  • path (string)      │
                 │  • headers (array)    │
                 │  • body (buffer)      │
                 │  • query_params       │
                 └───────────┬───────────┘
                             │
                 ┌───────────▼───────────┐
                 │   ROUTING ENGINE      │
                 │   (http_route.c)      │
                 │                       │
                 │  Match: method + path │
                 └───────────┬───────────┘
                             │
         ┌───────────────────┴───────────────────┐
         │                                       │
    ┌────▼──────┐  ┌───────────┐  ┌────────────▼──┐
    │  GET /    │  │ POST      │  │ PATCH /api/   │
    │  handler  │  │ /api/users│  │ users/:id     │
    └────┬──────┘  └─────┬─────┘  └────────┬──────┘
         │               │                  │
         └───────────────┴──────────────────┘
                         │
                 ┌───────▼────────┐
                 │ ROUTE HANDLER  │
                 │   (User Code)  │
                 │                │
                 │ • Read request │
                 │ • Process data │
                 │ • Build response│
                 └───────┬────────┘
                         │
                 ┌───────▼───────────┐
                 │  UNIFIED RESPONSE │
                 │  (HTTP_RESPONSE)  │
                 │                   │
                 │  • status (enum)  │
                 │  • headers (array)│
                 │  • body (buffer)  │
                 └───────┬───────────┘
                         │
         ┌───────────────┴───────────────────┐
         │                                   │
    ┌────▼────────────┐         ┌───────────▼────────┐
    │ HTTP/1.1        │         │   HTTP/2           │
    │ RESPONSE BUILDER│         │ RESPONSE BUILDER   │
    │                 │         │                    │
    │ • Status line   │         │ • HEADERS frame    │
    │ • Headers       │         │ • HPACK encode     │
    │ • Body          │         │ • DATA frame       │
    │ • Text format   │         │ • Binary format    │
    └────┬────────────┘         └───────────┬────────┘
         │                                  │
         └──────────────┬───────────────────┘
                        │
                ┌───────▼───────┐
                │  send() to    │
                │  TCP Socket   │
                └───────┬───────┘
                        │
                ┌───────▼────────┐
                │     CLIENT     │
                └────────────────┘
```

## HTTP Methods Flow

```
┌─────────────────────────────────────────────────────────┐
│                   HTTP METHODS                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌─────────┐      │
│  │  GET   │  │  POST  │  │  PUT   │  │ PATCH * │      │
│  └───┬────┘  └───┬────┘  └───┬────┘  └────┬────┘      │
│      │           │           │            │            │
│      │ Retrieve  │ Create    │ Replace    │ Partial   │
│      │ resource  │ resource  │ entire     │ update    │
│      │           │           │ resource   │ fields    │
│      │           │           │            │            │
│  ┌────────┐  ┌─────────────────────────────────────┐  │
│  │ DELETE │  │        HEAD, OPTIONS                │  │
│  └───┬────┘  └──────────────┬──────────────────────┘  │
│      │                      │                         │
│      │ Remove               │ Metadata queries        │
│      │ resource             │                         │
│      │                      │                         │
│      └──────────────────────┴─────────────────────┐   │
│                                                    │   │
│               * NEW IN THIS IMPLEMENTATION         │   │
└────────────────────────────────────────────────────┼───┘
                                                     │
                                                     ▼
                                        ┌─────────────────────┐
                                        │  http_server_get()  │
                                        │  http_server_post() │
                                        │  http_server_put()  │
                                        │  http_server_patch()│ *NEW*
                                        │  http_server_delete()│
                                        └─────────────────────┘
```

## PATCH vs PUT Comparison

```
┌─────────────────────────────────────────────────────────────┐
│                    PUT (Full Replacement)                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Client sends:                                              │
│  ┌───────────────────────────────────────────────────┐     │
│  │ PUT /api/users/1                                  │     │
│  │ Content-Type: application/json                    │     │
│  │                                                   │     │
│  │ {                                                 │     │
│  │   "id": 1,                                        │     │
│  │   "name": "John Doe",                             │     │
│  │   "email": "john@example.com",                    │     │
│  │   "role": "admin",                                │     │
│  │   "created": "2025-01-01"                         │     │
│  │ }                                                 │     │
│  └───────────────────────────────────────────────────┘     │
│                                                             │
│  ⚠️  Must include ALL fields                                │
│  ⚠️  Replaces entire resource                               │
│  ✅  Idempotent (same result if called multiple times)      │
│                                                             │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                  PATCH (Partial Update) *NEW*                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Client sends:                                              │
│  ┌───────────────────────────────────────────────────┐     │
│  │ PATCH /api/users/1                                │     │
│  │ Content-Type: application/json                    │     │
│  │                                                   │     │
│  │ {                                                 │     │
│  │   "email": "newemail@example.com"                 │     │
│  │ }                                                 │     │
│  └───────────────────────────────────────────────────┘     │
│                                                             │
│  ✅  Only changed fields needed                             │
│  ✅  Other fields remain unchanged                          │
│  ⚠️  Not idempotent (may have different effects)            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## HTTP/2 Frame Structure

```
┌─────────────────────────────────────────────────────────────┐
│                    HTTP/2 Binary Frame                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────────────────────────────────────────────┐     │
│  │  Byte 0-2:  Length (24-bit)                       │     │
│  ├───────────────────────────────────────────────────┤     │
│  │  Byte 3:    Type                                  │     │
│  │             0x0 = DATA                            │     │
│  │             0x1 = HEADERS                         │     │
│  │             0x2 = PRIORITY                        │     │
│  │             0x3 = RST_STREAM                      │     │
│  │             0x4 = SETTINGS                        │     │
│  │             0x5 = PUSH_PROMISE                    │     │
│  │             0x6 = PING                            │     │
│  │             0x7 = GOAWAY                          │     │
│  │             0x8 = WINDOW_UPDATE                   │     │
│  │             0x9 = CONTINUATION                    │     │
│  ├───────────────────────────────────────────────────┤     │
│  │  Byte 4:    Flags (8-bit)                         │     │
│  │             0x1 = END_STREAM                      │     │
│  │             0x4 = END_HEADERS                     │     │
│  │             0x20 = PRIORITY                       │     │
│  ├───────────────────────────────────────────────────┤     │
│  │  Byte 5-8:  Stream ID (31-bit)                    │     │
│  │             Bit 31 reserved (0)                   │     │
│  ├───────────────────────────────────────────────────┤     │
│  │  Byte 9+:   Payload (variable length)             │     │
│  └───────────────────────────────────────────────────┘     │
│                                                             │
│  Total: 9 bytes header + N bytes payload                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Component Interaction

```
┌──────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                      │
│  ┌────────────────────────────────────────────────┐     │
│  │        User Application Code                   │     │
│  │  • main()                                      │     │
│  │  • Route handlers                              │     │
│  │  • Business logic                              │     │
│  └────────────────┬───────────────────────────────┘     │
└───────────────────┼──────────────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────────────┐
│                 FRAMEWORK LAYER                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ application  │  │    module    │  │   service_   │  │
│  │     .c       │  │     .c       │  │ controller.c │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │              framework.c                        │   │
│  │  • Initialization                               │   │
│  │  • Logging                                      │   │
│  │  • Error handling                               │   │
│  └─────────────────────────────────────────────────┘   │
└───────────────────┬──────────────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────────────┐
│                   HTTP LAYER                             │
│  ┌─────────────┐  ┌─────────────┐  ┌────────────────┐  │
│  │ http_server │  │ http_route  │  │    http2.c     │  │
│  │    .c       │  │    .c       │  │   (NEW)        │  │
│  │             │  │             │  │                │  │
│  │ • Socket    │  │ • Routing   │  │ • Frames       │  │
│  │ • Listen    │  │ • Matching  │  │ • Streams      │  │
│  │ • Accept    │  │ • Dispatch  │  │ • HPACK        │  │
│  └─────────────┘  └─────────────┘  └────────────────┘  │
└───────────────────┬──────────────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────────────┐
│                  OS/NETWORK LAYER                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │              TCP/IP Stack                         │   │
│  │  • BSD Sockets API                                │   │
│  │  • POSIX functions                                │   │
│  └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

## Build System

```
┌──────────────────────────────────────────┐
│             make debug                    │
└───────────────┬──────────────────────────┘
                │
    ┌───────────┼───────────┬──────────────┐
    │           │           │              │
    ▼           ▼           ▼              ▼
┌────────┐ ┌────────┐ ┌─────────┐  ┌──────────┐
│src/*.c │ │src/*.c │ │ src/*.c │  │ src/*.c  │
│  .o    │ │  .o    │ │   .o    │  │   .o     │
└───┬────┘ └───┬────┘ └────┬────┘  └────┬─────┘
    │          │           │            │
    └──────────┴───────────┴────────────┘
               │
               ▼
    ┌──────────────────────┐
    │   lib/libequinox.a   │
    │    (Static Library)   │
    │      163 KB          │
    └──────────┬───────────┘
               │
    ┌──────────┼────────────┬──────────────────┐
    │          │            │                  │
    ▼          ▼            ▼                  ▼
┌────────┐ ┌─────────┐ ┌──────────────┐ ┌────────────────┐
│demo_app│ │ http_   │ │  http2_      │ │   (Your App)   │
│ 63 KB  │ │ server_ │ │  server_app  │ │                │
│        │ │ app     │ │   108 KB     │ │                │
│        │ │ 104 KB  │ │   *NEW*      │ │                │
└────────┘ └─────────┘ └──────────────┘ └────────────────┘
```

## Data Flow Example

```
1. Client Request:
   curl -X PATCH -d '{"email":"new@example.com"}' \
        http://localhost:8080/api/users/1

2. TCP Connection established

3. Protocol Detection:
   → Peek first 24 bytes
   → Not HTTP/2 preface
   → Use HTTP/1.1 parser

4. HTTP/1.1 Parser:
   → Method: PATCH
   → Path: /api/users/1
   → Headers: Content-Type: application/json
   → Body: {"email":"new@example.com"}

5. Create HTTP_REQUEST:
   request->method = HTTP_METHOD_PATCH
   request->path = "/api/users/1"
   request->body = '{"email":"new@example.com"}'

6. Route Matching:
   → Find route: PATCH /api/users/:id
   → Extract id = 1
   → Call handler: handle_patch_user()

7. Handler Execution:
   → Read request->body
   → Update user email
   → Build HTTP_RESPONSE
   response->status = HTTP_STATUS_OK
   response->body = '{"success":true,"method":"PATCH"}'

8. HTTP/1.1 Response Builder:
   HTTP/1.1 200 OK\r\n
   Server: Equinox/1.0 (HTTP/1.1, HTTP/2)\r\n
   Content-Type: application/json\r\n
   Content-Length: 35\r\n
   \r\n
   {"success":true,"method":"PATCH"}

9. Send to client via TCP

10. Close connection (or keep-alive)
```

---

**Legend:**
- ✅ Implemented and tested
- ⚠️  Important note
- *NEW* - Added in this implementation
- → Data flow direction

