#!/bin/bash
# Test DHCPv6 Client
# Usage: ./test_dhcpv6_client.sh [interface] [-P] [-S hexbyte]
#   -P : Request Prefix Delegation
#   -S : Spoof last byte of MAC (for multiple client testing)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
CLIENT_BIN="$ROOT_DIR/client/bin/dhcpv6_client"

# Parse arguments
INTERFACE=""
EXTRA_ARGS=""

for arg in "$@"; do
    case $arg in
        -P|-S)
            EXTRA_ARGS="$EXTRA_ARGS $arg"
            ;;
        -*)
            EXTRA_ARGS="$EXTRA_ARGS $arg"
            ;;
        *)
            if [ -z "$INTERFACE" ]; then
                INTERFACE="$arg"
            else
                EXTRA_ARGS="$EXTRA_ARGS $arg"
            fi
            ;;
    esac
done

# Default interface
INTERFACE="${INTERFACE:-lo}"

echo "=== DHCPv6 Client Test ==="
echo "Interface: $INTERFACE"
echo "Binary: $CLIENT_BIN"
echo "Extra args: $EXTRA_ARGS"
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

# Run client (needs root for port 546 and IP configuration)
sudo "$CLIENT_BIN" "$INTERFACE" $EXTRA_ARGS
