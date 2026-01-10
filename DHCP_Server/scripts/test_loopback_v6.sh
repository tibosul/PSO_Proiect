#!/bin/bash
#
# DHCPv6 Loopback Testing Script
# Tests DHCPv6 server and client functionality using loopback interface
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Cleanup function
cleanup() {
    echo ""
    echo -e "${YELLOW}Cleaning up...${NC}"
    
    # Kill clients
    if [ -f "$CLIENT_PIDS_FILE" ]; then
        while read pid; do
            if kill -0 $pid 2>/dev/null; then
                sudo kill $pid 2>/dev/null || true
            fi
        done < "$CLIENT_PIDS_FILE" 2>/dev/null || true
        rm -f "$CLIENT_PIDS_FILE" || true
    fi
    
    # Kill server
    if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        sudo kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi

    # Remove temporary IP address
    echo "Removing temporary IPv6 address..."
    sudo ip -6 addr del 2001:db8:1:0::1/64 dev lo 2>/dev/null || true
    sudo ip -6 route del ff02::1:2/128 dev lo 2>/dev/null || true
}

trap cleanup EXIT INT TERM

cd "$(dirname "$0")/.."

# Configuration
SERVER_BIN="./build/bin/dhcpv6_server"
CLIENT_BIN="./build/bin/dhcpv6_client"
LOG_DIR="logs"
TEST_DURATION=30
SERVER_PID=""
CLIENT_PIDS_FILE=$(mktemp)

echo -e "${BLUE}=============================================${NC}"
echo -e "${BLUE}  DHCPv6 Loopback Functionality Test${NC}"
echo -e "${BLUE}=============================================${NC}"
echo ""

# Create log directory
mkdir -p "$LOG_DIR"
mkdir -p "DHCPv6/logs"
rm -f "$LOG_DIR"/dhcpv6*.log
rm -f "DHCPv6/logs"/dhcpv6*.log

# Check binaries
if [ ! -f "$SERVER_BIN" ] || [ ! -f "$CLIENT_BIN" ]; then
    echo -e "${RED}ERROR: Binaries not found. Running make...${NC}"
    make v6
fi

# Setup Network
echo -e "${YELLOW}Step 1: Setting up Network Environment...${NC}"

# Enable Multicast on loopback (Crucial!)
echo "Enabling multicast on lo..."
sudo ip link set lo multicast on

echo "Adding temporary IPv6 address 2001:db8:1:0::1/64 to lo..."
if sudo ip -6 addr add 2001:db8:1:0::1/64 dev lo 2>/dev/null; then
    echo -e "${GREEN}  ✓ Address added${NC}"
else
    echo -e "${YELLOW}  ! Address might already exist (ignoring)${NC}"
fi

# Add multicast route for loopback (Crucial for Client -> Server communication)
echo "Adding multicast route for ff02::1:2 on lo..."
sudo ip -6 route add ff02::1:2/128 dev lo 2>/dev/null || true

# Start Server
echo -e "${YELLOW}Step 2: Starting DHCPv6 Server...${NC}"
# Server expects to run from DHCPv6 directory to find config/dhcpv6.conf
pushd DHCPv6 > /dev/null
sudo "../$SERVER_BIN" > "../$LOG_DIR/test_server_v6_stdout.log" 2>&1 &
SERVER_PID=$!
popd > /dev/null
echo "  Server PID: $SERVER_PID"

echo "  Waiting for server to initialize..."
sleep 3

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}ERROR: Server failed to start${NC}"
    # Check both potential log locations and stdout
    cat "DHCPv6/logs/dhcpv6_server.log" 2>/dev/null
    cat "$LOG_DIR/test_server_v6_stdout.log" 2>/dev/null
    exit 1
fi
echo -e "${GREEN}  ✓ Server started${NC}"

# Start Client
echo -e "${YELLOW}Step 3: Starting DHCPv6 Client...${NC}"
echo "  Interface: lo"
sudo "$CLIENT_BIN" lo > "$LOG_DIR/test_client_v6_stdout.log" 2>&1 &
CLIENT_PID=$!
echo "  Client PID: $CLIENT_PID"
echo "$CLIENT_PID" >> "$CLIENT_PIDS_FILE"

echo -e "${YELLOW}Running test for ${TEST_DURATION} seconds...${NC}"
sleep $TEST_DURATION

# Check Results
echo ""
echo -e "${BLUE}=============================================${NC}"
echo -e "${BLUE}  Test Results${NC}"
echo -e "${BLUE}=============================================${NC}"

SERVER_LOG="DHCPv6/logs/dhcpv6_server.log"

if [ -f "$SERVER_LOG" ]; then
    SOLICIT=$(grep -c "SOLICIT" "$SERVER_LOG" | head -n 1)
    ADVERTISE=$(grep -c "ADVERTISE" "$SERVER_LOG" | head -n 1)
    REQUEST=$(grep -c "REQUEST" "$SERVER_LOG" | head -n 1)
    REPLY=$(grep -c "REPLY" "$SERVER_LOG" | head -n 1) # DHCPv6 uses REPLY, not ACK

    echo -e "  SOLICIT messages: ${SOLICIT:-0}"
    echo -e "  ADVERTISE responses: ${ADVERTISE:-0}"
    echo -e "  REQUEST messages: ${REQUEST:-0}"
    echo -e "  REPLY responses: ${REPLY:-0}"

    if [ "${REPLY:-0}" -gt 0 ]; then
        echo -e "${GREEN}  ✓ SUCCESS: Complete DHCPv6 handshake detected${NC}"
    else
        echo -e "${RED}  ✗ FAILURE: Incomplete handshake${NC}"
        echo -e "${YELLOW}Server Log Tail:${NC}"
        tail -n 20 "$SERVER_LOG"
        echo ""
        echo -e "${YELLOW}Client Log Tail:${NC}"
        # DHCPv6 client log path might handle things differently, checking standard location
        if [ -f "$LOG_DIR/dhcpv6_client.log" ]; then
             tail -n 20 "$LOG_DIR/dhcpv6_client.log"
        elif [ -f "dhcpv6_client.log" ]; then
             tail -n 20 "dhcpv6_client.log"
        else
             echo "Client log not found."
        fi
    fi
else
    echo -e "${RED}  ✗ No server log found at $SERVER_LOG${NC}"
fi
echo ""
