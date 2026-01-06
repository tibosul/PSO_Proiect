#!/bin/bash
cd "$(dirname "$0")/.."
mkdir -p logs
IF="${1:-lo}"
shift
if ! ip link show "$IF" &>/dev/null; then
    echo "Interface '$IF' not found. Available:"
    ip -br link show
    exit 1
fi
echo "Starting DHCPv6 Client on $IF..."
echo "Log: logs/dhcpv6_client.log"
echo ""
sudo ./build/bin/dhcpv6_client "$IF" "$@"
