# Async HTTP Client Features

The Equinox HTTP client now supports asynchronous requests with callbacks, enabling non-blocking parallel HTTP operations.

## Key Features

### 1. **Async Request Execution**
```c
HTTP_CLIENT_ASYNC_HANDLE* http_client_execute_async(
    HTTP_CLIENT_REQUEST *request,
    HTTP_CLIENT_CALLBACK callback,
    void *user_data
);
```
- Returns immediately (non-blocking)
- Executes request in background thread
- Invokes callback when complete
- Automatically frees request after execution

### 2. **Callback Function**
```c
typedef void (*HTTP_CLIENT_CALLBACK)(HTTP_CLIENT_RESPONSE *response, void *user_data);
```
- Called when request completes
- Receives response object (must be freed by callback)
- Access to user-provided data pointer

### 3. **Handle Management**
```c
int http_client_async_wait(HTTP_CLIENT_ASYNC_HANDLE *handle);
int http_client_async_cancel(HTTP_CLIENT_ASYNC_HANDLE *handle);
int http_client_async_is_complete(HTTP_CLIENT_ASYNC_HANDLE *handle);
```
- Wait for completion
- Cancel running request
- Check completion status

## Usage Patterns

### Pattern 1: Fire and Forget
```c
HTTP_CLIENT_REQUEST *req = http_client_request_create("GET", url);
http_client_execute_async(req, my_callback, user_data);
// Continue immediately, callback will be invoked later
```

### Pattern 2: Wait for Completion
```c
HTTP_CLIENT_ASYNC_HANDLE *handle = http_client_execute_async(req, callback, data);
// Do other work...
http_client_async_wait(handle); // Block until complete
```

### Pattern 3: Parallel Requests
```c
for (int i = 0; i < 10; i++) {
    HTTP_CLIENT_REQUEST *req = http_client_request_create("GET", urls[i]);
    http_client_execute_async(req, callback, &state);
}
// All 10 requests execute in parallel
```

### Pattern 4: Status Monitoring
```c
HTTP_CLIENT_ASYNC_HANDLE *handle = http_client_execute_async(req, callback, data);
while (!http_client_async_is_complete(handle)) {
    // Update UI, do other work
    usleep(100000);
}
http_client_async_wait(handle);
```

## Benefits

✅ **Non-blocking**: Main thread never blocks  
✅ **Parallel execution**: Multiple simultaneous requests  
✅ **Thread-safe**: Built with pthread mutexes  
✅ **Memory managed**: Auto-cleanup after callback  
✅ **Flexible**: Multiple usage patterns  

## Performance

- **Parallel speedup**: 5 requests in ~1 second vs ~5 seconds sequential
- **Dashboard loading**: Fetch multiple APIs simultaneously
- **Background tasks**: Queue processing without blocking UI
- **Real-time updates**: Poll multiple endpoints concurrently

## Examples

See demo applications:
- `async_demo.c` - Basic async patterns
- `async_practical.c` - Real-world use cases

## Thread Safety

- Each async request runs in its own thread
- Response callbacks execute in background thread
- Mutex protection for handle state
- Safe for concurrent requests
