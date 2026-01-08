# DHCP Server Testing Guide

This guide explains how to verify the functionality of the DHCP servers (DHCPv4 and DHCPv6) with clients in different scenarios.

## Table of Contents
- [Quick Start](#quick-start)
- [Loopback Testing](#loopback-testing)
- [Network Testing (Different Computers)](#network-testing-different-computers)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)

## Quick Start

### Build Everything
```bash
cd DHCP_Server
make clean
make all
```

This will build:
- `build/bin/dhcpv4_server` - DHCPv4 server
- `build/bin/dhcpv4_client` - DHCPv4 client
- `build/bin/dhcpv6_server` - DHCPv6 server
- `build/bin/dhcpv6_client` - DHCPv6 client
- `build/bin/dhcpv6_monitor` - DHCPv6 monitoring tool

## Loopback Testing

Loopback testing allows you to test DHCP functionality on a single computer using the `lo` (loopback) interface.

### Automated Loopback Test

Run the comprehensive loopback test script:

```bash
cd DHCP_Server
sudo ./scripts/test_loopback.sh
```

This script will:
1. Start a DHCPv4 server on loopback (127.0.0.1)
2. Launch 3 clients with different MAC addresses and ports
3. Verify DHCP handshake (DISCOVER → OFFER → REQUEST → ACK)
4. Check lease database
5. Display results and cleanup

### Manual Loopback Testing

**Terminal 1 - Start Server:**
```bash
cd DHCP_Server
sudo ./scripts/run_server_v4.sh
```

**Terminal 2 - Start Client 1:**
```bash
cd DHCP_Server
sudo ./scripts/run_client_v4.sh lo -s 127.0.0.1
```

**Terminal 3 - Start Client 2 (optional):**
```bash
cd DHCP_Server
sudo ./scripts/run_client_v4.sh lo -s 127.0.0.1 -m aa:bb:cc:dd:ee:02 -c 69
```

**View Logs:**
```bash
tail -f logs/dhcpv4_server.log
tail -f logs/dhcpv4_client.log
```

### What Happens in Loopback Mode?

- **Server**: Listens on 0.0.0.0:67 and serves the `127.0.0.0/8` subnet
- **Client**: Connects to 127.0.0.1:67 using unicast (not broadcast)
- **IP Range**: Server assigns IPs from 127.0.0.100 - 127.0.0.200
- **Multiple Clients**: Each client needs unique MAC address (`-m`) and port (`-c`)

## Network Testing (Different Computers)

This section explains how to test DHCP when server and clients are on **different physical computers** connected via a network.

### Prerequisites

1. **Two or more computers** on the same network (wired or wireless)
2. **Network topology**: Computers must be on the same subnet (e.g., 192.168.1.0/24)
3. **Build the project** on all computers (or copy binaries)
4. **Root access** on all computers (DHCP requires privileged ports)

### Scenario 1: Server and Client on Same Subnet

**Network Setup:**
```
Computer A (Server): 192.168.1.10 (eth0)
Computer B (Client): (no IP yet) (eth0)
Same subnet: 192.168.1.0/24
```

**On Computer A (Server):**

1. Edit configuration to match your network:
```bash
cd DHCP_Server
nano DHCPv4/config/dhcpv4.conf
```

Ensure you have a subnet matching your network:
```
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
    option subnet-mask 255.255.255.0;
    option domain-name-servers 8.8.8.8, 1.1.1.1;
}
```

2. Start the DHCP server:
```bash
sudo ./scripts/test_network.sh server eth0
# Replace eth0 with your network interface name
```

**On Computer B (Client):**

1. **Option A - Broadcast mode** (discovers any DHCP server):
```bash
cd DHCP_Server
sudo ./scripts/test_network.sh client eth0
```

2. **Option B - Unicast mode** (connects to specific server):
```bash
cd DHCP_Server
sudo ./scripts/test_network.sh client eth0 192.168.1.10
```

### Scenario 2: Testing Across VLANs or Subnets

If server and client are on **different subnets**, you need a **DHCP relay agent** on the router.

**Network Setup:**
```
Subnet A: 192.168.1.0/24 (Server here)
  └─ Computer A (Server): 192.168.1.10
  
Subnet B: 192.168.2.0/24 (Client here)
  └─ Computer B (Client): (no IP yet)
  
Router: Routes between subnets, runs DHCP relay
```

**Router Configuration (Example for Linux):**
```bash
# Install DHCP relay
sudo apt-get install isc-dhcp-relay

# Configure relay
sudo nano /etc/default/isc-dhcp-relay
# Set: SERVERS="192.168.1.10"
# Set: INTERFACES="eth0 eth1"

sudo systemctl restart isc-dhcp-relay
```

**Server Configuration:**

Add the remote subnet to `dhcpv4.conf`:
```
subnet 192.168.2.0 netmask 255.255.255.0 {
    range 192.168.2.100 192.168.2.200;
    option routers 192.168.2.1;
    option subnet-mask 255.255.255.0;
}
```

### Scenario 3: Testing with Virtual Machines

**Setup:**
1. Create two VMs with **bridged network** adapters
2. Ensure both VMs are on the same virtual network

**On VM 1 (Server):**
```bash
cd DHCP_Server
sudo ./scripts/test_network.sh server eth0
```

**On VM 2 (Client):**
```bash
cd DHCP_Server
sudo ./scripts/test_network.sh client eth0 <VM1_IP>
```

### Scenario 4: Testing on Docker/Containers

**Create a Docker network:**
```bash
docker network create --subnet=192.168.10.0/24 dhcp-test-net
```

**Server Container:**
```bash
docker run -it --rm --privileged \
  --network dhcp-test-net \
  --ip 192.168.10.10 \
  -v $(pwd):/app \
  ubuntu:22.04 bash

# Inside container:
cd /app/DHCP_Server
make all
sudo ./scripts/test_network.sh server eth0
```

**Client Container:**
```bash
docker run -it --rm --privileged \
  --network dhcp-test-net \
  -v $(pwd):/app \
  ubuntu:22.04 bash

# Inside container:
cd /app/DHCP_Server
make all
sudo ./scripts/test_network.sh client eth0 192.168.10.10
```

## Configuration

### Server Configuration File

Location: `DHCPv4/config/dhcpv4.conf`

The configuration file includes several pre-configured networks:

1. **Loopback (127.0.0.0/8)** - For local testing
2. **Corporate LAN (192.168.1.0/24)** - Main office network
3. **Guest WiFi (10.0.0.0/24)** - Guest network
4. **IoT Network (10.10.0.0/24)** - Smart devices
5. **VoIP (172.16.100.0/24)** - Phone network
6. **Development (192.168.50.0/24)** - Dev/test environment
7. **DMZ (203.0.113.0/28)** - Public-facing servers

**Important Settings:**
- `authoritative` - Server is authoritative (sends NAKs)
- `ping-check false` - Disabled for loopback (all 127.x respond to ping)
- `default-lease-time` - Default lease duration (seconds)
- `max-lease-time` - Maximum lease duration (seconds)

### Client Options

The client supports several options:

```bash
./build/bin/dhcpv4_client <interface> [options]

Options:
  -s <ip>    : Server IP (enables unicast mode)
  -p <port>  : Server port (default: 67)
  -m <mac>   : Custom MAC address (format: aa:bb:cc:dd:ee:ff)
  -c <port>  : Client port (default: 68)
```

**Examples:**

```bash
# Normal broadcast mode
sudo ./build/bin/dhcpv4_client eth0

# Unicast to specific server
sudo ./build/bin/dhcpv4_client eth0 -s 192.168.1.10

# Multiple clients on same machine
sudo ./build/bin/dhcpv4_client lo -s 127.0.0.1 -m aa:bb:cc:dd:ee:01 -c 68
sudo ./build/bin/dhcpv4_client lo -s 127.0.0.1 -m aa:bb:cc:dd:ee:02 -c 69
sudo ./build/bin/dhcpv4_client lo -s 127.0.0.1 -m aa:bb:cc:dd:ee:03 -c 70
```

## Troubleshooting

### Server Won't Start

**Symptom:** Server exits immediately or fails to bind

**Causes:**
1. Port 67 already in use (another DHCP server running)
2. No matching subnet configuration
3. Permission denied (not running as root)

**Solutions:**
```bash
# Check if port 67 is in use
sudo netstat -ulnp | grep :67

# Kill other DHCP servers
sudo killall dhcpd dhcpv4_server

# Check permissions
sudo ./scripts/run_server_v4.sh

# Verify configuration
cat DHCPv4/config/dhcpv4.conf

# Check logs
tail -100 logs/dhcpv4_server.log
```

### Client Not Receiving IP

**Symptom:** Client sends DISCOVER but no response

**Causes:**
1. Server not running or on wrong subnet
2. Firewall blocking UDP ports 67/68
3. Client and server on different subnets without relay
4. Network interface not up

**Solutions:**
```bash
# Verify server is running
ps aux | grep dhcpv4_server

# Check firewall (Ubuntu/Debian)
sudo ufw status
sudo ufw allow 67/udp
sudo ufw allow 68/udp

# Check firewall (RHEL/CentOS)
sudo firewall-cmd --list-all
sudo firewall-cmd --add-port=67/udp --permanent
sudo firewall-cmd --add-port=68/udp --permanent
sudo firewall-cmd --reload

# Verify interface is up
ip link show eth0
sudo ip link set eth0 up

# Check routing
ip route

# Capture packets to verify communication
sudo tcpdump -i eth0 -n port 67 or port 68
```

### Lease Not Appearing in Database

**Symptom:** Client gets IP but lease file is empty

**Causes:**
1. Lease directory doesn't exist
2. Permission issues
3. Disk full

**Solutions:**
```bash
# Create lease directory
mkdir -p DHCPv4/data

# Check permissions
ls -la DHCPv4/data/

# Check disk space
df -h

# Verify lease file location
cat logs/dhcpv4_server.log | grep lease
```

### Network Testing Issues

**Symptom:** Works on loopback but not on network

**Causes:**
1. Broadcast not working (need unicast `-s` option)
2. Different subnet (need relay agent)
3. Switch blocking DHCP
4. Wrong interface selected

**Solutions:**
```bash
# List available interfaces
ip -br link show

# Check interface IP
ip -br addr show eth0

# Try unicast instead of broadcast
sudo ./build/bin/dhcpv4_client eth0 -s <server_ip>

# Verify network connectivity
ping <server_ip>

# Check if broadcasts are working
# On server:
sudo tcpdump -i eth0 -n broadcast

# On client:
ping -b 192.168.1.255
```

## Viewing Logs

All components log to the `logs/` directory:

```bash
# Server logs
tail -f logs/dhcpv4_server.log

# Client logs
tail -f logs/dhcpv4_client.log

# View all DHCP-related logs
tail -f logs/dhcpv*.log

# Search for specific events
grep "DHCPACK" logs/dhcpv4_server.log
grep "BOUND" logs/dhcpv4_client.log
```

## Advanced Testing

### Stress Testing (Many Clients)

```bash
#!/bin/bash
# Start 10 clients with different MACs
for i in {1..10}; do
    MAC="aa:bb:cc:dd:ee:$(printf '%02x' $i)"
    PORT=$((68 + $i))
    sudo ./build/bin/dhcpv4_client lo -s 127.0.0.1 -m "$MAC" -c "$PORT" &
done
```

### Lease Renewal Testing

The client automatically renews leases when T1 timer expires (typically 50% of lease time).

Watch the logs to see renewal:
```bash
tail -f logs/dhcpv4_client.log | grep -i renew
```

### IPv6 Testing

Similar procedures apply for DHCPv6:

```bash
# Start DHCPv6 server
sudo ./scripts/run_server_v6.sh

# Start DHCPv6 client
sudo ./scripts/run_client_v6.sh eth0
```

## Summary

- ✅ **Loopback testing**: Use `test_loopback.sh` for automated testing on one computer
- ✅ **Network testing**: Use `test_network.sh` for testing across different computers
- ✅ **Multiple clients**: Use different MACs (`-m`) and ports (`-c`) on same interface
- ✅ **Configuration**: Edit `dhcpv4.conf` to match your network topology
- ✅ **Troubleshooting**: Check logs, firewall, network connectivity, and permissions

For more details, see the source code and configuration files.
