/*
 * JSON Parser with Schema Example
 * Demonstrates schema-based JSON parsing in Equinox Framework
 */

#include "application.h"
#include "framework.h"
#include "json_parser.h"
#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Define data structures */
typedef struct {
    char street[128];
    char city[64];
    char state[32];
    int zip_code;
} Address;

typedef struct {
    int id;
    char name[128];
    char email[128];
    int age;
    int is_active;
    double balance;
    Address address;
} User;

typedef struct {
    int order_id;
    int user_id;
    char product[128];
    int quantity;
    double price;
    double total;
} Order;

/* Define schemas using macros */
JSON_SCHEMA_DEFINE(address_schema, Address,
    JSON_SCHEMA_FIELD_STRING(Address, street, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(Address, city, 64, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(Address, state, 32, 0),
    JSON_SCHEMA_FIELD_INT(Address, zip_code, 0)
);

JSON_SCHEMA_DEFINE(user_schema, User,
    JSON_SCHEMA_FIELD_INT(User, id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(User, name, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(User, email, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_INT(User, age, 0),
    JSON_SCHEMA_FIELD_BOOL(User, is_active, 0),
    JSON_SCHEMA_FIELD_DOUBLE(User, balance, 0),
    JSON_SCHEMA_FIELD_OBJECT(User, address, &address_schema, 0)
);

JSON_SCHEMA_DEFINE(order_schema, Order,
    JSON_SCHEMA_FIELD_INT(Order, order_id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_INT(Order, user_id, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_STRING(Order, product, 128, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_INT(Order, quantity, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_DOUBLE(Order, price, SCHEMA_FLAG_REQUIRED),
    JSON_SCHEMA_FIELD_DOUBLE(Order, total, 0)
);

/* HTTP Handler: POST /api/users - Create user with schema validation */
void handle_create_user(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    printf("\n=== Received POST /api/users ===\n");
    printf("Body: %s\n", request->body);
    
    User user;
    JSON_VALIDATION_RESULT result;
    
    /* Parse and validate JSON with schema */
    if (json_parse_and_validate(request->body, &user_schema, &user, &result) == 0) {
        printf("\n✓ User created successfully:\n");
        printf("  ID: %d\n", user.id);
        printf("  Name: %s\n", user.name);
        printf("  Email: %s\n", user.email);
        printf("  Age: %d\n", user.age);
        printf("  Active: %s\n", user.is_active ? "Yes" : "No");
        printf("  Balance: $%.2f\n", user.balance);
        printf("  Address: %s, %s, %s %d\n", 
               user.address.street, user.address.city, 
               user.address.state, user.address.zip_code);
        
        /* Build response JSON */
        JSON_BUILDER *builder = json_builder_create(1024);
        json_builder_start_object(builder);
        json_builder_add_string(builder, "status", "success");
        json_builder_add_int(builder, "user_id", user.id);
        json_builder_add_string(builder, "name", user.name);
        json_builder_end_object(builder);
        
        http_response_set_status(response, HTTP_STATUS_CREATED);
        http_response_set_json(response, json_builder_get_string(builder));
        
        json_builder_destroy(builder);
    } else {
        printf("\n✗ Validation failed:\n");
        printf("  Error: %s\n", result.error_message);
        if (result.error_field) {
            printf("  Field: %s\n", result.error_field);
        }
        
        /* Build error response */
        char error_json[1024];
        snprintf(error_json, sizeof(error_json),
                "{\"error\":\"%s\",\"field\":\"%s\"}",
                result.error_message,
                result.error_field ? result.error_field : "unknown");
        
        http_response_set_status(response, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(response, error_json);
    }
}

/* HTTP Handler: POST /api/orders */
void handle_create_order(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)user_data;
    
    printf("\n=== Received POST /api/orders ===\n");
    
    Order order;
    JSON_VALIDATION_RESULT result;
    
    if (json_parse_and_validate(request->body, &order_schema, &order, &result) == 0) {
        /* Calculate total if not provided */
        if (order.total == 0) {
            order.total = order.price * order.quantity;
        }
        
        printf("✓ Order created:\n");
        printf("  Order ID: %d\n", order.order_id);
        printf("  User ID: %d\n", order.user_id);
        printf("  Product: %s\n", order.product);
        printf("  Quantity: %d\n", order.quantity);
        printf("  Price: $%.2f\n", order.price);
        printf("  Total: $%.2f\n", order.total);
        
        /* Serialize order back to JSON */
        char response_json[1024];
        json_serialize(&order, &order_schema, response_json, sizeof(response_json));
        
        http_response_set_status(response, HTTP_STATUS_CREATED);
        http_response_set_json(response, response_json);
    } else {
        printf("✗ Validation failed: %s\n", result.error_message);
        
        char error_json[1024];
        snprintf(error_json, sizeof(error_json),
                "{\"error\":\"%s\"}", result.error_message);
        
        http_response_set_status(response, HTTP_STATUS_BAD_REQUEST);
        http_response_set_json(response, error_json);
    }
}

/* HTTP Handler: GET /api/demo */
void handle_demo(HTTP_REQUEST *request, HTTP_RESPONSE *response, void *user_data)
{
    (void)request;
    (void)user_data;
    
    const char *html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <title>JSON Schema Parser Demo</title>\n"
        "  <style>\n"
        "    body { font-family: Arial, sans-serif; margin: 40px; }\n"
        "    .example { background: #f4f4f4; padding: 15px; margin: 10px 0; border-radius: 5px; }\n"
        "    pre { background: #282c34; color: #abb2bf; padding: 15px; border-radius: 5px; overflow-x: auto; }\n"
        "    button { background: #007bff; color: white; border: none; padding: 10px 20px; "
        "             border-radius: 5px; cursor: pointer; margin: 5px; }\n"
        "    button:hover { background: #0056b3; }\n"
        "    .result { margin-top: 10px; padding: 10px; border-radius: 5px; }\n"
        "    .success { background: #d4edda; color: #155724; }\n"
        "    .error { background: #f8d7da; color: #721c24; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>🔧 JSON Schema Parser Demo</h1>\n"
        "  <p>This demo shows schema-based JSON parsing with validation.</p>\n"
        "\n"
        "  <div class='example'>\n"
        "    <h2>Create User (POST /api/users)</h2>\n"
        "    <pre id='user-json'>{\n"
        "  \"id\": 1,\n"
        "  \"name\": \"Alice Johnson\",\n"
        "  \"email\": \"alice@example.com\",\n"
        "  \"age\": 30,\n"
        "  \"is_active\": true,\n"
        "  \"balance\": 1250.75,\n"
        "  \"address\": {\n"
        "    \"street\": \"123 Main St\",\n"
        "    \"city\": \"San Francisco\",\n"
        "    \"state\": \"CA\",\n"
        "    \"zip_code\": 94102\n"
        "  }\n"
        "}</pre>\n"
        "    <button onclick='testUser(true)'>✓ Test Valid User</button>\n"
        "    <button onclick='testUser(false)'>✗ Test Invalid User</button>\n"
        "    <div id='user-result'></div>\n"
        "  </div>\n"
        "\n"
        "  <div class='example'>\n"
        "    <h2>Create Order (POST /api/orders)</h2>\n"
        "    <pre id='order-json'>{\n"
        "  \"order_id\": 1001,\n"
        "  \"user_id\": 1,\n"
        "  \"product\": \"Laptop\",\n"
        "  \"quantity\": 2,\n"
        "  \"price\": 999.99\n"
        "}</pre>\n"
        "    <button onclick='testOrder(true)'>✓ Test Valid Order</button>\n"
        "    <button onclick='testOrder(false)'>✗ Test Invalid Order</button>\n"
        "    <div id='order-result'></div>\n"
        "  </div>\n"
        "\n"
        "  <script>\n"
        "    async function testUser(valid) {\n"
        "      const data = valid ? {\n"
        "        id: 1, name: 'Alice Johnson', email: 'alice@example.com',\n"
        "        age: 30, is_active: true, balance: 1250.75,\n"
        "        address: { street: '123 Main St', city: 'San Francisco', state: 'CA', zip_code: 94102 }\n"
        "      } : { name: 'Invalid User' }; // Missing required fields\n"
        "      \n"
        "      try {\n"
        "        const response = await fetch('/api/users', {\n"
        "          method: 'POST',\n"
        "          headers: { 'Content-Type': 'application/json' },\n"
        "          body: JSON.stringify(data)\n"
        "        });\n"
        "        const result = await response.json();\n"
        "        const div = document.getElementById('user-result');\n"
        "        div.className = 'result ' + (response.ok ? 'success' : 'error');\n"
        "        div.innerHTML = '<strong>' + (response.ok ? 'Success!' : 'Error!') + '</strong><br>' +\n"
        "                       'Response: <pre>' + JSON.stringify(result, null, 2) + '</pre>';\n"
        "      } catch(e) {\n"
        "        document.getElementById('user-result').innerHTML = 'Error: ' + e;\n"
        "      }\n"
        "    }\n"
        "    \n"
        "    async function testOrder(valid) {\n"
        "      const data = valid ? {\n"
        "        order_id: 1001, user_id: 1, product: 'Laptop', quantity: 2, price: 999.99\n"
        "      } : { order_id: 1002 }; // Missing required fields\n"
        "      \n"
        "      try {\n"
        "        const response = await fetch('/api/orders', {\n"
        "          method: 'POST',\n"
        "          headers: { 'Content-Type': 'application/json' },\n"
        "          body: JSON.stringify(data)\n"
        "        });\n"
        "        const result = await response.json();\n"
        "        const div = document.getElementById('order-result');\n"
        "        div.className = 'result ' + (response.ok ? 'success' : 'error');\n"
        "        div.innerHTML = '<strong>' + (response.ok ? 'Success!' : 'Error!') + '</strong><br>' +\n"
        "                       'Response: <pre>' + JSON.stringify(result, null, 2) + '</pre>';\n"
        "      } catch(e) {\n"
        "        document.getElementById('order-result').innerHTML = 'Error: ' + e;\n"
        "      }\n"
        "    }\n"
        "  </script>\n"
        "</body>\n"
        "</html>";
    
    http_response_set_status(response, HTTP_STATUS_OK);
    http_response_add_header(response, "Content-Type", "text/html; charset=UTF-8");
    http_response_set_body(response, html, strlen(html));
}

int main(void)
{
    framework_init();
    
    /* Create application */
    APPLICATION *app = application_create("JSON Schema Demo", 1);
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Create HTTP server */
    HTTP_SERVER *server = http_server_create("0.0.0.0", 8080);
    if (!server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        application_destroy(app);
        return 1;
    }
    
    /* Register routes */
    http_server_add_route(server, HTTP_METHOD_GET, "/api/demo", handle_demo, NULL);
    http_server_add_route(server, HTTP_METHOD_POST, "/api/users", handle_create_user, NULL);
    http_server_add_route(server, HTTP_METHOD_POST, "/api/orders", handle_create_order, NULL);
    
    /* Register with application */
    application_set_http_server(app, server);
    
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  JSON Schema Parser Demo                                 ║\n");
    printf("║  ─────────────────────────────────────────────────────── ║\n");
    printf("║  Open: http://localhost:8080/api/demo                    ║\n");
    printf("║  ─────────────────────────────────────────────────────── ║\n");
    printf("║  Features:                                               ║\n");
    printf("║    • Schema-based JSON parsing                           ║\n");
    printf("║    • Automatic validation                                ║\n");
    printf("║    • Type checking                                       ║\n");
    printf("║    • Nested objects                                      ║\n");
    printf("║    • JSON serialization                                  ║\n");
    printf("║  ─────────────────────────────────────────────────────── ║\n");
    printf("║  Test with curl:                                         ║\n");
    printf("║    curl -X POST http://localhost:8080/api/users \\        ║\n");
    printf("║      -H 'Content-Type: application/json' \\               ║\n");
    printf("║      -d '{\"id\":1,\"name\":\"Alice\", ...}'                   ║\n");
    printf("║  ─────────────────────────────────────────────────────── ║\n");
    printf("║  Press Ctrl+C to stop                                    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    /* Run application */
    application_run(app);
    
    /* Cleanup */
    http_server_destroy(server);
    application_destroy(app);
    framework_shutdown();
    
    return 0;
}
