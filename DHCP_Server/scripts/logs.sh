#!/bin/bash
cd "$(dirname "$0")/.."
echo "=== DHCPv4 Server Log ==="
tail -30 logs/dhcpv4.log 2>/dev/null || echo "(empty)"
echo ""
echo "=== DHCPv6 Server Log ==="
tail -30 logs/dhcpv6.log 2>/dev/null || echo "(empty)"
