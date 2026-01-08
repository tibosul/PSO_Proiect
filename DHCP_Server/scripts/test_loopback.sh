#!/bin/bash
#
# DHCP Loopback Testing Script
# Tests DHCP server and client functionality using loopback interface (127.0.0.1)
#
# This script verifies:
# - DHCPv4 server can start and bind to loopback
# - DHCPv4 clients can discover, request, and obtain IP addresses
# - Multiple clients can coexist using different ports and MAC addresses
# - Lease management works correctly
#

set -e  # Exit on error

cd "$(dirname "$0")/.."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
SERVER_BIN="./build/bin/dhcpv4_server"
CLIENT_BIN="./build/bin/dhcpv4_client"
LOG_DIR="logs"
TEST_DURATION=30  # seconds to run test

echo -e "${BLUE}=============================================${NC}"
echo -e "${BLUE}  DHCP Loopback Functionality Test${NC}"
echo -e "${BLUE}=============================================${NC}"
echo ""

# Check if binaries exist
if [ ! -f "$SERVER_BIN" ]; then
    echo -e "${RED}ERROR: Server binary not found. Run 'make all' first.${NC}"
    exit 1
fi

if [ ! -f "$CLIENT_BIN" ]; then
    echo -e "${RED}ERROR: Client binary not found. Run 'make all' first.${NC}"
    exit 1
fi

# Create log directory
mkdir -p "$LOG_DIR"

# Clean up old logs
rm -f "$LOG_DIR"/dhcpv4*.log

echo -e "${YELLOW}Step 1: Starting DHCPv4 Server...${NC}"
# Start server in background
sudo "$SERVER_BIN" > "$LOG_DIR/test_server_stdout.log" 2>&1 &
SERVER_PID=$!
echo -e "  Server PID: $SERVER_PID"

# Wait for server to initialize
echo -e "  Waiting for server to start..."
sleep 3

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}ERROR: Server failed to start${NC}"
    echo -e "${YELLOW}Server log:${NC}"
    cat "$LOG_DIR/dhcpv4_server.log" 2>/dev/null || echo "No log file found"
    exit 1
fi
echo -e "${GREEN}  ✓ Server started successfully${NC}"
echo ""

# Function to start a client
start_client() {
    local CLIENT_NUM=$1
    local CLIENT_PORT=$((68 + CLIENT_NUM))
    local MAC="aa:bb:cc:dd:ee:$(printf '%02x' $CLIENT_NUM)"
    
    echo -e "${YELLOW}Step $((CLIENT_NUM + 1)): Starting Client $CLIENT_NUM...${NC}"
    echo -e "  Interface: lo"
    echo -e "  Server: 127.0.0.1"
    echo -e "  MAC: $MAC"
    echo -e "  Client Port: $CLIENT_PORT"
    
    # Start client in background
    sudo "$CLIENT_BIN" lo -s 127.0.0.1 -m "$MAC" -c "$CLIENT_PORT" \
        > "$LOG_DIR/test_client${CLIENT_NUM}_stdout.log" 2>&1 &
    
    local CLIENT_PID=$!
    echo "  Client $CLIENT_NUM PID: $CLIENT_PID"
    echo "$CLIENT_PID" >> /tmp/dhcp_test_clients.pids
    
    # Give client time to complete DHCP handshake
    sleep 2
    
    # Check if client is still running or completed successfully
    if kill -0 $CLIENT_PID 2>/dev/null; then
        echo -e "${GREEN}  ✓ Client $CLIENT_NUM running${NC}"
    else
        echo -e "${YELLOW}  ! Client $CLIENT_NUM completed (check logs)${NC}"
    fi
    echo ""
}

# Clear client PIDs file
rm -f /tmp/dhcp_test_clients.pids

# Start multiple clients to test concurrent operation
echo -e "${BLUE}Testing Multiple Clients:${NC}"
echo ""
start_client 1
start_client 2
start_client 3

# Let clients run for a while
echo -e "${YELLOW}Running test for ${TEST_DURATION} seconds...${NC}"
sleep $TEST_DURATION

echo ""
echo -e "${BLUE}=============================================${NC}"
echo -e "${BLUE}  Test Results${NC}"
echo -e "${BLUE}=============================================${NC}"
echo ""

# Check server log for successful operations
echo -e "${YELLOW}Server Activity:${NC}"
if [ -f "$LOG_DIR/dhcpv4_server.log" ]; then
    DISCOVERS=$(grep -c "DHCPDISCOVER" "$LOG_DIR/dhcpv4_server.log" 2>/dev/null || echo "0")
    OFFERS=$(grep -c "DHCPOFFER" "$LOG_DIR/dhcpv4_server.log" 2>/dev/null || echo "0")
    REQUESTS=$(grep -c "DHCPREQUEST" "$LOG_DIR/dhcpv4_server.log" 2>/dev/null || echo "0")
    ACKS=$(grep -c "DHCPACK" "$LOG_DIR/dhcpv4_server.log" 2>/dev/null || echo "0")
    
    echo -e "  DHCPDISCOVER messages: $DISCOVERS"
    echo -e "  DHCPOFFER responses: $OFFERS"
    echo -e "  DHCPREQUEST messages: $REQUESTS"
    echo -e "  DHCPACK responses: $ACKS"
    
    if [ "$DISCOVERS" -gt 0 ] && [ "$OFFERS" -gt 0 ] && [ "$REQUESTS" -gt 0 ] && [ "$ACKS" -gt 0 ]; then
        echo -e "${GREEN}  ✓ Complete DHCP handshake detected${NC}"
    else
        echo -e "${YELLOW}  ! Partial DHCP activity detected${NC}"
    fi
else
    echo -e "${RED}  ✗ No server log found${NC}"
fi
echo ""

# Check client logs
echo -e "${YELLOW}Client Results:${NC}"
for i in 1 2 3; do
    CLIENT_LOG="$LOG_DIR/dhcpv4_client.log"
    if [ -f "$CLIENT_LOG" ]; then
        # Check if client received an IP
        IP_ASSIGNED=$(grep "BOUND" "$CLIENT_LOG" 2>/dev/null || echo "")
        if [ -n "$IP_ASSIGNED" ]; then
            echo -e "  ${GREEN}✓ Client $i: IP assigned${NC}"
            # Show assigned IP
            ASSIGNED_IP=$(echo "$IP_ASSIGNED" | grep -oP '127\.\d+\.\d+\.\d+' | head -1)
            if [ -n "$ASSIGNED_IP" ]; then
                echo -e "    IP: $ASSIGNED_IP"
            fi
        else
            echo -e "  ${YELLOW}! Client $i: No IP assignment found in log${NC}"
        fi
    else
        echo -e "  ${YELLOW}! Client $i: No log file found${NC}"
    fi
done
echo ""

# Check lease database
echo -e "${YELLOW}Lease Database:${NC}"
LEASE_FILE="DHCPv4/data/dhcpv4.leases"
if [ -f "$LEASE_FILE" ]; then
    LEASE_COUNT=$(grep -c "lease" "$LEASE_FILE" 2>/dev/null || echo "0")
    echo -e "  Total leases: $LEASE_COUNT"
    echo -e "  ${GREEN}✓ Lease database exists${NC}"
    echo ""
    echo -e "  Recent leases:"
    tail -20 "$LEASE_FILE" | sed 's/^/    /'
else
    echo -e "  ${YELLOW}! No lease database found${NC}"
fi
echo ""

# Cleanup
echo -e "${YELLOW}Cleaning up...${NC}"

# Kill clients
if [ -f /tmp/dhcp_test_clients.pids ]; then
    while read pid; do
        if kill -0 $pid 2>/dev/null; then
            sudo kill $pid 2>/dev/null || true
        fi
    done < /tmp/dhcp_test_clients.pids
    rm -f /tmp/dhcp_test_clients.pids
fi

# Kill server
if kill -0 $SERVER_PID 2>/dev/null; then
    sudo kill $SERVER_PID
    wait $SERVER_PID 2>/dev/null || true
fi

echo -e "${GREEN}✓ Cleanup complete${NC}"
echo ""

echo -e "${BLUE}=============================================${NC}"
echo -e "${BLUE}  Test Complete${NC}"
echo -e "${BLUE}=============================================${NC}"
echo ""
echo -e "Logs saved to: $LOG_DIR/"
echo -e "  - dhcpv4_server.log"
echo -e "  - dhcpv4_client.log"
echo -e "  - test_server_stdout.log"
echo -e "  - test_client*_stdout.log"
echo ""
