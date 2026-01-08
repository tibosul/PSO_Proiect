# DHCP Server Suite

A complete DHCP server implementation supporting both **DHCPv4** (IPv4) and **DHCPv6** (IPv6) protocols, written in C11.

## Features

### DHCPv4 Server
- ✅ Full DHCP protocol: DISCOVER, OFFER, REQUEST, ACK, NAK, RELEASE
- ✅ Multi-subnet support (up to MAX_SUBNETS)
- ✅ Static host reservations (MAC-based)
- ✅ Lease persistence to disk
- ✅ DHCP options: subnet mask, router, DNS, hostname, domain name, lease times
- ✅ Thread pool (4 workers) for concurrent processing
- ✅ Ping check for IP conflict detection (optional)
- ✅ Relay agent support (giaddr)
- ✅ ISC-like configuration format

### DHCPv6 Server
- ✅ Full DHCPv6 protocol: SOLICIT, ADVERTISE, REQUEST, REPLY, RENEW, REBIND, RELEASE
- ✅ Identity Association for Non-temporary Addresses (IA_NA)
- ✅ Identity Association for Prefix Delegation (IA_PD)
- ✅ ICMPv6 ping check for conflict detection
- ✅ DUID-based client identification
- ✅ Thread pool (8 workers) for concurrent processing
- ✅ Shared memory statistics for real-time monitoring
- ✅ Cleanup thread for expired leases
- ✅ ISC-like configuration format

### Additional Components
- **Logging System**: Centralized logging with levels (DEBUG, INFO, WARN, ERROR)
- **DHCPv4 Client**: Test client for IPv4
- **DHCPv6 Client**: Test client for IPv6
- **DHCPv6 Monitor**: Real-time dashboard for server statistics

## Build Instructions

### Prerequisites
- GCC compiler with C11 support
- Linux/Unix system
- pthread library
- Standard C libraries

### Building

```bash
# Build everything
make all

# Build only DHCPv4 server and client
make v4

# Build only DHCPv6 server, client, and monitor
make v6

# Build only servers
make servers

# Build only clients
make clients

# Clean build artifacts
make clean
```

### Build Output

Binaries are placed in `build/bin/`:
- `dhcpv4_server` - DHCPv4 server
- `dhcpv4_client` - DHCPv4 test client
- `dhcpv6_server` - DHCPv6 server
- `dhcpv6_client` - DHCPv6 test client
- `dhcpv6_monitor` - DHCPv6 statistics monitor

## Configuration

### DHCPv4

Configuration file: `DHCPv4/config/dhcpv4.conf`

Example configuration includes:
- 7 pre-configured subnets (loopback, corporate, guest, IoT, VoIP, dev, DMZ)
- 23 static host reservations
- Global options (DNS servers, lease times, ping check)
- Per-subnet overrides

See the config file for extensive documentation and examples.

### DHCPv6

Configuration file: `DHCPv6/config/dhcpv6.conf`

Example configuration includes:
- 15 pre-configured IPv6 subnets
- 2 prefix delegation pools (/56 and /60)
- Static host reservations based on DUID
- Global and per-subnet options

## Running

### DHCPv4 Server

```bash
# Requires root privileges for port 67, or will fallback to port 6767
cd DHCPv4
sudo ../build/bin/dhcpv4_server

# Or with custom config
sudo ../build/bin/dhcpv4_server /path/to/config.conf
```

### DHCPv6 Server

```bash
# Requires root privileges for port 547
cd DHCPv6
sudo ../build/bin/dhcpv6_server

# Or with custom config
sudo ../build/bin/dhcpv6_server /path/to/config.conf
```

### DHCPv6 Monitor

```bash
# Monitor server statistics in real-time
../build/bin/dhcpv6_monitor
```

### Testing with Clients

```bash
# Test DHCPv4
../build/bin/dhcpv4_client

# Test DHCPv6
../build/bin/dhcpv6_client
```

## Directory Structure

```
DHCP_Server/
├── DHCPv4/
│   ├── config/         # Configuration files
│   ├── data/           # Lease database (created at runtime)
│   ├── include/        # Header files
│   │   ├── src/        # Core headers
│   │   └── utils/      # Utility headers
│   ├── src/            # Main source code
│   │   ├── main.c      # Server entry point
│   │   ├── config_v4.c # Config parser
│   │   ├── dhcp_message.c # Protocol handling
│   │   ├── ip_pool.c   # IP allocation
│   │   └── lease_v4.c  # Lease management
│   └── utils/          # Utility functions
│       ├── thread_pool.c
│       ├── network_utils.c
│       ├── file_utils.c
│       └── ...
├── DHCPv6/
│   ├── config/         # Configuration files
│   ├── include/        # Header files
│   ├── leases/         # Lease database (created at runtime)
│   ├── monitor/        # Monitoring tool
│   └── sources/        # Source code
│       ├── server.c    # Main server logic
│       ├── standalone.c # Entry point
│       ├── protocol_v6.c # Protocol handling
│       ├── ip6_pool.c  # IPv6 allocation
│       ├── pd_pool.c   # Prefix delegation
│       ├── leases6.c   # Lease management
│       └── config_v6.c # Config parser
├── client/             # Test clients
│   ├── client_v4.c
│   └── client_v6.c
├── logger/             # Logging system
│   ├── logger.c
│   └── logger.h
├── Makefile            # Build system
├── README.md           # This file
└── REVIZIE_DHCP.md     # Comprehensive review (Romanian)
```

## Logs

Logs are written to:
- `logs/dhcpv4_server.log` - DHCPv4 server logs
- `logs/dhcpv6_server.log` - DHCPv6 server logs

View recent logs:
```bash
make logs
```

## Known Limitations

- **Security**: No rate limiting or DOS protection
- **DHCPv4**: INFORM and DECLINE messages not implemented
- **DHCPv6**: RECONFIGURE and temporary addresses (IA_TA) not implemented
- **Relay**: Incomplete relay agent support
- **High Availability**: No failover mechanism
- **Testing**: No unit tests (manual testing only)

See `REVIZIE_DHCP.md` for a comprehensive analysis.

## Performance

Estimated throughput (with conflict detection disabled):
- DHCPv4: ~1,000 requests/second
- DHCPv6: ~1,500 requests/second

Actual performance depends on:
- Hardware specifications
- Network latency
- Ping check configuration
- Disk I/O (lease database writes)

## Thread Safety

Both servers are thread-safe:
- Mutex protection for shared resources (lease database, IP pools)
- Thread pool pattern for concurrent request processing
- No global mutable state without synchronization

## License

This is an educational project. See repository license for details.

## Contributing

For improvements or bug fixes, please:
1. Review the code and `REVIZIE_DHCP.md` for architectural overview
2. Ensure thread safety in any changes
3. Follow existing code style
4. Test with both test clients
5. Update documentation

## Support

For issues or questions, please refer to the comprehensive review document (`REVIZIE_DHCP.md`) which includes:
- Detailed architecture analysis
- Security considerations
- Performance benchmarks
- Identified issues and recommendations
- Comparison between v4 and v6 implementations

## Acknowledgments

Built following RFC standards:
- RFC 2131, 2132 (DHCPv4)
- RFC 8415 (DHCPv6)
- RFC 3633 (Prefix Delegation)
- RFC 3646 (DNS options for DHCPv6)
