#ifndef CLIENT_V4_H
#define CLIENT_V4_H

#include <stdint.h>
#include <net/if.h>
#include <netinet/in.h>

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define BUF_SIZE_V4 1024

typedef enum {
    STATE_V4_INIT,
    STATE_V4_SELECTING,
    STATE_V4_REQUESTING,
    STATE_V4_BOUND,
    STATE_V4_RENEWING,
    STATE_V4_REBINDING
} client_state_v4_t;

int get_mac_address_v4(const char* ifname, uint8_t* mac);
int get_if_index_v4(const char* ifname);

#endif // CLIENT_V4_H
