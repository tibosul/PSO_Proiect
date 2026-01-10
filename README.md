# PSO Proiect - DHCP Server Implementation

A comprehensive implementation of DHCPv4 and DHCPv6 servers with clients, developed in C for the Operating Systems course (Proiect Sisteme de Operare).

## Project Components

### 1. DHCP Server Suite (`DHCP_Server/`)

Complete implementation of DHCP servers and clients:
- **DHCPv4 Server**: RFC 2131 compliant DHCP server
- **DHCPv6 Server**: RFC 8415 compliant IPv6 DHCP server
- **DHCPv4 Client**: Full DHCP client with lease management
- **DHCPv6 Client**: IPv6 client implementation
- **Multiple Network Support**: Loopback, LAN, VLAN, and cross-machine configurations

See [DHCP_Server/README.md](DHCP_Server/README.md) for detailed information.

### 2. DNS Server (`DNS_Server/`)

DNS server implementation (separate component).

See [DNS_Server/README.md](DNS_Server/README.md) for details.

### 3. Documentation (`documentation/`)

IEEE-format academic documentation covering the DHCP implementation:
- Comprehensive LaTeX documentation
- Architecture and design details
- Implementation specifics
- Testing and deployment guide

See [documentation/README.md](documentation/README.md) for compilation instructions.

## Quick Start

### Build DHCP Servers

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

### Test DHCP on Loopback

```bash
cd DHCP_Server
sudo ./scripts/test_loopback.sh
```

### Compile Documentation

```bash
cd documentation
pdflatex dhcp_servers_documentation.tex
```

Or use [Overleaf](https://www.overleaf.com/) for online compilation.

## Features

### DHCPv4 Features
- Full DORA (Discover, Offer, Request, Acknowledge) handshake
- Multiple subnet support with individual configuration
- Static host reservations based on MAC addresses
- Lease management with persistent storage
- Configurable lease times (default and maximum)
- DNS server configuration (DHCP option 6)
- Router/gateway configuration (DHCP option 3)
- Domain name assignment
- NTP server support
- Broadcast and unicast communication modes

### DHCPv6 Features
- SOLICIT, ADVERTISE, REQUEST, REPLY message exchange
- IPv6 multicast support (ff02::1:2)
- DUID-based client identification
- Prefix delegation (IA_PD) for routing scenarios
- Identity Associations (IA_NA, IA_TA, IA_PD)
- Stateful and stateless configuration support
- Multiple subnet definitions
- DNS servers and domain search lists
- SNTP server support

### Common Features
- Comprehensive logging system
- Multiple network topology support
- Automated testing scripts
- Lease renewal and expiration handling
- Configuration file parsing (ISC DHCP syntax)
- Cross-platform Linux support

## Testing

The implementation includes extensive testing capabilities:

### Loopback Testing
Test on a single computer using the loopback interface:
```bash
sudo ./scripts/test_loopback.sh
```

### Network Testing
Test across multiple computers:
```bash
# On server computer
sudo ./scripts/test_network.sh server eth0

# On client computer
sudo ./scripts/test_network.sh client eth0 <server_ip>
```

### Supported Test Scenarios
- ✅ Loopback (single machine)
- ✅ Local Area Network (same subnet)
- ✅ Virtual Machine environments
- ✅ Docker containers
- ✅ Cross-subnet with DHCP relay
- ✅ Raspberry Pi / IoT devices

See [DHCP_Server/TESTING.md](DHCP_Server/TESTING.md) for complete testing documentation.

## Requirements

### Build Requirements
- **OS**: Linux (Ubuntu, Debian, RHEL, CentOS, etc.)
- **Compiler**: GCC with C11 support
- **Build System**: GNU Make
- **Runtime**: Root access (sudo) for privileged ports

### Documentation Requirements
- LaTeX distribution (TeX Live, MiKTeX, or MacTeX)
- IEEEtran document class
- Standard LaTeX packages (included in most distributions)

## Project Structure

```
PSO_Proiect/
├── DHCP_Server/              # DHCP implementation
│   ├── DHCPv4/              # DHCPv4 server source
│   ├── DHCPv6/              # DHCPv6 server source
│   ├── client/              # Client implementations
│   ├── logger/              # Logging system
│   ├── scripts/             # Testing and deployment scripts
│   ├── logs/                # Runtime logs
│   ├── build/               # Build artifacts
│   └── README.md            # DHCP documentation
├── DNS_Server/              # DNS server implementation
├── documentation/           # IEEE-format documentation
│   ├── dhcp_servers_documentation.tex  # LaTeX source
│   ├── dhcp_servers_documentation.pdf  # Compiled PDF
│   └── README.md            # Documentation guide
├── DHCP_VERIFICATION.md     # Verification procedures
└── README.md               # This file
```

## Documentation

- **[DHCP_Server/README.md](DHCP_Server/README.md)** - DHCP implementation guide
- **[DHCP_Server/TESTING.md](DHCP_Server/TESTING.md)** - Comprehensive testing guide
- **[DHCP_Server/CROSS_MACHINE_TESTING.md](DHCP_Server/CROSS_MACHINE_TESTING.md)** - Cross-machine testing reference
- **[documentation/](documentation/)** - IEEE-format academic documentation
- **[DHCP_VERIFICATION.md](DHCP_VERIFICATION.md)** - Verification procedures

## Configuration

### DHCPv4 Configuration
Configuration file: `DHCP_Server/DHCPv4/config/dhcpv4.conf`

Supported options:
- Global parameters (lease times, DNS servers)
- Multiple subnets with address ranges
- Static host reservations
- Router and gateway configuration
- DNS and domain name settings
- NTP server configuration

### DHCPv6 Configuration
Configuration file: `DHCP_Server/DHCPv6/config/dhcpv6.conf`

Supported options:
- Global IPv6 parameters
- Multiple subnet6 definitions
- Address pools (range6)
- Prefix delegation blocks (prefix6)
- DUID-based static reservations
- DNS servers and search domains
- SNTP server configuration

## Logging

Comprehensive logging for debugging and monitoring:

### Log Files
- `logs/dhcpv4_server.log` - DHCPv4 server events
- `logs/dhcpv4_client.log` - DHCPv4 client events
- `logs/dhcpv6_server.log` - DHCPv6 server events
- `logs/dhcpv6_client.log` - DHCPv6 client events

### View Logs
```bash
# Follow server logs
tail -f logs/dhcpv4_server.log

# View all logs
./scripts/logs.sh
```

## Troubleshooting

### Server Won't Start
```bash
# Check if port 67 is in use
sudo netstat -ulnp | grep :67

# Kill conflicting DHCP servers
sudo killall dhcpd dhcpv4_server

# Check logs
cat logs/dhcpv4_server.log
```

### Client Not Receiving IP
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

## Academic Paper

A comprehensive IEEE-format academic paper is available in the `documentation/` directory. The paper covers:

1. Introduction and motivation
2. DHCPv4 server architecture and implementation
3. DHCPv6 server architecture and key differences
4. Implementation details and build system
5. Testing methodologies and deployment
6. Network deployment and security
7. Advanced features (prefix delegation, lease management)
8. Lessons learned and conclusions

To read the paper:
```bash
# View PDF
xdg-open documentation/dhcp_servers_documentation.pdf

# Or compile from source
cd documentation
pdflatex dhcp_servers_documentation.tex
```

## License

See project license file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly using provided test scripts
5. Submit a pull request

## Authors

Project for PSO (Proiect Sisteme de Operare) - Operating Systems Course

## References

- RFC 2131: Dynamic Host Configuration Protocol (DHCPv4)
- RFC 8415: Dynamic Host Configuration Protocol for IPv6 (DHCPv6)
- RFC 2132: DHCP Options and BOOTP Vendor Extensions
- RFC 3633: IPv6 Prefix Options for DHCPv6
- RFC 4361: Node-specific Client Identifiers for DHCPv4

## Support

For issues and questions:
- Review documentation in `DHCP_Server/TESTING.md`
- Check logs in `logs/` directory
- Run `./scripts/verify_setup.sh` to check system status
- Consult the academic paper in `documentation/` for detailed architecture information
