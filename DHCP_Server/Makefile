# DHCP Server Suite - Main Makefile
# Builds DHCPv4 server, DHCPv6 server, and clients

.PHONY: all clean rebuild v4 v6 client test-v4 test-v6 logs clean-logs help

# Default target - build everything
all: v4 v6 client
	@echo ""
	@echo "=== Build complete ==="
	@echo "Binaries:"
	@echo "  DHCPv4 Server: DHCPv4/build/bin/dhcpd"
	@echo "  DHCPv6 Server: DHCPv6/bin/dhcpv6_server"
	@echo "  DHCPv4 Client: client/bin/dhcpv4_client"
	@echo "  DHCPv6 Client: client/bin/dhcpv6_client"
	@echo ""
	@echo "Run 'make help' for available commands"

# Build DHCPv4 server
v4:
	@echo "=== Building DHCPv4 Server ==="
	$(MAKE) -C DHCPv4

# Build DHCPv6 server
v6:
	@echo "=== Building DHCPv6 Server ==="
	$(MAKE) -C DHCPv6

# Build clients
client:
	@echo "=== Building Clients ==="
	$(MAKE) -C client

# Clean all builds
clean:
	@echo "=== Cleaning all builds ==="
	$(MAKE) -C DHCPv4 clean
	$(MAKE) -C DHCPv6 clean
	$(MAKE) -C client clean
	@echo "Clean complete"

# Clean and rebuild
rebuild: clean all

# Clean logs and leases
clean-logs:
	@echo "=== Cleaning logs and leases ==="
	rm -f DHCPv4/dhcpv4.log DHCPv6/dhcpv6.log
	rm -f DHCPv4/data/dhcpd.leases DHCPv6/leases/dhcpd6.leases
	@echo "Logs and leases cleaned"

# View logs
logs:
	@./scripts/view_logs.sh

# Run DHCPv4 server (needs sudo for port 67)
run-v4:
	@mkdir -p DHCPv4/data
	cd DHCPv4 && ./build/bin/dhcpd

# Run DHCPv4 server with sudo
run-v4-sudo:
	@mkdir -p DHCPv4/data
	cd DHCPv4 && sudo ./build/bin/dhcpd

# Run DHCPv6 server (needs sudo for port 547)
run-v6:
	@mkdir -p DHCPv6/leases
	cd DHCPv6 && sudo ./bin/dhcpv6_server

# Run DHCPv4 client (usage: make client-v4 IF=eth0)
IF ?= lo
client-v4:
	sudo ./client/bin/dhcpv4_client $(IF)

# Run DHCPv6 client (usage: make client-v6 IF=eth0)
client-v6:
	sudo ./client/bin/dhcpv6_client $(IF)

# Run DHCPv6 client with prefix delegation
client-v6-pd:
	sudo ./client/bin/dhcpv6_client $(IF) -P

# Show available interfaces
interfaces:
	@echo "=== Available Network Interfaces ==="
	@ip -br link show

# Help
help:
	@echo "DHCP Server Suite - Available targets:"
	@echo ""
	@echo "  Build:"
	@echo "    make          - Build everything (v4 + v6 + clients)"
	@echo "    make v4       - Build DHCPv4 server only"
	@echo "    make v6       - Build DHCPv6 server only"
	@echo "    make client   - Build clients only"
	@echo "    make rebuild  - Clean and rebuild all"
	@echo "    make clean    - Remove all build files"
	@echo ""
	@echo "  Run servers:"
	@echo "    make run-v4      - Run DHCPv4 server (port 6767 fallback)"
	@echo "    make run-v4-sudo - Run DHCPv4 server with sudo (port 67)"
	@echo "    make run-v6      - Run DHCPv6 server with sudo (port 547)"
	@echo ""
	@echo "  Run clients:"
	@echo "    make client-v4 IF=eth0    - Run DHCPv4 client on interface"
	@echo "    make client-v6 IF=eth0    - Run DHCPv6 client on interface"
	@echo "    make client-v6-pd IF=eth0 - Run DHCPv6 client with prefix delegation"
	@echo ""
	@echo "  Utilities:"
	@echo "    make logs        - View server logs"
	@echo "    make clean-logs  - Remove logs and lease files"
	@echo "    make interfaces  - Show available network interfaces"
	@echo ""
	@echo "  Log files:"
	@echo "    DHCPv4: DHCPv4/dhcpv4.log"
	@echo "    DHCPv6: DHCPv6/dhcpv6.log"
