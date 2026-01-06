#!/bin/bash
# Start DHCPv6 Server
# Logs are written to: DHCPv6/dhcpv6.log

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
DHCPV6_DIR="$ROOT_DIR/DHCPv6"

cd "$DHCPV6_DIR" || exit 1

# Create required directories
mkdir -p leases

echo "=== Starting DHCPv6 Server ==="
echo "Config: config/dhcpv6.conf"
echo "Log file: dhcpv6.log"
echo "Lease DB: leases/dhcpd6.leases"
echo "Port: 547 (UDP)"
echo ""
echo "Press Ctrl+C to stop"
echo "================================"

# Run with sudo if port 547 is needed
if [ "$1" == "--privileged" ]; then
    sudo ./bin/dhcpv6_server
else
    echo "NOTE: Port 547 requires root privileges"
    echo "Use --privileged flag or run with sudo"
    echo ""
    sudo ./bin/dhcpv6_server
fi
