#!/bin/bash
# Cleanup script - removes logs, leases, and build files
# Usage: ./cleanup.sh [--all]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== DHCP Cleanup ==="
echo ""

# Always clean logs
echo "Removing log files..."
rm -f "$ROOT_DIR/DHCPv6/dhcpv6.log"
rm -f "$ROOT_DIR/DHCPv4/dhcpv4.log"

# Clean leases
echo "Removing lease databases..."
rm -f "$ROOT_DIR/DHCPv6/leases/dhcpd6.leases"
rm -f "$ROOT_DIR/DHCPv4/data/dhcpd.leases"

if [ "$1" == "--all" ]; then
    echo "Cleaning build files..."
    cd "$ROOT_DIR/DHCPv6" && make clean
    cd "$ROOT_DIR/DHCPv4" && make clean
    cd "$ROOT_DIR/client" && make clean
fi

echo ""
echo "Cleanup complete!"
echo ""
echo "To rebuild, run:"
echo "  cd DHCPv4 && make"
echo "  cd DHCPv6 && make"
echo "  cd client && make"
