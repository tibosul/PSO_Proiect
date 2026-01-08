#!/bin/bash
#
# DHCP Network Testing Script
# Tests DHCP server and client across actual network interfaces
#
# This script helps verify:
# - DHCP server works on real network interfaces (not just loopback)
# - Clients on different computers can obtain IP addresses
# - Network configuration is applied correctly
#
# USAGE:
#   On Server Computer:
#     sudo ./test_network.sh server <interface>
#
#   On Client Computer(s):
#     sudo ./test_network.sh client <interface> <server_ip>
#

# Note: We use 'set -e' for early failure detection. 
# This script is a helper that execs into server/client binaries,
# so cleanup is handled by the executed process.
set -e

cd "$(dirname "$0")/.."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SERVER_BIN="./build/bin/dhcpv4_server"
CLIENT_BIN="./build/bin/dhcpv4_client"
LOG_DIR="logs"

usage() {
    echo -e "${BLUE}DHCP Network Testing Script${NC}"
    echo ""
    echo "Usage:"
    echo "  Server mode: sudo $0 server <interface>"
    echo "  Client mode: sudo $0 client <interface> <server_ip>"
    echo ""
    echo "Examples:"
    echo "  ${YELLOW}# On server computer (e.g., 192.168.1.1):${NC}"
    echo "  sudo $0 server eth0"
    echo ""
    echo "  ${YELLOW}# On client computer:${NC}"
    echo "  sudo $0 client eth0 192.168.1.1"
    echo ""
    echo "  ${YELLOW}# Broadcast mode (server must be on same subnet):${NC}"
    echo "  sudo $0 client eth0"
    echo ""
    exit 1
}

if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}ERROR: This script must be run as root (use sudo)${NC}"
    exit 1
fi

MODE="${1:-}"
INTERFACE="${2:-}"

if [ -z "$MODE" ] || [ -z "$INTERFACE" ]; then
    usage
fi

# Check if binaries exist
if [ ! -f "$SERVER_BIN" ] && [ "$MODE" = "server" ]; then
    echo -e "${RED}ERROR: Server binary not found. Run 'make all' first.${NC}"
    exit 1
fi

if [ ! -f "$CLIENT_BIN" ] && [ "$MODE" = "client" ]; then
    echo -e "${RED}ERROR: Client binary not found. Run 'make all' first.${NC}"
    exit 1
fi

# Check if interface exists
if ! ip link show "$INTERFACE" &>/dev/null; then
    echo -e "${RED}ERROR: Interface '$INTERFACE' not found${NC}"
    echo ""
    echo "Available interfaces:"
    ip -br link show
    exit 1
fi

mkdir -p "$LOG_DIR"

if [ "$MODE" = "server" ]; then
    echo -e "${BLUE}=============================================${NC}"
    echo -e "${BLUE}  DHCP Server - Network Mode${NC}"
    echo -e "${BLUE}=============================================${NC}"
    echo ""
    echo -e "${YELLOW}Interface: $INTERFACE${NC}"
    
    # Show interface details
    echo ""
    echo -e "${YELLOW}Interface Details:${NC}"
    ip -br addr show "$INTERFACE"
    echo ""
    
    # Check configuration
    CONFIG_FILE="DHCPv4/config/dhcpv4.conf"
    if [ ! -f "$CONFIG_FILE" ]; then
        echo -e "${RED}ERROR: Configuration file not found: $CONFIG_FILE${NC}"
        exit 1
    fi
    
    echo -e "${YELLOW}Configuration:${NC}"
    echo -e "  Config file: $CONFIG_FILE"
    echo -e "  Log file: $LOG_DIR/dhcpv4_server.log"
    echo ""
    
    echo -e "${YELLOW}Configured Subnets:${NC}"
    grep -A 1 "^subnet" "$CONFIG_FILE" | grep -v "^--" | sed 's/^/  /'
    echo ""
    
    echo -e "${GREEN}Starting DHCP Server...${NC}"
    echo -e "${YELLOW}Press Ctrl+C to stop${NC}"
    echo ""
    
    # Start server
    exec "$SERVER_BIN"
    
elif [ "$MODE" = "client" ]; then
    SERVER_IP="${3:-}"
    
    echo -e "${BLUE}=============================================${NC}"
    echo -e "${BLUE}  DHCP Client - Network Mode${NC}"
    echo -e "${BLUE}=============================================${NC}"
    echo ""
    echo -e "${YELLOW}Interface: $INTERFACE${NC}"
    
    # Show interface details
    echo ""
    echo -e "${YELLOW}Interface Details (before):${NC}"
    ip -br addr show "$INTERFACE"
    MAC=$(ip link show "$INTERFACE" | grep link/ether | awk '{print $2}')
    echo -e "  MAC Address: $MAC"
    echo ""
    
    if [ -n "$SERVER_IP" ]; then
        echo -e "${YELLOW}Mode: Unicast to $SERVER_IP${NC}"
        echo -e "  (Server must be at $SERVER_IP)${NC}"
        echo ""
        
        # Test connectivity
        echo -e "${YELLOW}Testing connectivity to server...${NC}"
        if ping -c 1 -W 2 "$SERVER_IP" &>/dev/null; then
            echo -e "${GREEN}✓ Server is reachable${NC}"
        else
            echo -e "${YELLOW}! Server not responding to ping (might be OK if ICMP is blocked)${NC}"
        fi
        echo ""
        
        echo -e "${GREEN}Starting DHCP Client (unicast mode)...${NC}"
        exec "$CLIENT_BIN" "$INTERFACE" -s "$SERVER_IP"
    else
        echo -e "${YELLOW}Mode: Broadcast${NC}"
        echo -e "  (Will discover any DHCP server on the subnet)${NC}"
        echo ""
        
        echo -e "${GREEN}Starting DHCP Client (broadcast mode)...${NC}"
        exec "$CLIENT_BIN" "$INTERFACE"
    fi
    
else
    echo -e "${RED}ERROR: Invalid mode '$MODE'. Use 'server' or 'client'${NC}"
    usage
fi
