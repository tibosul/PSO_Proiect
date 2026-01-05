#include "string_utils.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_encoding(){
    printf("Testing domain name to dns binary labels conversion...\n");

    unsigned char encoded[256];
    char *input = "www.mta.ro";

    // rezultatul asteptat: [3]w w w [3] m t a [2] r o [0]
    unsigned char expected_result[] ={
        0x03, 'w', 'w', 'w',
        0x03, 'm', 't', 'a',
        0x02, 'r', 'o',
        0x00
    };

    text_to_dns_binary(encoded, (unsigned char*)input);

    if(memcmp(encoded, expected_result, sizeof(expected_result)) == 0)
    {
        printf("[SUCCESS] Function returned correct output for www.mta.ro!\n");
    }else{
        printf("[FAIL] Function did not return correct output for www.mta.ro!\n");
        
        printf("Expected result: ");
        for(int i = 0; i < sizeof(expected_result); i++)
        {
            printf("%02x ", expected_result[i]);
        }

        printf("\nFunction returned: ");
        for(int i = 0; i < sizeof(encoded); i++)
        {
            printf("%02x", encoded[i]);
        }
        printf("\n");
    }
}

void test_decoding_basic() {
    printf("\nTesting dns binary label to text domain name conversion...\n");
    
    unsigned char packet[] = {
        0x03, 'm', 't', 'a', 
        0x02, 'r', 'o', 
        0x00
    };
    
    char output[256];
    int count = 0;
    
    dns_binary_to_text(packet, packet, &count, output);
    
    // contorul trebuie sa dea 8 (0x03 - 1 octet, 'm' 't' 'a' - 3 octeti, 0x02 - 1 octet, 'r' 'o' - 2 octeti, 0x00 - 1 octet) 
    if (strcmp(output, "mta.ro") == 0 && count == 8) {
        printf("[SUCCESS] Decoding correct for mta.ro (bytes read: %d)\n", count);
    } else {
        printf("[FAIL] Decoding incorrect! Got: '%s', count: %d\n", output, count);
    }
}

void test_decoding_compression() {
    printf("\nTesting dns_binary_to_text (Compression Pointer)...\n");
    
    unsigned char mock_packet[64];
    memset(mock_packet, 0, 64);
    
    unsigned char name[] = {0x03, 'm', 't', 'a', 0x02, 'r', 'o', 0x00};
    memcpy(mock_packet, name, sizeof(name));
    
    //offset 15 (valoare arbitrara pentru testare)
    mock_packet[15] = 0xC0;
    mock_packet[16] = 0x00;
    
    char output[256];
    int count = 0;
    unsigned char *read_ptr = &mock_packet[15]; 

    dns_binary_to_text(read_ptr, mock_packet, &count, output);
    
    if (strcmp(output, "mta.ro") == 0 && count == 2) {
        printf("[SUCCESS] Compression handled correctly!\n");
        printf("Resolved name: %s, Bytes consumed from pointer: %d\n", output, count);
    } else {
        printf("[FAIL] Compression failed! Got: '%s', count: %d\n", output, count);
    }
}

int main() {
    printf("DNS STRING UTILS TEST: \n\n");
    
    test_encoding();
    test_decoding_basic();
    test_decoding_compression();
    
    printf("\nTests finished.\n");
    return 0;
}