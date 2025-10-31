#include "lease_v4.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

int main()
{
    printf("==============================================================\n");
    printf("DHCPv4 Extended Lease Database Test\n");
    printf("==============================================================\n\n");

    // Initialize lease database
    struct lease_database_t db;
    if(lease_db_init(&db, "dhcpv4.leases") != 0) {
        fprintf(stderr, "Failed to initialize lease database\n");
        return 1;
    }

    // Load leases from file
    printf("Loading leases from dhcpv4.leases...\n");
    if(lease_db_load(&db) != 0) {
        fprintf(stderr, "Failed to load lease database\n");
        return 1;
    }

    printf("\n✓ Successfully loaded %u leases\n\n", db.lease_count);

    // Print detailed lease information
    printf("==============================================================\n");
    printf("Detailed Lease Information\n");
    printf("==============================================================\n\n");

    for(uint32_t i = 0; i < db.lease_count && i < 5; i++)  // Show first 5 leases
    {
        const struct dhcp_lease_t *lease = &db.leases[i];
        char ip_str[INET_ADDRSTRLEN];
        char time_buf[64];

        inet_ntop(AF_INET, &lease->ip_address, ip_str, INET_ADDRSTRLEN);

        printf("Lease #%u: %s\n", i+1, ip_str);
        printf("  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               lease->mac_address[0], lease->mac_address[1], lease->mac_address[2],
               lease->mac_address[3], lease->mac_address[4], lease->mac_address[5]);

        printf("  State: %s → next: %s, rewind: %s\n",
               lease_state_to_string(lease->state),
               lease_state_to_string(lease->next_binding_state),
               lease_state_to_string(lease->rewind_binding_state));

        format_lease_time(lease->start_time, time_buf, sizeof(time_buf));
        printf("  Starts: %s\n", time_buf);

        format_lease_time(lease->end_time, time_buf, sizeof(time_buf));
        printf("  Ends:   %s\n", time_buf);

        if(lease->tstp > 0) {
            format_lease_time(lease->tstp, time_buf, sizeof(time_buf));
            printf("  Tstp:   %s\n", time_buf);
        }

        if(lease->cltt > 0) {
            format_lease_time(lease->cltt, time_buf, sizeof(time_buf));
            printf("  Cltt:   %s\n", time_buf);
        }

        if(lease->client_id_len > 0) {
            printf("  Client ID (%u bytes): ", lease->client_id_len);
            for(uint32_t j = 0; j < lease->client_id_len; j++) {
                printf("%02x", lease->client_id[j]);
            }
            printf("\n");
        }

        if(strlen(lease->client_hostname) > 0) {
            printf("  Hostname: %s\n", lease->client_hostname);
        }

        if(strlen(lease->vendor_class_identifier) > 0) {
            printf("  Vendor: %s\n", lease->vendor_class_identifier);
        }

        if(lease->is_abandoned) {
            printf("  ⚠ ABANDONED\n");
        }

        if(lease_is_expired(lease)) {
            printf("  ⚠ EXPIRED\n");
        }

        printf("\n");
    }

    // Test adding a new lease
    printf("==============================================================\n");
    printf("Testing New Lease Creation\n");
    printf("==============================================================\n\n");

    struct in_addr new_ip;
    inet_pton(AF_INET, "192.168.1.250", &new_ip);
    uint8_t new_mac[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    struct dhcp_lease_t* new_lease = lease_db_add_lease(&db, new_ip, new_mac, 3600);
    if(new_lease) {
        // Add some extended information
        lease_set_client_id(new_lease, (uint8_t[]){0x01, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}, 7);
        lease_set_vendor_class(new_lease, "Test Client v1.0");
        strncpy(new_lease->client_hostname, "test-machine", MAX_CLIENT_HOSTNAME - 1);

        // Set state transitions
        lease_set_state_transition(new_lease, LEASE_STATE_ACTIVE,
                                   LEASE_STATE_FREE, LEASE_STATE_FREE);

        printf("✓ Created new lease for 192.168.1.250\n");
        printf("  MAC: aa:bb:cc:dd:ee:ff\n");
        printf("  Duration: 3600 seconds (1 hour)\n");
        printf("  Hostname: test-machine\n");
        printf("  Vendor: Test Client v1.0\n");
        printf("  Client ID: 01aabbccddeeff\n");
    }

    // Test time parsing
    printf("\n==============================================================\n");
    printf("Testing Time Format Conversion\n");
    printf("==============================================================\n\n");

    const char* test_time = "4 2024/10/26 14:30:00";
    time_t parsed_time = parse_lease_time(test_time);
    char formatted[64];
    format_lease_time(parsed_time, formatted, sizeof(formatted));

    printf("Original:  %s\n", test_time);
    printf("Parsed:    %ld (Unix timestamp)\n", parsed_time);
    printf("Formatted: %s\n", formatted);
    printf("✓ Round-trip %s\n\n",
           strcmp(test_time, formatted) == 0 ? "PASSED" : "WARNING: Format differs");

    // Test client ID parsing
    printf("==============================================================\n");
    printf("Testing Client ID Parsing\n");
    printf("==============================================================\n\n");

    const char* test_uid = "\"\\001\\000\\021\\042\\063\\104\\125\\252\"";
    uint8_t parsed_id[64];
    uint32_t parsed_len;

    parse_client_id_from_string(test_uid, parsed_id, &parsed_len);

    printf("Original: %s\n", test_uid);
    printf("Parsed (%u bytes): ", parsed_len);
    for(uint32_t i = 0; i < parsed_len; i++) {
        printf("%02x", parsed_id[i]);
    }
    printf("\n");

    char formatted_id[256];
    format_client_id_to_string(parsed_id, parsed_len, formatted_id, sizeof(formatted_id));
    printf("Formatted: %s\n", formatted_id);

    // Summary
    printf("\n==============================================================\n");
    printf("Summary\n");
    printf("==============================================================\n");
    printf("Total leases in database: %u\n", db.lease_count);

    uint32_t count_by_state[7] = {0};
    for(uint32_t i = 0; i < db.lease_count; i++) {
        count_by_state[db.leases[i].state]++;
    }

    printf("  FREE: %u\n", count_by_state[LEASE_STATE_FREE]);
    printf("  ACTIVE: %u\n", count_by_state[LEASE_STATE_ACTIVE]);
    printf("  EXPIRED: %u\n", count_by_state[LEASE_STATE_EXPIRED]);
    printf("  RELEASED: %u\n", count_by_state[LEASE_STATE_RELEASED]);
    printf("  ABANDONED: %u\n", count_by_state[LEASE_STATE_ABANDONED]);
    printf("  RESERVED: %u\n", count_by_state[LEASE_STATE_RESERVED]);
    printf("  BACKUP: %u\n", count_by_state[LEASE_STATE_BACKUP]);

    printf("\n==============================================================\n");
    printf("Test Complete!\n");
    printf("==============================================================\n");

    lease_db_free(&db);
    return 0;
}

// Compile: gcc -o test_lease_extended test_lease_extended.c lease_v4.c -Wall
// Run: ./test_lease_extended
