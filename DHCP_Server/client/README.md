# DHCP Client Directory

This directory contains both DHCPv4 and DHCPv6 client implementations. Both clients are located in the same folder to allow them to receive both IPv4 and IPv6 addresses from their respective DHCP servers.

## Structure

```
client/
├── bin/
│   ├── dhcpv4_client    # DHCPv4 client executable
│   └── dhcpv6_client    # DHCPv6 client executable
├── client_v4.c          # DHCPv4 client implementation
├── client_v4.h          # DHCPv4 client header
├── client_v6.c          # DHCPv6 client implementation
├── client_v6.h          # DHCPv6 client header
├── Makefile             # Build configuration for both clients
└── README.md            # This file
```

## Building

To build both clients:
```bash
make
```

To build only DHCPv4 client:
```bash
make v4
```

To build only DHCPv6 client:
```bash
make v6
```

To clean build artifacts:
```bash
make clean
```

## DHCPv4 Client

### Usage
```bash
sudo ./bin/dhcpv4_client <interface>
```

Example:
```bash
sudo ./bin/dhcpv4_client eth0
```

### Features
- DISCOVER, OFFER, REQUEST, ACK handshake
- IPv4 address assignment
- Automatic interface configuration using `ip` command
- Lease renewal (RENEWING state)
- Error handling and retry logic

### State Machine
1. **INIT**: Sends DHCPDISCOVER to find available DHCP servers
2. **SELECTING**: Waits for DHCPOFFER from server
3. **REQUESTING**: Sends DHCPREQUEST to confirm the offered IP
4. **BOUND**: Lease acquired, waits for renewal time (T1)
5. **RENEWING**: Sends REQUEST to renew the lease

## DHCPv6 Client

### Usage
```bash
sudo ./bin/dhcpv6_client <interface> [-P] [-S hexByte]
```

Options:
- `-P`: Request Prefix Delegation (IA_PD)
- `-S hexByte`: Spoof last byte of MAC address (e.g., `-S 0A`)

Example:
```bash
sudo ./bin/dhcpv6_client eth0
sudo ./bin/dhcpv6_client eth0 -P
```

### Features
- SOLICIT, ADVERTISE, REQUEST, REPLY handshake
- IPv6 address assignment (IA_NA)
- Prefix Delegation (IA_PD) - optional
- Automatic interface configuration using `ip` command
- Lease renewal (RENEWING state)
- DUID-LL (Link-Layer address) generation

### State Machine
1. **INIT**: Sends SOLICIT to find available DHCPv6 servers
2. **SOLICITING**: Waits for ADVERTISE from server
3. **REQUESTING**: Sends REQUEST to confirm the offered address
4. **BOUND**: Lease acquired, waits for renewal time (T1)
5. **RENEWING**: Sends RENEW to extend the lease

## Requirements

- Root privileges (for binding to privileged ports and configuring interfaces)
- Linux system with `ip` command available
- Network interface configured and up

## Notes

- Both clients require root/sudo access to:
  - Bind to privileged ports (68 for DHCPv4, 546 for DHCPv6)
  - Configure network interfaces with assigned addresses
- The clients automatically configure the network interface when a lease is obtained
- Lease renewal is handled automatically
- Timeouts and retries are implemented for robustness
