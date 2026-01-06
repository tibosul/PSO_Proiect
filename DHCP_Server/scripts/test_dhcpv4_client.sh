#!/bin/bash
# Test DHCPv4 Client
# Usage: ./test_dhcpv4_client.sh [interface]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
CLIENT_BIN="$ROOT_DIR/client/bin/dhcpv4_client"

# Default interface
INTERFACE="${1:-lo}"

echo "=== DHCPv4 Client Test ==="
echo "Interface: $INTERFACE"
echo "Binary: $CLIENT_BIN"
echo ""

# Check if interface exists
if ! ip link show "$INTERFACE" &>/dev/null; then
    echo "ERROR: Interface '$INTERFACE' does not exist"
    echo ""
    echo "Available interfaces:"
    ip -br link show
    exit 1
fi

echo "Press Ctrl+C to stop"
echo "========================="
echo ""

# Run client (needs root for port 68 and IP configuration)
sudo "$CLIENT_BIN" "$INTERFACE"
