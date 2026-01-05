#include "string_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

void dns_binary_to_text(unsigned char* read_pointer, unsigned char* buffer, int* count, char* output)
{
    int pos = 0; // pozitie curenta in sirul de iesire
    bool compression_pointer_jump = false;  // false - nu este prezent pointerul de compresie ; true - este prezent
    int offset; // valoare offset dat de pointer de compresie

    *count = 1;
    output[0] = '\0';

    while(*read_pointer != 0)
    {
        // verificare >= 0xC0 (adica 192 in baza 10), daca se incepe cu aceasta valoare avem pointer de compresie
        if(*read_pointer >= 192)
        {
            // 0x3F - masca pentru primii doi biti (0011 1111)
            offset = (*read_pointer & 0x3F) * 256 + *(read_pointer + 1);
            read_pointer = buffer + offset;

            if(compression_pointer_jump == false)
            {
                *count = *count + 1;
            }
            compression_pointer_jump = true;
        }else {
            unsigned char len = *read_pointer;
            
            for(int i = 0; i < len; i++)
            {
                output[pos++] = *(read_pointer + 1 + i);
            }

            output[pos++] = '.'; // punct separator
            read_pointer = read_pointer + len + 1;

            if(compression_pointer_jump == false)
            {
                *count = *count + len + 1;
            }
        }
    }

    if(pos > 0)
    {
        output[pos - 1] = '\0'; // eliminare ultim punct
    }else {
        strcpy(output, "."); // caz root
    }

}


// Conversie din domain name (www.mta.ro) in format binar DNS (label): www.mta.ro -> 3www3mta2ro0
void text_to_dns_binary(unsigned char* domain_name, unsigned char* host)
{
    int last_pos = 0;
    char temp_host[256];

    strncpy(temp_host, (char*)host, 255);
    strcat(temp_host, ".");

    for(int i = 0; i < strlen(temp_host); i++)
    {
        if (temp_host[i] == '.')
        {
            *domain_name++ = (unsigned char)(i - last_pos);

            while(last_pos < i)
            {
                *domain_name++ = temp_host[last_pos];
                last_pos++;
            }
            last_pos++;
        }
    }

    *domain_name++ = '\0';
}