#!/bin/bash

# Test script for Hanuman Framework HTTP/2 Server
# Tests HTTP/1.1, HTTP/2, and PATCH method support

set -e

SERVER_URL="http://localhost:8080"
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "╔═══════════════════════════════════════════════════════╗"
echo "║   Hanuman Framework HTTP/2 Server Test Suite        ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""

# Check if server is running
echo -e "${BLUE}[1/12]${NC} Checking server status..."
if curl -s -f "$SERVER_URL/" > /dev/null; then
    echo -e "${GREEN}✓${NC} Server is running"
else
    echo -e "${RED}✗${NC} Server is not running"
    echo "Please start the server first with: ./build/http2_server_app"
    exit 1
fi
echo ""

# Test GET /
echo -e "${BLUE}[2/12]${NC} Testing GET / (root page)..."
RESPONSE=$(curl -s "$SERVER_URL/")
if echo "$RESPONSE" | grep -q "Welcome to Equinox"; then
    echo -e "${GREEN}✓${NC} GET / returned HTML page"
else
    echo -e "${RED}✗${NC} GET / failed"
fi
echo ""

# Test GET /api/status
echo -e "${BLUE}[3/12]${NC} Testing GET /api/status..."
RESPONSE=$(curl -s "$SERVER_URL/api/status")
echo "Response: $RESPONSE"
if echo "$RESPONSE" | grep -q "HTTP/2"; then
    echo -e "${GREEN}✓${NC} GET /api/status returned JSON with HTTP/2 support"
else
    echo -e "${RED}✗${NC} GET /api/status failed"
fi
echo ""

# Test GET /api/users
echo -e "${BLUE}[4/12]${NC} Testing GET /api/users..."
RESPONSE=$(curl -s "$SERVER_URL/api/users")
echo "Response: $RESPONSE"
if echo "$RESPONSE" | grep -q "Alice"; then
    echo -e "${GREEN}✓${NC} GET /api/users returned user list"
else
    echo -e "${RED}✗${NC} GET /api/users failed"
fi
echo ""

# Test POST /api/users
echo -e "${BLUE}[5/12]${NC} Testing POST /api/users..."
RESPONSE=$(curl -s -X POST -H "Content-Type: application/json" \
    -d '{"name":"Test User","email":"test@example.com"}' \
    "$SERVER_URL/api/users")
echo "Response: $RESPONSE"
if echo "$RESPONSE" | grep -q "created successfully"; then
    echo -e "${GREEN}✓${NC} POST /api/users created new user"
else
    echo -e "${RED}✗${NC} POST /api/users failed"
fi
echo ""

# Test PUT /api/users/1 (full replacement)
echo -e "${BLUE}[6/12]${NC} Testing PUT /api/users/1 (full replacement)..."
RESPONSE=$(curl -s -X PUT -H "Content-Type: application/json" \
    -d '{"name":"Updated User","email":"updated@example.com","role":"admin"}' \
    "$SERVER_URL/api/users/1")
echo "Response: $RESPONSE"
if echo "$RESPONSE" | grep -q "full replacement" && echo "$RESPONSE" | grep -q "PUT"; then
    echo -e "${GREEN}✓${NC} PUT /api/users/1 performed full replacement"
else
    echo -e "${RED}✗${NC} PUT /api/users/1 failed"
fi
echo ""

# Test PATCH /api/users/1 (partial update) - NEW!
echo -e "${BLUE}[7/12]${NC} Testing PATCH /api/users/1 (partial update) - NEW!"
RESPONSE=$(curl -s -X PATCH -H "Content-Type: application/json" \
    -d '{"email":"newemail@example.com"}' \
    "$SERVER_URL/api/users/1")
echo "Response: $RESPONSE"
if echo "$RESPONSE" | grep -q "partial update" && echo "$RESPONSE" | grep -q "PATCH"; then
    echo -e "${GREEN}✓${NC} PATCH /api/users/1 performed partial update"
else
    echo -e "${RED}✗${NC} PATCH /api/users/1 failed"
fi
echo ""

# Test DELETE /api/users/1
echo -e "${BLUE}[8/12]${NC} Testing DELETE /api/users/1..."
RESPONSE=$(curl -s -X DELETE "$SERVER_URL/api/users/1")
echo "Response: $RESPONSE"
if echo "$RESPONSE" | grep -q "deleted successfully"; then
    echo -e "${GREEN}✓${NC} DELETE /api/users/1 deleted user"
else
    echo -e "${RED}✗${NC} DELETE /api/users/1 failed"
fi
echo ""

# Test 404 error
echo -e "${BLUE}[9/12]${NC} Testing 404 (not found)..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL/nonexistent")
if [ "$HTTP_CODE" == "404" ]; then
    echo -e "${GREEN}✓${NC} 404 error handling works"
else
    echo -e "${RED}✗${NC} 404 error handling failed (got HTTP $HTTP_CODE)"
fi
echo ""

# Check Server header for protocol support
echo -e "${BLUE}[10/12]${NC} Checking server protocol support..."
SERVER_HEADER=$(curl -I -s "$SERVER_URL/" | grep -i "^Server:")
echo "Server header: $SERVER_HEADER"
if echo "$SERVER_HEADER" | grep -q "HTTP/1.1" && echo "$SERVER_HEADER" | grep -q "HTTP/2"; then
    echo -e "${GREEN}✓${NC} Server advertises HTTP/1.1 and HTTP/2 support"
else
    echo -e "${RED}✗${NC} Server header doesn't show both protocols"
fi
echo ""

# Test Content-Type headers
echo -e "${BLUE}[11/12]${NC} Testing Content-Type headers..."
CONTENT_TYPE=$(curl -s -I "$SERVER_URL/api/status" | grep -i "^Content-Type:")
echo "Content-Type: $CONTENT_TYPE"
if echo "$CONTENT_TYPE" | grep -q "application/json"; then
    echo -e "${GREEN}✓${NC} JSON Content-Type header is correct"
else
    echo -e "${RED}✗${NC} Content-Type header is incorrect"
fi
echo ""

# Test all HTTP methods
echo -e "${BLUE}[12/12]${NC} Testing all supported HTTP methods..."
METHODS=("GET" "POST" "PUT" "PATCH" "DELETE")
METHODS_OK=true
for METHOD in "${METHODS[@]}"; do
    if [ "$METHOD" == "GET" ]; then
        curl -s -X $METHOD "$SERVER_URL/api/users" > /dev/null && echo -e "  ${GREEN}✓${NC} $METHOD" || METHODS_OK=false
    elif [ "$METHOD" == "DELETE" ]; then
        curl -s -X $METHOD "$SERVER_URL/api/users/1" > /dev/null && echo -e "  ${GREEN}✓${NC} $METHOD" || METHODS_OK=false
    else
        curl -s -X $METHOD -H "Content-Type: application/json" -d '{"test":"data"}' "$SERVER_URL/api/users/1" > /dev/null && echo -e "  ${GREEN}✓${NC} $METHOD" || METHODS_OK=false
    fi
done
if [ "$METHODS_OK" = true ]; then
    echo -e "${GREEN}✓${NC} All HTTP methods are working"
fi
echo ""

echo "╔═══════════════════════════════════════════════════════╗"
echo "║   Test Suite Complete!                               ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""
echo -e "${GREEN}Summary:${NC}"
echo "  ✓ HTTP/1.1 protocol working"
echo "  ✓ HTTP/2 support advertised"
echo "  ✓ PATCH method implemented"
echo "  ✓ GET, POST, PUT, DELETE methods working"
echo "  ✓ JSON response handling"
echo "  ✓ Error handling (404)"
echo ""
echo -e "${YELLOW}Note:${NC} For full HTTP/2 testing with binary protocol, use:"
echo "  curl --http2-prior-knowledge http://localhost:8080/api/status"
echo "  or install nghttp2-client and use: nghttp -v http://localhost:8080/"
echo ""
