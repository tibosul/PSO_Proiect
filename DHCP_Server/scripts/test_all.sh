#!/bin/bash
# Full test script - starts servers and clients in separate terminals
# Usage: ./test_all.sh [v4|v6|both] [interface]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="${1:-both}"
INTERFACE="${2:-lo}"

echo "=== DHCP Testing Suite ==="
echo "Version: $VERSION"
echo "Interface: $INTERFACE"
echo ""

# Function to check if a terminal emulator is available
find_terminal() {
    for term in gnome-terminal konsole xterm xfce4-terminal; do
        if command -v $term &>/dev/null; then
            echo $term
            return 0
        fi
    done
    echo ""
    return 1
}

TERMINAL=$(find_terminal)

if [ -z "$TERMINAL" ]; then
    echo "No graphical terminal found."
    echo ""
    echo "Please run these commands in separate terminals:"
    echo ""

    if [ "$VERSION" == "v4" ] || [ "$VERSION" == "both" ]; then
        echo "Terminal 1 (DHCPv4 Server):"
        echo "  cd $SCRIPT_DIR && ./start_dhcpv4_server.sh"
        echo ""
        echo "Terminal 2 (DHCPv4 Client):"
        echo "  cd $SCRIPT_DIR && ./test_dhcpv4_client.sh $INTERFACE"
        echo ""
    fi

    if [ "$VERSION" == "v6" ] || [ "$VERSION" == "both" ]; then
        echo "Terminal 3 (DHCPv6 Server):"
        echo "  cd $SCRIPT_DIR && ./start_dhcpv6_server.sh"
        echo ""
        echo "Terminal 4 (DHCPv6 Client):"
        echo "  cd $SCRIPT_DIR && ./test_dhcpv6_client.sh $INTERFACE"
        echo ""
    fi

    exit 0
fi

echo "Using terminal: $TERMINAL"
echo ""

case $TERMINAL in
    gnome-terminal)
        if [ "$VERSION" == "v4" ] || [ "$VERSION" == "both" ]; then
            gnome-terminal --title="DHCPv4 Server" -- bash -c "cd '$SCRIPT_DIR' && ./start_dhcpv4_server.sh; exec bash"
            sleep 2
            gnome-terminal --title="DHCPv4 Client" -- bash -c "cd '$SCRIPT_DIR' && ./test_dhcpv4_client.sh $INTERFACE; exec bash"
        fi
        if [ "$VERSION" == "v6" ] || [ "$VERSION" == "both" ]; then
            gnome-terminal --title="DHCPv6 Server" -- bash -c "cd '$SCRIPT_DIR' && ./start_dhcpv6_server.sh; exec bash"
            sleep 2
            gnome-terminal --title="DHCPv6 Client" -- bash -c "cd '$SCRIPT_DIR' && ./test_dhcpv6_client.sh $INTERFACE; exec bash"
        fi
        ;;
    konsole)
        if [ "$VERSION" == "v4" ] || [ "$VERSION" == "both" ]; then
            konsole --new-tab -e bash -c "cd '$SCRIPT_DIR' && ./start_dhcpv4_server.sh; exec bash" &
            sleep 2
            konsole --new-tab -e bash -c "cd '$SCRIPT_DIR' && ./test_dhcpv4_client.sh $INTERFACE; exec bash" &
        fi
        if [ "$VERSION" == "v6" ] || [ "$VERSION" == "both" ]; then
            konsole --new-tab -e bash -c "cd '$SCRIPT_DIR' && ./start_dhcpv6_server.sh; exec bash" &
            sleep 2
            konsole --new-tab -e bash -c "cd '$SCRIPT_DIR' && ./test_dhcpv6_client.sh $INTERFACE; exec bash" &
        fi
        ;;
    *)
        echo "Terminal $TERMINAL not fully supported."
        ;;
esac

echo "Terminals launched!"
