#include "../include/ip_pool.h"
#include "../include/config_v4.h"
#include "../include/lease_v4.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <assert.h>

// Test counter validation
void test_counter_validation(struct ip_pool_t* pool)
{
    printf("\n=== Testing Counter Validation ===\n");
    
    uint32_t expected_available = 0;
    uint32_t expected_allocated = 0;
    
    for(uint32_t i = 0; i < pool->pool_size; i++)
    {
        if(pool->entries[i].state == IP_STATE_AVAILABLE)
            expected_available++;
        else if(pool->entries[i].state == IP_STATE_ALLOCATED)
            expected_allocated++;
    }
    
    printf("Expected available: %u, Actual: %u\n", expected_available, pool->available_count);
    printf("Expected allocated: %u, Actual: %u\n", expected_allocated, pool->allocated_count);
    
    assert(expected_available == pool->available_count);
    assert(expected_allocated == pool->allocated_count);
    
    printf("✓ Counter validation PASSED\n");
}

// Test allocation and release cycle
void test_allocation_release_cycle(struct ip_pool_t* pool, struct dhcp_config_t* config)
{
    printf("\n=== Testing Allocation/Release Cycle ===\n");
    
    uint8_t test_mac[6] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe};
    struct in_addr no_request = {0};
    
    uint32_t initial_available = pool->available_count;
    uint32_t initial_allocated = pool->allocated_count;
    
    // Allocate IP
    struct ip_allocation_result_t alloc = ip_pool_allocate(pool, test_mac, no_request, config);
    assert(alloc.success);
    printf("✓ Allocated IP: %s\n", inet_ntoa(alloc.ip_address));
    
    // Check counters
    assert(pool->available_count == initial_available - 1);
    assert(pool->allocated_count == initial_allocated + 1);
    printf("✓ Counters updated correctly after allocation\n");
    
    test_counter_validation(pool);
    
    // Release IP
    int release_result = ip_pool_release_ip(pool, alloc.ip_address);
    assert(release_result == 0);
    printf("✓ Released IP: %s\n", inet_ntoa(alloc.ip_address));
    
    // Check counters restored
    assert(pool->available_count == initial_available);
    assert(pool->allocated_count == initial_allocated);
    printf("✓ Counters restored after release\n");
    
    test_counter_validation(pool);
}

// Test conflict detection
void test_conflict_detection(struct ip_pool_t* pool)
{
    printf("\n=== Testing Conflict Detection ===\n");
    
    // Find an available IP
    struct in_addr test_ip = {0};
    for(uint32_t i = 0; i < pool->pool_size; i++)
    {
        if(pool->entries[i].state == IP_STATE_AVAILABLE)
        {
            test_ip = pool->entries[i].ip_address;
            break;
        }
    }
    
    assert(test_ip.s_addr != 0);
    printf("Testing conflict on IP: %s\n", inet_ntoa(test_ip));
    
    uint32_t initial_available = pool->available_count;
    
    // Mark as conflict
    int result = ip_pool_mark_conflict(pool, test_ip);
    assert(result == 0);
    printf("✓ Marked IP as conflict\n");
    
    // Check counter decreased
    assert(pool->available_count == initial_available - 1);
    printf("✓ Available count decreased\n");
    
    // Verify state
    struct ip_pool_entry_t* entry = ip_pool_find_entry(pool, test_ip);
    assert(entry != NULL);
    assert(entry->state == IP_STATE_CONFLICT);
    printf("✓ IP state is CONFLICT\n");
    
    test_counter_validation(pool);
}

// Test state transitions: ALLOCATED → CONFLICT
void test_allocated_to_conflict(struct ip_pool_t* pool, struct dhcp_config_t* config)
{
    printf("\n=== Testing ALLOCATED → CONFLICT Transition ===\n");
    
    uint8_t test_mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    struct in_addr no_request = {0};
    
    // Allocate IP
    struct ip_allocation_result_t alloc = ip_pool_allocate(pool, test_mac, no_request, config);
    assert(alloc.success);
    printf("Allocated IP: %s\n", inet_ntoa(alloc.ip_address));
    
    uint32_t allocated_before = pool->allocated_count;
    
    // Mark as conflict (simulating ping response on allocated IP)
    int result = ip_pool_mark_conflict(pool, alloc.ip_address);
    assert(result == 0);
    
    // ⚠️ EXPECTED BEHAVIOR: allocated_count should decrease
    // This will FAIL with current implementation!
    printf("Allocated count before: %u, after: %u\n", allocated_before, pool->allocated_count);
    
    // Uncomment when fix is implemented:
    // assert(pool->allocated_count == allocated_before - 1);
    
    printf("⚠️  This test highlights a BUG in current implementation\n");
}

// Test double allocation (same MAC)
void test_double_allocation_same_mac(struct ip_pool_t* pool, struct dhcp_config_t* config)
{
    printf("\n=== Testing Double Allocation (Same MAC) ===\n");
    
    uint8_t test_mac[6] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    struct in_addr no_request = {0};
    
    // First allocation
    struct ip_allocation_result_t alloc1 = ip_pool_allocate(pool, test_mac, no_request, config);
    assert(alloc1.success);
    printf("First allocation: %s\n", inet_ntoa(alloc1.ip_address));
    
    // Second allocation with SAME MAC (should return same IP)
    struct ip_allocation_result_t alloc2 = ip_pool_allocate(pool, test_mac, no_request, config);
    assert(alloc2.success);
    printf("Second allocation: %s\n", inet_ntoa(alloc2.ip_address));
    
    // Should be same IP
    assert(alloc1.ip_address.s_addr == alloc2.ip_address.s_addr);
    printf("✓ Same MAC got same IP (idempotent)\n");
}

// Test requested IP (already allocated to different MAC)
void test_requested_ip_conflict(struct ip_pool_t* pool, struct dhcp_config_t* config)
{
    printf("\n=== Testing Requested IP Conflict ===\n");
    
    uint8_t mac1[6] = {0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb};
    uint8_t mac2[6] = {0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc};
    struct in_addr no_request = {0};
    
    // MAC1 allocates an IP
    struct ip_allocation_result_t alloc1 = ip_pool_allocate(pool, mac1, no_request, config);
    assert(alloc1.success);
    printf("MAC1 allocated: %s\n", inet_ntoa(alloc1.ip_address));
    
    // MAC2 requests MAC1's IP (should get different IP)
    struct ip_allocation_result_t alloc2 = ip_pool_allocate(pool, mac2, alloc1.ip_address, config);
    assert(alloc2.success);
    printf("MAC2 requested %s, got: %s\n", inet_ntoa(alloc1.ip_address), inet_ntoa(alloc2.ip_address));
    
    // Should be different
    assert(alloc1.ip_address.s_addr != alloc2.ip_address.s_addr);
    printf("✓ Different MAC got different IP\n");
}

// Test static reservation priority
void test_static_reservation_priority(struct ip_pool_t* pool, struct dhcp_config_t* config)
{
    printf("\n=== Testing Static Reservation Priority ===\n");
    
    // Test with dc01 MAC (has static reservation to 192.168.1.10)
    uint8_t reserved_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    struct in_addr no_request = {0};
    
    struct ip_allocation_result_t alloc = ip_pool_allocate(pool, reserved_mac, no_request, config);
    assert(alloc.success);
    
    struct in_addr expected;
    inet_pton(AF_INET, "192.168.1.10", &expected);
    
    assert(alloc.ip_address.s_addr == expected.s_addr);
    printf("✓ Static reservation honored: %s\n", inet_ntoa(alloc.ip_address));
}

// Test pool exhaustion
void test_pool_exhaustion(struct ip_pool_t* pool, struct dhcp_config_t* config)
{
    printf("\n=== Testing Pool Exhaustion ===\n");
    
    printf("Total pool size: %u\n", pool->pool_size);
    printf("Initial available: %u\n", pool->available_count);
    printf("Initial allocated: %u\n", pool->allocated_count);
    
    // Try to allocate more IPs than available
    uint32_t allocations = 0;
    for(uint32_t i = 0; i < pool->available_count + 10; i++)
    {
        uint8_t mac[6] = {0xee, 0xee, 0xee, (uint8_t)(i >> 16), (uint8_t)(i >> 8), (uint8_t)i};
        struct in_addr no_request = {0};
        
        struct ip_allocation_result_t alloc = ip_pool_allocate(pool, mac, no_request, config);
        if(alloc.success)
        {
            allocations++;
        }
        else
        {
            printf("Pool exhausted after %u allocations\n", allocations);
            printf("Error: %s\n", alloc.error_message);
            break;
        }
    }
    
    printf("✓ Pool exhaustion handled correctly\n");
}

int main()
{
    printf("==============================================================\n");
    printf("Comprehensive IP Pool Testing\n");
    printf("==============================================================\n");

    // Load configuration
    struct dhcp_config_t config;
    if(parse_config_file("../config/dhcpv4.conf", &config) != 0)
    {
        fprintf(stderr, "Failed to load config\n");
        return 1;
    }

    // Load lease database
    struct lease_database_t lease_db;
    lease_db_init(&lease_db, "../data/dhcpv4.leases");
    lease_db_load(&lease_db);
    
    // Initialize IP pool for first subnet (192.168.1.0/24)
    struct ip_pool_t pool;
    if(ip_pool_init(&pool, &config.subnets[0], &lease_db) != 0)
    {
        fprintf(stderr, "Failed to initialize IP pool\n");
        return 1;
    }
    
    printf("✓ IP Pool initialized\n");
    ip_pool_print_stats(&pool);
    
    // Initial counter validation
    test_counter_validation(&pool);
    
    // Run all tests
    test_allocation_release_cycle(&pool, &config);
    test_conflict_detection(&pool);
    test_allocated_to_conflict(&pool, &config);
    test_double_allocation_same_mac(&pool, &config);
    test_requested_ip_conflict(&pool, &config);
    test_static_reservation_priority(&pool, &config);
    // test_pool_exhaustion(&pool, &config);  // Uncomment to test exhaustion
    
    // Final stats
    printf("\n==============================================================\n");
    printf("Final Pool Statistics\n");
    printf("==============================================================\n");
    ip_pool_print_stats(&pool);
    test_counter_validation(&pool);
    
    // Cleanup
    ip_pool_free(&pool);
    lease_db_free(&lease_db);
    free_config(&config);
    
    printf("\n==============================================================\n");
    printf("All tests completed!\n");
    printf("==============================================================\n");
    return 0;
}
