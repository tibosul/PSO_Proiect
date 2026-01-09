#!/bin/bash
# Test multiple DHCPv4 clients simultaneously
cd "$(dirname "$0")/.."

NUM_CLIENTS=${1:-3}
SERVER_PORT=${2:-67}

echo "=============================================="
echo "  DHCPv4 Multi-Client Test"
echo "=============================================="
echo "Clients: $NUM_CLIENTS"
echo "Server port: $SERVER_PORT"
echo ""

# Clear old logs
> logs/dhcpv4_server.log
> logs/dhcpv4_client.log

# Start server in background
echo "[*] Starting DHCPv4 Server..."
sudo ./build/bin/dhcpv4_server &
SERVER_PID=$!
sleep 2

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "[!] Server failed to start"
    exit 1
fi
echo "[+] Server running (PID: $SERVER_PID)"
echo ""

# Array to store client PIDs
declare -a CLIENT_PIDS

# Start multiple clients with different MACs and ports
echo "[*] Starting $NUM_CLIENTS clients..."
for i in $(seq 1 $NUM_CLIENTS); do
    # Generate unique MAC for each client
    MAC=$(printf "aa:bb:cc:dd:ee:%02x" $i)
    # Each client uses different port (68, 69, 70, etc.)
    CLIENT_PORT=$((67 + i))

    echo "    Client $i: MAC=$MAC, Port=$CLIENT_PORT"

    sudo ./build/bin/dhcpv4_client lo -s 127.0.0.1 -p $SERVER_PORT -m $MAC -c $CLIENT_PORT &
    CLIENT_PIDS+=($!)

    # Small delay between client starts
    sleep 0.5
done

echo ""
echo "[*] Waiting for DHCP handshakes to complete..."
sleep 8

# Kill all clients (they would loop forever otherwise)
echo ""
echo "[*] Stopping clients..."
for pid in "${CLIENT_PIDS[@]}"; do
    sudo kill $pid 2>/dev/null
done

# Stop server
echo "[*] Stopping server..."
sudo kill $SERVER_PID 2>/dev/null

sleep 1

# Show results
echo ""
echo "=============================================="
echo "  RESULTS"
echo "=============================================="
echo ""
echo "--- Server Log (IP allocations) ---"
grep -E "(OFFER|ACK|DISCOVER|REQUEST)" logs/dhcpv4_server.log | tail -20
echo ""
echo "--- Client Log (IPs received) ---"
grep -E "(OFFER RECEIVED|ACK RECEIVED|SUCCESS)" logs/dhcpv4_client.log | tail -20
echo ""
echo "--- Summary ---"
OFFERS=$(grep -c ">>> OFFER:" logs/dhcpv4_server.log 2>/dev/null || echo 0)
ACKS=$(grep -c ">>> ACK:" logs/dhcpv4_server.log 2>/dev/null || echo 0)
RECEIVED=$(grep -c "<<< ACK RECEIVED" logs/dhcpv4_client.log 2>/dev/null || echo 0)
echo "Server sent $OFFERS OFFERs and $ACKS ACKs"
echo "Clients received $RECEIVED IP addresses"
echo ""

# Show unique IPs allocated
echo "--- Unique IPs Allocated ---"
grep ">>> OFFER:" logs/dhcpv4_server.log | grep -oP 'IP \K[0-9.]+' | sort -u
echo ""
echo "=============================================="
