#!/bin/bash
# Script to launch multiple DHCPv6 clients with spoofed MAC addresses

IFACE=$1
COUNT=$2

if [ -z "$IFACE" ] || [ -z "$COUNT" ]; then
    echo "Usage: $0 <interface> <count>"
    echo "Example: $0 enp0s3 5"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo $0 ...)"
  exit 1
fi

echo "Launching $COUNT clients on $IFACE..."

for i in $(seq 1 $COUNT); do
    # Spoof byte in hex (e.g. 1 -> 01, 10 -> 0A)
    SPOOF=$(printf "%02X" $i)
    
    echo "Starting Client $i (Spoof: $SPOOF) ..."
    # Run in background, requesting PD (-P) and Spoofing (-S)
    ./bin/dhcpv6_client $IFACE -P -S $SPOOF > client_$i.log 2>&1 &
    
    # Small delay to avoid packet collision storm
    sleep 0.5
done

echo "All clients started. Check client_N.log for output."
echo "Use 'pgrep dhcpv6_client' to list PIDs."
echo "To kill all: sudo pkill dhcpv6_client"
