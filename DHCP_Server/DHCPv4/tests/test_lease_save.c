#include "../include/lease_v4.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

int main()
{
    printf("Testing lease database save with complete header...\n\n");

    // Create a simple database with a few leases
    struct lease_database_t db;
    lease_db_init(&db, "test_output.leases");

    // Add some test leases
    struct in_addr ip1, ip2;
    inet_pton(AF_INET, "192.168.1.100", &ip1);
    inet_pton(AF_INET, "10.0.0.50", &ip2);

    uint8_t mac1[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0xaa};
    uint8_t mac2[6] = {0xaa, 0xbb, 0xcc, 0x11, 0x22, 0x33};

    // Create lease 1
    struct dhcp_lease_t* lease1 = lease_db_add_lease(&db, ip1, mac1, 3600);
    if(lease1) {
        lease_set_client_id(lease1, (uint8_t[]){0x01, 0x00, 0x11, 0x22, 0x33, 0x44, 0xaa}, 7);
        lease_set_vendor_class(lease1, "MSFT 5.0");
        strncpy(lease1->client_hostname, "laptop-test", MAX_CLIENT_HOSTNAME - 1);
    }

    // Create lease 2
    struct dhcp_lease_t* lease2 = lease_db_add_lease(&db, ip2, mac2, 1800);
    if(lease2) {
        lease_set_client_id(lease2, (uint8_t[]){0x01, 0xaa, 0xbb, 0xcc, 0x11, 0x22, 0x33}, 7);
        lease_set_vendor_class(lease2, "dhcpcd-9.4.1:Linux-5.15.0");
        strncpy(lease2->client_hostname, "android-phone", MAX_CLIENT_HOSTNAME - 1);
    }

    printf("✓ Created %u test leases\n", db.lease_count);

    // Save database
    printf("Saving database to test_output.leases...\n");
    if(lease_db_save(&db) == 0) {
        printf("✓ Database saved successfully\n\n");
    } else {
        printf("✗ Failed to save database\n");
        return 1;
    }

    // Display first 40 lines of the saved file
    printf("=============================================================\n");
    printf("First 40 lines of saved file:\n");
    printf("=============================================================\n");

    FILE* fp = fopen("test_output.leases", "r");
    if(fp) {
        char line[1024];
        int line_count = 0;
        while(fgets(line, sizeof(line), fp) && line_count < 40) {
            printf("%s", line);
            line_count++;
        }
        fclose(fp);
    }

    printf("\n=============================================================\n");
    printf("Test complete! Check test_output.leases for full content.\n");
    printf("=============================================================\n");

    lease_db_free(&db);
    return 0;
}

// Compile: gcc -o test_lease_save test_lease_save.c lease_v4.c -Wall
// Run: ./test_lease_save
