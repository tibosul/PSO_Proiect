#!/bin/bash
cd "$(dirname "$0")/.."
mkdir -p DHCPv4/data logs
echo "Starting DHCPv4 Server..."
echo "Log: logs/dhcpv4_server.log"
echo "Config: DHCPv4/config/dhcpv4.conf"
echo ""
sudo ./build/bin/dhcpv4_server
