# DHCP Server Suite

A comprehensive DHCPv4 and DHCPv6 server and client implementation in C.

## Features

- **DHCPv4 Server**: Full RFC 2131 compliant DHCP server
- **DHCPv6 Server**: IPv6 DHCP support
- **DHCPv4 Client**: Complete DHCP client with lease management
- **DHCPv6 Client**: IPv6 client implementation
- **Multiple Network Support**: Loopback, LAN, VLAN, and cross-machine configurations
- **Lease Management**: Persistent lease database with renewal support
- **Logging**: Comprehensive logging system for debugging and monitoring

## Quick Start

### Build

```bash
cd DHCP_Server
make clean
make all
```

This builds:
- `build/bin/dhcpv4_server` - DHCPv4 server
- `build/bin/dhcpv4_client` - DHCPv4 client
- `build/bin/dhcpv6_server` - DHCPv6 server
- `build/bin/dhcpv6_client` - DHCPv6 client
- `build/bin/dhcpv6_monitor` - DHCPv6 monitoring tool

### Verify Setup

```bash
./scripts/verify_setup.sh
```

### Test Loopback (Single Machine)

```bash
sudo ./scripts/test_loopback.sh
```

### Test Network (Multiple Machines)

**On Server Computer:**
```bash
sudo ./scripts/test_network.sh server eth0
```

**On Client Computer:**
```bash
sudo ./scripts/test_network.sh client eth0 <server_ip>
```

## Documentation

- **[TESTING.md](TESTING.md)** - Complete testing guide with detailed scenarios
- **[CROSS_MACHINE_TESTING.md](CROSS_MACHINE_TESTING.md)** - Quick reference for testing across different computers

## Testing Scenarios Covered

### ✅ Loopback Testing
Tests DHCP functionality on a single computer using the loopback interface (127.0.0.1).

**Use case:** Quick validation, development, debugging

**Command:**
```bash
sudo ./scripts/test_loopback.sh
```

**What it tests:**
- Server startup and configuration parsing
- Multiple clients with different MAC addresses
- DHCP handshake (DISCOVER → OFFER → REQUEST → ACK)
- Lease database creation and management
- Concurrent client handling

### ✅ Network Testing (Same Subnet)
Tests DHCP when server and clients are on the same physical network.

**Use case:** Real-world LAN deployment, production testing

**Setup:**
- Computer A (Server): 192.168.1.10
- Computer B (Client): Gets IP from DHCP
- Network: 192.168.1.0/24

**Commands:**
```bash
# Server
sudo ./scripts/test_network.sh server eth0

# Client (broadcast mode)
sudo ./scripts/test_network.sh client eth0

# Client (unicast mode)
sudo ./scripts/test_network.sh client eth0 192.168.1.10
```

### ✅ Virtual Machine Testing
Tests DHCP across VMs with isolated networks.

**Use case:** Safe testing environment, development

**Supported:**
- VirtualBox with bridged/internal network
- VMware with custom virtual networks
- KVM/QEMU with bridge networking

### ✅ Docker Container Testing
Tests DHCP in containerized environments.

**Use case:** CI/CD testing, reproducible environments

**Example:**
```bash
docker network create --subnet=192.168.100.0/24 dhcp-test
# Run server and client containers
```

### ✅ Cross-Subnet Testing (with DHCP Relay)
Tests DHCP when server and clients are on different subnets.

**Use case:** Enterprise networks with VLANs, routed networks

**Requirements:** DHCP relay agent on router

### ✅ Raspberry Pi / IoT Testing
Tests DHCP on embedded devices and IoT networks.

**Use case:** Home automation, IoT deployments

**Supported interfaces:** eth0, wlan0

## Configuration

Configuration file: `DHCPv4/config/dhcpv4.conf`

Pre-configured subnets:
- **127.0.0.0/8** - Loopback (testing)
- **192.168.1.0/24** - Corporate LAN
- **10.0.0.0/24** - Guest WiFi
- **10.10.0.0/24** - IoT devices
- **172.16.100.0/24** - VoIP phones
- **192.168.50.0/24** - Development/Testing
- **203.0.113.0/28** - DMZ

Edit the configuration to match your network topology.

## Scripts

- `scripts/run_server_v4.sh` - Start DHCPv4 server
- `scripts/run_client_v4.sh` - Start DHCPv4 client
- `scripts/test_loopback.sh` - Automated loopback testing
- `scripts/test_network.sh` - Network testing helper
- `scripts/verify_setup.sh` - System verification
- `scripts/logs.sh` - View logs
- `scripts/monitor_v6.sh` - Monitor DHCPv6

## Logs

All logs are saved to `logs/` directory:
- `logs/dhcpv4_server.log` - Server events
- `logs/dhcpv4_client.log` - Client events
- `logs/dhcpv6_server.log` - DHCPv6 server
- `logs/dhcpv6_client.log` - DHCPv6 client

View logs:
```bash
tail -f logs/dhcpv4_server.log
./scripts/logs.sh
```

## Troubleshooting

### Server won't start
```bash
# Check if port 67 is in use
sudo netstat -ulnp | grep :67

# Kill conflicting DHCP servers
sudo killall dhcpd dhcpv4_server

# Check configuration
cat logs/dhcpv4_server.log
```

### Client not receiving IP
```bash
# Verify server is running
ps aux | grep dhcpv4_server

# Check firewall
sudo ufw allow 67/udp
sudo ufw allow 68/udp

# Try unicast mode
sudo ./build/bin/dhcpv4_client eth0 -s <server_ip>

# Capture packets
sudo tcpdump -i eth0 port 67 or port 68
```

### Network issues
```bash
# Check interface
ip -br link show

# Check connectivity
ping <server_ip>

# Check routing
ip route
```

See [TESTING.md](TESTING.md) for complete troubleshooting guide.

## Project Structure

```
DHCP_Server/
├── build/              # Build artifacts
│   ├── bin/           # Compiled binaries
│   └── obj/           # Object files
├── DHCPv4/            # DHCPv4 server implementation
│   ├── config/        # Configuration files
│   ├── data/          # Lease database
│   ├── include/       # Header files
│   ├── src/           # Source code
│   └── utils/         # Utility functions
├── DHCPv6/            # DHCPv6 server implementation
│   ├── config/
│   ├── include/
│   ├── leases/
│   ├── monitor/
│   └── sources/
├── client/            # DHCP client implementations
│   ├── client_v4.c
│   └── client_v6.c
├── logger/            # Logging system
├── scripts/           # Helper scripts
└── logs/             # Log files
```

## Requirements

- **OS:** Linux (Ubuntu, Debian, RHEL, CentOS, etc.)
- **Compiler:** GCC with C11 support
- **Build:** GNU Make
- **Runtime:** Root access (sudo) for binding to privileged ports
- **Network:** Network interfaces for testing

## License

See project root for license information.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly (use test scripts)
5. Submit a pull request

## Support

For issues and questions:
- Check [TESTING.md](TESTING.md) for detailed documentation
- Check [CROSS_MACHINE_TESTING.md](CROSS_MACHINE_TESTING.md) for cross-machine scenarios
- Review logs in `logs/` directory
- Run `./scripts/verify_setup.sh` to check system status

## Authors

Project for PSO (Proiect Sisteme de Operare)
