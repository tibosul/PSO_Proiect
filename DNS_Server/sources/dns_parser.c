#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "dns_parser.h"
#include "string_utils.h"
#include "error_codes.h"

int parse_dns_request(const unsigned char* buffer, size_t len, char* qname, uint16_t* qtype)
{
    if(len < sizeof(dns_header))
    {
        return ERR_INVALID_LENGTH;  //pachet prea scurt
    }

    unsigned char* reader = (unsigned char*)(buffer + sizeof(dns_header));

    if(reader >= buffer + len)
    {
        return ERR_PTR_OUT_OF_BUFFER_RANGE; // pointer in afara bufferului
    }

    int bytes_read = 0;

    dns_binary_to_text(reader, (unsigned char*) buffer, &bytes_read, qname);

    reader = reader + bytes_read;

    if((reader + sizeof(dns_question_fixed)) > (buffer + len))
    {
        return ERR_OUT_OF_BUFFER_SPACE; 
    }

    dns_question_fixed* question = (dns_question_fixed*)reader;

    if(qtype != NULL)
    {
        // The ntohs() function converts the unsigned short integer netshort from network byte order to host byte order. (linux.die.net/man/3/ntohs) -> Network byte order TO HoSt byte order (NTOHS)
        *qtype = ntohs(question->query_type);
    }

    return 0;
}