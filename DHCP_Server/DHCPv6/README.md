# DHCPv6 Server in C (POSIX / Linux)

A fully implemented, modular and robust DHCPv6 server written in C, designed for POSIX-compliant systems (Linux).
The project includes a production-ready server capable of IPv6 address allocation (IA_NA) and Prefix Delegation (IA_PD), a test client, and a real-time monitoring utility implemented using POSIX Shared Memory.

## Features
    DHCPv6 Multithreaded Server

The server uses a thread pool to efficiently process incoming DHCPv6 packets. The architecture is modular, with the DHCP logic isolated in the dhcpv6_agent component, allowing easy reuse in other applications. The server supports both IPv6 address allocation (IA_NA) and Prefix Delegation (IA_PD). Lease information is stored persistently on disk, and expired leases are automatically cleaned using a dedicated garbage collector thread. Graceful shutdown is implemented using POSIX signal handling.

    Real-Time Monitoring

The server exposes live runtime statistics using POSIX shared memory through shm_open and mmap. A separate monitoring utility, dhcpv6_monitor, can attach to this shared memory segment and display server uptime, processed packet count, and active leases in real time. This monitoring mechanism is zero-copy and does not impact server performance.

    DHCPv6 Test Client

The test client implements a complete DHCPv6 state machine, following the standard workflow: SOLICIT, ADVERTISE, REQUEST, REPLY, and RENEW. It simulates lease timers and automatic renewal behavior and is capable of configuring the network interface automatically using system commands such as ip addr add. The client can request either an IPv6 address or both an address and a delegated prefix.

    Project Structure

The project is organized into clearly separated components:

    sources contains the server implementation, including core logic, thread pool management, and configuration parsing.
    include contains all public header files.
    client contains the DHCPv6 client implementation.
    monitor contains the real-time monitoring utility.
    config stores configuration files such as dhcpv6.conf.
    leases stores the persistent DHCPv6 lease database.
The Makefile defines the build system and targets.

Build Instructions

The project is built using make. From the root directory, run the following command:

make all

This command generates the server, client, and monitoring executables in the bin directory.

    Running the Server

Root privileges are required in order to bind to UDP port 547. The server can be started using:

sudo ./bin/dhcpv6_server

    Running the Client

The client must be provided with the name of the network interface used to send DHCPv6 requests, such as eth0 or enp0s3.

sudo ./bin/dhcpv6_client <interface_name>

Example:

sudo ./bin/dhcpv6_client enp0s3

    Monitoring the Server

To view live server statistics, run the monitoring utility in a separate terminal:

./bin/dhcpv6_monitor

    Configuration

The server configuration is defined in the file config/dhcpv6.conf. This file specifies the IPv6 subnets, address ranges, and delegated prefixes available for clients.

Example configuration:

subnet 2001:db8:1::/64 {
range 2001:db8:1::1000 2001:db8:1::2000
prefix-delegation 2001:db8:1:100::/56
}

    Client Options
The DHCPv6 client supports additional options:
sudo ./bin/dhcpv6_client <interface_name> [-P]
Using the -P option enables Prefix Delegation (IA_PD). If this option is omitted, the client requests only an IPv6 address (IA_NA).

    Multi-Client and Stress Testing
Client Spoofing on a Single Machine
The client supports spoofing the last byte of its MAC address using the -S option. This allows multiple distinct clients to be simulated on the same physical interface.
sudo ./bin/dhcpv6_client enp0s3 -P -S 01
sudo ./bin/dhcpv6_client enp0s3 -P -S 02

Stress Test Script
A stress testing script named test_stress.sh is included to rapidly launch multiple client instances.
./test_stress.sh <interface> <count>
For example:

./test_stress.sh enp0s3 5

Each client writes its output to a separate client_X.log file.

Testing on a Second Device
To test the client on another Linux machine within the same network, copy the dhcpv6_client executable or the entire project to the second device. Ensure that the firewall allows UDP ports 546 and 547. Run the client with root privileges and specify the appropriate network interface. The server logs will display incoming requests from the new MAC address.

POSIX Concepts Demonstrated
This project demonstrates advanced usage of POSIX system programming concepts, including multithreading with pthreads, synchronization using mutexes and condition variables, IPv6 networking with UDP sockets and multicast support, inter-process communication using POSIX shared memory, signal handling for clean shutdown, and atomic file I/O for lease persistence.

Verification
The project was successfully compiled using make. The server correctly binds to UDP port 547 and initializes all worker threads. Graceful shutdown was verified using sigaction and poll. End-to-end client-server communication was tested and confirmed, including IPv6 address allocation, prefix delegation, automatic lease renewal, and correct network configuration verified using the ip a command.