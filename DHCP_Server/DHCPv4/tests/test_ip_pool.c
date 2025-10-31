#include "../include/ip_pool.h"
#include "../include/config_v4.h"
#include "../include/lease_v4.h"
#include <stdio.h>
#include <arpa/inet.h>

int main() {
    printf("--- Testing IP Pool Management ---\n\n");
    
    // Load configuration
    struct dhcp_config_t config;
    if(parse_config_file("dhcpv4.conf", &config) != 0)
    {
        fprintf(stderr, "Failed to load config\n");
        return 1;
    }
    
    // Load lease database
    struct lease_database_t lease_db;
    lease_db_init(&lease_db, "dhcpd.leases");
    lease_db_load(&lease_db);
    
    // Initialize IP pool for first subnet
    struct ip_pool_t pool;
    if(ip_pool_init(&pool, &config.subnets[0], &lease_db) != 0)
    {
        fprintf(stderr, "Failed to initialize IP pool\n");
        return 1;
    }
    
    printf("✓ IP Pool initialized\n");
    ip_pool_print_stats(&pool);
    
    // Test allocation
    printf("\n=== Testing IP Allocation ===\n");
    
    uint8_t mac1[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01};
    struct in_addr requested;
    requested.s_addr = 0; // No specific request
    
    struct ip_allocation_result_t alloc1 = ip_pool_allocate(&pool, mac1, requested, &config);
    if(alloc1.success)
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &alloc1.ip_address, ip_str, INET_ADDRSTRLEN);
        printf("✓ Allocated IP: %s to MAC aa:bb:cc:dd:ee:01\n", ip_str);
    }
    else
    {
        printf("✗ Allocation failed: %s\n", alloc1.error_message);
    }
    
    // Test static reservation (should get 192.168.1.10)
    printf("\n=== Testing Static Reservation ===\n");
    uint8_t mac_reserved[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    
    struct ip_allocation_result_t alloc2 = ip_pool_allocate(&pool, mac_reserved, requested, &config);
    if(alloc2.success)
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &alloc2.ip_address, ip_str, INET_ADDRSTRLEN);
        printf("✓ Static reservation: %s\n", ip_str);
    }
    
    // Test requested IP
    printf("\n=== Testing Requested IP ===\n");
    uint8_t mac2[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02};
    inet_pton(AF_INET, "192.168.1.150", &requested);
    
    struct ip_allocation_result_t alloc3 = ip_pool_allocate(&pool, mac2, requested, &config);
    if(alloc3.success)
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &alloc3.ip_address, ip_str, INET_ADDRSTRLEN);
        printf("✓ Honored request: %s\n", ip_str);
    }
    
    // Print detailed pool status
    printf("\n");
    ip_pool_print_detailed(&pool);
    
    // Cleanup
    ip_pool_free(&pool);
    lease_db_free(&lease_db);
    free_config(&config);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}

// Compile: gcc -o test_ip_pool test_ip_pool.c ip_pool.c config_v4.c lease_db.c
// Run: ./test_ip_pool