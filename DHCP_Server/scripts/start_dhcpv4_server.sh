#!/bin/bash
# Start DHCPv4 Server
# Logs are written to: DHCPv4/dhcpv4.log

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
DHCPV4_DIR="$ROOT_DIR/DHCPv4"

cd "$DHCPV4_DIR" || exit 1

# Create required directories
mkdir -p data

echo "=== Starting DHCPv4 Server ==="
echo "Config: config/dhcpv4.conf"
echo "Log file: dhcpv4.log"
echo "Lease DB: data/dhcpd.leases"
echo ""
echo "Press Ctrl+C to stop"
echo "================================"

# Run with sudo if port 67 is needed
if [ "$1" == "--privileged" ]; then
    sudo ./build/bin/dhcpd
else
    echo "NOTE: Running without sudo (using fallback port 6767)"
    echo "Use --privileged flag to bind to port 67"
    echo ""
    ./build/bin/dhcpd
fi
