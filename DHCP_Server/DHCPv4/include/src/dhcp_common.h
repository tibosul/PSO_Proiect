#ifndef DHCP_COMMON_H
#define DHCP_COMMON_H

#include <netinet/in.h>
#include <stdint.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define DHCP_MAGIC_COOKIE 0x63825363

/* DHCP Message Types (Option 53) */
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_DECLINE 4
#define DHCP_ACK 5
#define DHCP_NAK 6
#define DHCP_RELEASE 7
#define DHCP_INFORM 8

/* DHCP Options */
#define DHCP_OPT_PAD 0
#define DHCP_OPT_SUBNET_MASK 1
#define DHCP_OPT_ROUTER 3
#define DHCP_OPT_DNS_SERVERS 6
#define DHCP_OPT_HOST_NAME 12
#define DHCP_OPT_DOMAIN_NAME 15
#define DHCP_OPT_BROADCAST_ADDR 28
#define DHCP_OPT_REQUESTED_IP 50
#define DHCP_OPT_LEASE_TIME 51
#define DHCP_OPT_MESSAGE_TYPE 53
#define DHCP_OPT_SERVER_ID 54
#define DHCP_OPT_PARAM_REQUEST_LIST 55
#define DHCP_OPT_RENEWAL_TIME 58
#define DHCP_OPT_REBINDING_TIME 59
#define DHCP_OPT_CLIENT_ID 61
#define DHCP_OPT_END 255

/* BootP op codes */
#define BOOTREQUEST 1
#define BOOTREPLY 2

/* Hardware types */
#define HTYPE_ETHER 1

struct dhcp_packet
{
    uint8_t op;            /* Message op code / message type. 1 = BOOTREQUEST, 2 = BOOTREPLY */
    uint8_t htype;         /* Hardware address type (e.g.,  1 = 10Mb Ethernet) */
    uint8_t hlen;          /* Hardware address length (e.g.  6 = 10Mb Ethernet) */
    uint8_t hops;          /* Client sets to zero, optionally used by relay agents */
    uint32_t xid;          /* Transaction ID, a random number chosen by the client */
    uint16_t secs;         /* Seconds elapsed since client began address acquisition or renewal process */
    uint16_t flags;        /* Flags */
    struct in_addr ciaddr; /* Client IP address; only filled in if client is in
                              BOUND, RENEW or REBINDING state */
    struct in_addr yiaddr; /* 'your' (client) IP address */
    struct in_addr siaddr; /* IP address of next server to use in bootstrap;
                              returned in DHCPOFFER, DHCPACK by server */
    struct in_addr giaddr; /* Relay agent IP address; used in booting via a relay agent */
    uint8_t chaddr[16];    /* Client hardware address */
    uint8_t sname[64];     /* Optional server host name, null terminated string */
    uint8_t file[128];     /* Boot file name, null terminated string; "generic" name
                              or null in DHCPDISCOVER, fully qualified directory-path
                              name in DHCPOFFER */
    uint8_t options[312];  /* Optional parameters field. reference to magic cookie
                              is first 4 bytes */
};

#endif // DHCP_COMMON_H
