#!/bin/bash
#
# DHCP Verification Script
# Comprehensive verification of DHCP server and client setup
#
# This script checks:
# - Build status of binaries
# - Configuration files
# - Network interfaces
# - Permissions
# - Runs basic functionality tests
#

cd "$(dirname "$0")/.."

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASS=0
FAIL=0
WARN=0

pass() {
    echo -e "  ${GREEN}✓${NC} $1"
    ((PASS++))
}

fail() {
    echo -e "  ${RED}✗${NC} $1"
    ((FAIL++))
}

warn() {
    echo -e "  ${YELLOW}!${NC} $1"
    ((WARN++))
}

info() {
    echo -e "  ${BLUE}→${NC} $1"
}

section() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

echo -e "${BLUE}"
echo "╔════════════════════════════════════════╗"
echo "║   DHCP Server Verification Script     ║"
echo "╚════════════════════════════════════════╝"
echo -e "${NC}"

# =============================================================================
# Check 1: Build System
# =============================================================================
section "1. Build System"

if [ -f "Makefile" ]; then
    pass "Makefile exists"
else
    fail "Makefile not found"
fi

if [ -d "build" ]; then
    pass "Build directory exists"
else
    warn "Build directory not found (run 'make all' to build)"
fi

# Check binaries
BINARIES=(
    "build/bin/dhcpv4_server"
    "build/bin/dhcpv4_client"
    "build/bin/dhcpv6_server"
    "build/bin/dhcpv6_client"
    "build/bin/dhcpv6_monitor"
)

for bin in "${BINARIES[@]}"; do
    if [ -f "$bin" ]; then
        pass "$(basename $bin) built"
    else
        fail "$(basename $bin) not found"
    fi
done

# =============================================================================
# Check 2: Configuration Files
# =============================================================================
section "2. Configuration Files"

if [ -f "DHCPv4/config/dhcpv4.conf" ]; then
    pass "DHCPv4 configuration exists"
    
    # Check for loopback subnet
    if grep -q "subnet 127.0.0.0" DHCPv4/config/dhcpv4.conf; then
        pass "Loopback subnet configured (127.0.0.0/8)"
    else
        warn "Loopback subnet not found in config"
    fi
    
    # Check for other subnets
    SUBNET_COUNT=$(grep -c "^subnet" DHCPv4/config/dhcpv4.conf || echo "0")
    info "Total subnets configured: $SUBNET_COUNT"
else
    fail "DHCPv4 configuration not found"
fi

if [ -f "DHCPv6/config/dhcpv6.conf" ]; then
    pass "DHCPv6 configuration exists"
else
    fail "DHCPv6 configuration not found"
fi

# =============================================================================
# Check 3: Scripts
# =============================================================================
section "3. Helper Scripts"

SCRIPTS=(
    "scripts/run_server_v4.sh"
    "scripts/run_client_v4.sh"
    "scripts/run_server_v6.sh"
    "scripts/run_client_v6.sh"
    "scripts/test_loopback.sh"
    "scripts/test_network.sh"
)

for script in "${SCRIPTS[@]}"; do
    if [ -f "$script" ]; then
        if [ -x "$script" ]; then
            pass "$(basename $script) exists and is executable"
        else
            warn "$(basename $script) exists but not executable"
        fi
    else
        fail "$(basename $script) not found"
    fi
done

# =============================================================================
# Check 4: Directories
# =============================================================================
section "4. Required Directories"

DIRS=(
    "DHCPv4/data"
    "DHCPv6/leases"
    "logs"
)

for dir in "${DIRS[@]}"; do
    if [ -d "$dir" ]; then
        pass "$dir exists"
    else
        warn "$dir does not exist (will be created when needed)"
        mkdir -p "$dir" 2>/dev/null && info "Created $dir"
    fi
done

# =============================================================================
# Check 5: Network Interfaces
# =============================================================================
section "5. Network Interfaces"

# Check for loopback
if ip link show lo &>/dev/null; then
    pass "Loopback interface (lo) available"
    # Check if loopback is UP (look for UP flag within angle brackets)
    if ip link show lo | grep -qE '<[^>]*\bUP\b[^>]*>'; then
        pass "Loopback interface is UP"
    else
        fail "Loopback interface is DOWN"
    fi
else
    fail "Loopback interface not found"
fi

# List other interfaces
info "Available network interfaces:"
ip -br link show | sed 's/^/    /'

# =============================================================================
# Check 6: Permissions and System Requirements
# =============================================================================
section "6. System Requirements"

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    pass "Running as root (required for DHCP)"
else
    warn "Not running as root (use 'sudo' to run server/client)"
fi

# Check for required commands
COMMANDS=(
    "ip"
    "gcc"
    "make"
    "tcpdump"
)

for cmd in "${COMMANDS[@]}"; do
    if command -v "$cmd" &>/dev/null; then
        pass "$cmd available"
    else
        if [ "$cmd" = "tcpdump" ]; then
            warn "$cmd not found (optional, for debugging)"
        else
            fail "$cmd not found"
        fi
    fi
done

# Check firewall (if applicable)
if command -v ufw &>/dev/null; then
    UFW_STATUS=$(sudo ufw status 2>/dev/null | head -1)
    info "UFW firewall: $UFW_STATUS"
    if echo "$UFW_STATUS" | grep -iq "active"; then
        warn "Firewall is active - ensure ports 67/68 UDP are allowed"
    fi
fi

# =============================================================================
# Check 7: Port Availability
# =============================================================================
# Function to check port usage
check_port_usage() {
    local port=$1
    local name=$2
    
    if ss -ulnp 2>/dev/null | grep -q ":${port} " || netstat -ulnp 2>/dev/null | grep -q ":${port} "; then
        warn "Port ${port} (${name}) is already in use"
        if [ "$port" = "67" ]; then
            info "You may need to stop existing DHCP servers"
        fi
    else
        pass "Port ${port} (${name}) is available"
    fi
}

section "7. Port Availability"

if command -v netstat &>/dev/null || command -v ss &>/dev/null; then
    check_port_usage 67 "DHCP server"
    check_port_usage 68 "DHCP client"
else
    warn "netstat/ss not available, cannot check port usage"
fi

# =============================================================================
# Check 8: Documentation
# =============================================================================
section "8. Documentation"

if [ -f "TESTING.md" ]; then
    pass "TESTING.md documentation exists"
else
    warn "TESTING.md not found"
fi

# =============================================================================
# Summary
# =============================================================================
section "Summary"

echo ""
echo -e "  ${GREEN}Passed:${NC}  $PASS"
echo -e "  ${YELLOW}Warnings:${NC} $WARN"
echo -e "  ${RED}Failed:${NC}  $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ System is ready for DHCP testing!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Read TESTING.md for detailed instructions"
    echo "  2. Run loopback test: sudo ./scripts/test_loopback.sh"
    echo "  3. Run network test: sudo ./scripts/test_network.sh"
    echo ""
    exit 0
else
    echo -e "${RED}✗ System has issues that need to be addressed${NC}"
    echo ""
    if [ ! -f "build/bin/dhcpv4_server" ]; then
        echo "Run 'make all' to build the binaries"
    fi
    echo ""
    exit 1
fi
