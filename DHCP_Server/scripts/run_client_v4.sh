#!/bin/bash
cd "$(dirname "$0")/.."
mkdir -p logs

show_usage() {
    echo "Usage: $0 <interface> [-s server_ip] [-p port] [-m mac] [-c client_port]"
    echo "  -s : Server IP (unicast instead of broadcast)"
    echo "  -p : Server port (default: 67)"
    echo "  -m : Custom MAC address (e.g., aa:bb:cc:dd:ee:ff)"
    echo "  -c : Client port (default: 68, use different for multiple clients)"
    echo ""
    echo "Examples:"
    echo "  $0 eth0                           # Normal broadcast mode"
    echo "  $0 lo -s 127.0.0.1                # Loopback testing with unicast"
    echo "  $0 lo -s 127.0.0.1 -m aa:bb:cc:dd:ee:01 -c 69  # Multi-client test"
}

IF="${1:-}"
if [ -z "$IF" ]; then
    show_usage
    exit 1
fi

if ! ip link show "$IF" &>/dev/null; then
    echo "Interface '$IF' not found. Available:"
    ip -br link show
    exit 1
fi

shift
echo "Starting DHCPv4 Client on $IF..."
echo "Log: logs/dhcpv4_client.log"
echo ""
sudo ./build/bin/dhcpv4_client "$IF" "$@"
