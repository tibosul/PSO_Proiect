# DHCP Cross-Machine Testing Quick Guide

This is a quick reference guide for testing DHCP functionality when server and clients are on **different physical computers**.

## Prerequisites Checklist

- [ ] Two or more computers on the same network
- [ ] Project built on all computers: `cd DHCP_Server && make all`
- [ ] Root access available: `sudo` command works
- [ ] Firewall allows UDP ports 67 (server) and 68 (client)
- [ ] Computers can ping each other

## Quick Test Scenarios

### Scenario 1: Two Computers, Same Subnet (Easiest)

**Setup:**
```
Computer A (Server): 192.168.1.10 / eth0
Computer B (Client): No IP yet / eth0
Network: 192.168.1.0/24
```

**Steps:**

1. **On Computer A (Server):**
   ```bash
   cd DHCP_Server
   
   # Verify configuration includes your subnet
   grep "subnet 192.168.1.0" DHCPv4/config/dhcpv4.conf
   
   # Start server
   sudo ./scripts/test_network.sh server eth0
   ```

2. **On Computer B (Client):**
   ```bash
   cd DHCP_Server
   
   # Option 1: Broadcast mode (auto-discover server)
   sudo ./scripts/test_network.sh client eth0
   
   # Option 2: Unicast mode (direct to server)
   sudo ./scripts/test_network.sh client eth0 192.168.1.10
   ```

3. **Verify:**
   - Client should receive IP in range 192.168.1.100-200
   - Check logs: `tail -f DHCP_Server/logs/dhcpv4_*.log`
   - Check client IP: `ip addr show eth0`

### Scenario 2: Virtual Machines (Recommended for Testing)

**Using VirtualBox/VMware:**

1. **Configure Network:**
   - VM1 (Server): Bridged Adapter or Internal Network
   - VM2 (Client): Same network as VM1
   - Both VMs should be on same virtual subnet

2. **On VM1:**
   ```bash
   cd DHCP_Server
   sudo ./scripts/test_network.sh server eth0
   ```

3. **On VM2:**
   ```bash
   cd DHCP_Server
   sudo ./scripts/test_network.sh client eth0 <VM1_IP>
   ```

### Scenario 3: Docker Containers

**Create isolated test environment:**

```bash
# Create network
docker network create --subnet=192.168.100.0/24 dhcp-test

# Terminal 1 - Server Container
docker run -it --rm --privileged \
  --network dhcp-test --ip 192.168.100.10 \
  -v $(pwd):/app -w /app/DHCP_Server \
  ubuntu:22.04 bash

# In server container:
apt update && apt install -y gcc make iproute2
make all
sudo ./scripts/test_network.sh server eth0

# Terminal 2 - Client Container  
docker run -it --rm --privileged \
  --network dhcp-test \
  -v $(pwd):/app -w /app/DHCP_Server \
  ubuntu:22.04 bash

# In client container:
apt update && apt install -y gcc make iproute2
make all
sudo ./scripts/test_network.sh client eth0 192.168.100.10
```

### Scenario 4: Raspberry Pi or IoT Devices

**Server on Raspberry Pi, Client on PC (or vice versa):**

1. **On Raspberry Pi (Server):**
   ```bash
   cd DHCP_Server
   
   # Check interface name (might be wlan0 for WiFi)
   ip -br link show
   
   # Start server
   sudo ./scripts/test_network.sh server wlan0
   ```

2. **On PC (Client):**
   ```bash
   cd DHCP_Server
   sudo ./scripts/test_network.sh client wlan0 <RaspberryPi_IP>
   ```

## Common Issues and Solutions

### Issue: Client not receiving IP

**Check 1 - Network connectivity:**
```bash
# On client, ping server
ping <server_ip>
```

**Check 2 - Server is running:**
```bash
# On server
ps aux | grep dhcpv4_server
```

**Check 3 - Firewall:**
```bash
# On server (Ubuntu)
sudo ufw allow 67/udp
sudo ufw allow 68/udp

# On server (RHEL/CentOS)
sudo firewall-cmd --add-port=67/udp --permanent
sudo firewall-cmd --reload
```

**Check 4 - Configuration includes client subnet:**
```bash
# On server
cat DHCPv4/config/dhcpv4.conf | grep -A 5 "subnet"
```

**Check 5 - Try unicast instead of broadcast:**
```bash
# On client
sudo ./scripts/test_network.sh client eth0 <server_ip>
```

### Issue: Server won't start

**Check port availability:**
```bash
sudo netstat -ulnp | grep :67
# If port is used, kill other DHCP server:
sudo killall dhcpd dhcpv4_server
```

**Check configuration syntax:**
```bash
cat logs/dhcpv4_server.log
```

### Issue: Different subnets (need relay)

If server and client are on different subnets, you need a DHCP relay:

```bash
# On router between subnets (Linux)
sudo apt-get install isc-dhcp-relay
sudo nano /etc/default/isc-dhcp-relay
# Set: SERVERS="<server_ip>"
# Set: INTERFACES="eth0 eth1"
sudo systemctl restart isc-dhcp-relay
```

## Verification Commands

**On Server:**
```bash
# Check server is listening
sudo netstat -ulnp | grep dhcpv4_server

# View server log
tail -f logs/dhcpv4_server.log

# Check lease database
cat DHCPv4/data/dhcpv4.leases

# Count active leases
grep "binding state active" DHCPv4/data/dhcpv4.leases | wc -l
```

**On Client:**
```bash
# Check assigned IP
ip addr show eth0

# View client log
tail -f logs/dhcpv4_client.log

# Test connectivity with assigned IP
ping 8.8.8.8
```

**Network Capture:**
```bash
# Capture DHCP traffic on server
sudo tcpdump -i eth0 -n port 67 or port 68

# On client
sudo tcpdump -i eth0 -n port 67 or port 68
```

## Configuration for Different Networks

Edit `DHCPv4/config/dhcpv4.conf` to match your network:

**Example: Home Network (192.168.0.0/24)**
```
subnet 192.168.0.0 netmask 255.255.255.0 {
    range 192.168.0.100 192.168.0.200;
    option routers 192.168.0.1;
    option subnet-mask 255.255.255.0;
    option domain-name-servers 8.8.8.8, 1.1.1.1;
    default-lease-time 3600;
    max-lease-time 7200;
}
```

**Example: Office Network (10.0.0.0/8)**
```
subnet 10.0.0.0 netmask 255.0.0.0 {
    range 10.0.0.100 10.0.0.254;
    option routers 10.0.0.1;
    option subnet-mask 255.0.0.0;
    option domain-name-servers 10.0.0.1, 8.8.8.8;
}
```

After editing config, restart the server:
```bash
sudo killall dhcpv4_server
sudo ./scripts/test_network.sh server eth0
```

## Testing Multiple Clients

**On same computer (loopback):**
```bash
sudo ./scripts/test_loopback.sh
```

**On different computers:**
Just run the client command on each computer:
```bash
# Computer B
sudo ./scripts/test_network.sh client eth0 <server_ip>

# Computer C
sudo ./scripts/test_network.sh client eth0 <server_ip>

# Computer D
sudo ./scripts/test_network.sh client eth0 <server_ip>
```

Each will get a unique IP from the pool.

## Manual Testing (Without Scripts)

**Server:**
```bash
cd DHCP_Server
sudo ./build/bin/dhcpv4_server
```

**Client:**
```bash
cd DHCP_Server
# Broadcast mode:
sudo ./build/bin/dhcpv4_client eth0

# Unicast mode:
sudo ./build/bin/dhcpv4_client eth0 -s <server_ip>
```

## Summary

**Easiest path for cross-machine testing:**

1. Get two computers on same WiFi/LAN
2. Note the server's IP address
3. Run server: `sudo ./scripts/test_network.sh server <interface>`
4. Run client: `sudo ./scripts/test_network.sh client <interface> <server_ip>`
5. Check logs and verify client received IP

**If that doesn't work:**
- Check firewall settings
- Verify computers can ping each other
- Try different network interfaces
- Check configuration includes correct subnet
- Review troubleshooting section above

For complete details, see **TESTING.md**.
