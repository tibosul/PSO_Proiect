#ifndef DNS_PACKET_H
#define DNS_PACKET_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct __attribute__((packed)) {
    uint16_t identification; // folosit pentru identificarea perechilor cerere - raspuns
    // flags
    uint16_t QR: 1; // 0 = query (cerere) 1 = response -- serverul primeste pachete de query (bit 0) si trimite raspunsuri (bit 1)
    uint16_t Opcode: 4; // 0 = standard query, 1 = inverse query, 2 = server status (tip interogare)
    uint16_t AA: 1; // authoritative server (daca raspunsul provine de la un server autoritar pentru numele de domeniu sau de la un server intermediar)
    uint16_t TC: 1; // pachet trunchiat da/nu 
    uint16_t RD: 1; // recursion desired (daca clientul doreste raspuns doar de la serverul autoritarr sau doreste interogari suplimentare recursive)
    uint16_t RA: 1; // recursion available (daca serverul care raspunde ofera sau nu interogari recursive)
    uint16_t MBZ: 3; // must be zero (3 biti care obligatoriu trebuie sa fie 000)
    uint16_t Rcode: 4; // return code (0 - succes, 1 eroare de formatare, 2 eroare server, 3 numele de domeniu nu exista)
    // final flags
    uint16_t number_of_questions;
    uint16_t number_of_answers;
    uint16_t number_of_authoritative_answers;
    uint16_t number_of_additional_answers;
}dns_header; // 5 * 2 = 10 octeti (uint16_t) fara flag-uri + 8 biti + 8 biti = 12 octeti (dimensiunea unui header DNS)

typedef struct __attribute__((packed)){
   // uint16_t query_name; //un nume de domeniu sau adresa IP pentru care se face conversia
    uint16_t query_type; // un cod de doi octeti care specifica tipul interogarii
    uint16_t query_class; // un cod de octeti care specifica clasa interogarii
}dns_question_fixed;

typedef struct __attribute__((packed)){
   // uint16_t domain_name;
    uint16_t query_type;
    uint16_t query_class; 
    uint32_t TTL; // time to live
    uint16_t data_length; // lungimea zonei de date
   // uint16_t data; // adrese IP, nume de domeniu etc (depinde ce campul type)
}resource_record_fixed; // dns_answer

#endif 
