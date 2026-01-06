#!/bin/bash
# View DHCP server logs
# Usage: ./view_logs.sh [v4|v6|both]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
VERSION="${1:-both}"

echo "=== DHCP Server Logs ==="
echo ""

if [ "$VERSION" == "v4" ] || [ "$VERSION" == "both" ]; then
    LOGV4="$ROOT_DIR/DHCPv4/dhcpv4.log"
    echo "--- DHCPv4 Log ($LOGV4) ---"
    if [ -f "$LOGV4" ]; then
        tail -50 "$LOGV4"
    else
        echo "(Log file not found - server hasn't run yet)"
    fi
    echo ""
fi

if [ "$VERSION" == "v6" ] || [ "$VERSION" == "both" ]; then
    LOGV6="$ROOT_DIR/DHCPv6/dhcpv6.log"
    echo "--- DHCPv6 Log ($LOGV6) ---"
    if [ -f "$LOGV6" ]; then
        tail -50 "$LOGV6"
    else
        echo "(Log file not found - server hasn't run yet)"
    fi
    echo ""
fi

echo "=== End of Logs ==="
echo ""
echo "For live log viewing, use:"
echo "  DHCPv4: tail -f $ROOT_DIR/DHCPv4/dhcpv4.log"
echo "  DHCPv6: tail -f $ROOT_DIR/DHCPv6/dhcpv6.log"
