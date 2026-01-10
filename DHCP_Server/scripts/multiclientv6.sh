#!/bin/bash
# Test multiple DHCPv6 clients simultaneously
# Usage: ./multiclientv6.sh <interface> [num_clients]

cd "$(dirname "$0")/.."

IF="${1:-lo}"
NUM_CLIENTS=${2:-3}

echo "=============================================="
echo "  DHCPv6 Multi-Client Test"
echo "=============================================="
echo "Interface: $IF"
echo "Clients:   $NUM_CLIENTS"
echo ""

# Create logs directory
mkdir -p logs

# Array to store client PIDs
declare -a CLIENT_PIDS

echo "[*] Starting $NUM_CLIENTS clients on $IF..."

for i in $(seq 1 $NUM_CLIENTS); do
    # Hex byte for spoofing (01, 02, .. 0A, ..)
    SPOOF_BYTE=$(printf "%02X" $i)
    
    echo "    Starting Client $i (Spoof Byte: $SPOOF_BYTE)..."
    
    # Run client in background with spoofing
    # Output to a unique log or shared log? The client app logs to logs/dhcpv6_client.log by default.
    # To avoid log collision garble, we might want to redirect stdout/stderr too, but the app uses internal logger.
    # We will rely on the app's internal appending logger, hoping for no race corruption (O_APPEND).
    
    sudo ./build/bin/dhcpv6_client "$IF" -S "$SPOOF_BYTE" &
    PID=$!
    CLIENT_PIDS+=($PID)
    
    # Small delay to prevent network storm
    sleep 0.5
done

echo ""
echo "[*] Clients running. Waiting 10 seconds for transactions..."
sleep 10

echo ""
echo "[*] Checking Server Log for Activity..."
echo "--------------------------------------------------------"
grep "v6 add IA_NA" logs/dhcpv6_server.log | tail -n $NUM_CLIENTS
echo "--------------------------------------------------------"

echo ""
echo "[*] Stopping clients..."
for pid in "${CLIENT_PIDS[@]}"; do
    sudo kill $pid 2>/dev/null
done
echo "Done."
