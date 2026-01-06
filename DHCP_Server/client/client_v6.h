#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include <net/if.h>

#define CLIENT_PORT 546
#define SERVER_PORT 547
#define BUF_SIZE 4096

typedef enum {
    STATE_INIT,
    STATE_SOLICITING,
    STATE_REQUESTING,
    STATE_BOUND,
    STATE_RENEWING
} client_state_t;

int get_mac_address(const char* ifname, uint8_t* mac);
int get_if_index(const char* ifname);
void generate_duid(uint8_t* mac, uint8_t* duid, uint16_t* len);

#endif // CLIENT_H
