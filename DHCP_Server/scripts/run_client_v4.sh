#!/bin/bash
cd "$(dirname "$0")/.."
mkdir -p logs

show_usage() {
    echo "Usage: $0 <interface> [-s server_ip] [-p port]"
    echo "  -s : Server IP (unicast instead of broadcast)"
    echo "  -p : Server port (default: 67)"
    echo ""
    echo "Examples:"
    echo "  $0 eth0                    # Normal broadcast mode"
    echo "  $0 lo -s 127.0.0.1         # Loopback testing with unicast"
    echo "  $0 lo -s 127.0.0.1 -p 6767 # Testing with fallback port"
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
