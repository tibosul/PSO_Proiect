#!/bin/bash
cd "$(dirname "$0")/.."
mkdir -p DHCPv6/leases logs
echo "Starting DHCPv6 Server..."
echo "Log: logs/dhcpv6_server.log"
echo "Config: DHCPv6/config/dhcpv6.conf"
echo ""
sudo ../build/bin/dhcpv6_server
