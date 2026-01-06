#!/bin/bash
cd "$(dirname "$0")/.."
IF="${1:-lo}"
shift
if ! ip link show "$IF" &>/dev/null; then
    echo "Interface '$IF' not found. Available:"
    ip -br link show
    exit 1
fi
echo "Starting DHCPv6 Client on $IF..."
sudo ./build/bin/dhcpv6_client "$IF" "$@"
