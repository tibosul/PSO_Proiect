/*
 * DHCPv6 Client Implementation
 * 
 * Features:
 * - Solicit, Advertise, Request, Reply handshake.
 * - IPv6 Address Assignment (IA_NA).
 * - Prefix Delegation (IA_PD) [Optional via -P].
 * - System integration via 'ip' command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <time.h>

#include "../DHCPv6/include/protocol_v6.h"
#include "../DHCPv6/include/utilsv6.h"

#include "client_v6.h"

/**
 * @brief Retrieves the MAC address of the specified network interface.
 * 
 * @param ifname Name of the interface (e.g., "eth0").
 * @param mac Buffer to store the 6-byte MAC address.
 * @return int 0 on success, -1 on failure.
 */
int get_mac_address(const char* ifname, uint8_t* mac) {
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    
    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
    
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        close(fd);
        return -1;
    }
    close(fd);
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    return 0;
}

/**
 * @brief Wrapper for if_nametoindex.
 */
int get_if_index(const char* ifname) {
    return if_nametoindex(ifname);
}

/**
 * @brief Generates a DUID-LL (Link-Layer address) based on MAC.
 * Format: [Type=3][HW=1][MAC]
 */
void generate_duid(uint8_t* mac, uint8_t* duid, uint16_t* len) {
    // Type 3 (DUID-LL), HW Type 1 (Ethernet)
    duid[0] = 0; duid[1] = 3; 
    duid[2] = 0; duid[3] = 1;
    memcpy(duid+4, mac, 6);
    *len = 10;
}

int main(int argc, char** argv) {
    // --- Argument Parsing ---
    char* ifname = NULL;
    int request_pd = 0;
    int spoof_byte = -1;
    
    for(int i=1; i<argc; i++) {
        if(strcmp(argv[i], "-P") == 0) {
            request_pd = 1;
        } else if (strcmp(argv[i], "-S") == 0) {
            if (i+1 < argc) {
                spoof_byte = (int)strtol(argv[i+1], NULL, 16);
                i++;
            }
        } else if (ifname == NULL) {
            ifname = argv[i];
        }
    }
    
    if (ifname == NULL) {
        fprintf(stderr, "Usage: %s <interface> [-P] [-S hexByte]\n", argv[0]);
        fprintf(stderr, "  -P : Request Prefix Delegation\n");
        fprintf(stderr, "  -S : Spoof last byte of MAC (e.g. -S 0A)\n");
        return 1;
    }
    
    // --- Interface Setup ---
    int ifindex = get_if_index(ifname);
    if (ifindex == 0) {
        perror("Error getting interface index");
        return 1;
    }
    
    uint8_t mac[6];
    if (get_mac_address(ifname, mac) != 0) {
        perror("Error getting MAC address");
        return 1;
    }

    if (spoof_byte >= 0) {
        mac[5] = (uint8_t)spoof_byte;
        printf("DEBUG: Spoofing MAC last byte to %02X\n", mac[5]);
    }
    
    uint8_t client_duid[20];
    uint16_t client_duid_len;
    generate_duid(mac, client_duid, &client_duid_len);
    
    printf("Starting DHCPv6 Client on %s (Index %d)\n", ifname, ifindex);
    if (request_pd) printf("Option: Prefix Delegation Enabled (-P)\n");
    printf("DUID: ");
    for(int i=0; i<client_duid_len; i++) printf("%02x", client_duid[i]);
    printf("\n");
    
    // --- Socket Setup ---
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(CLIENT_PORT);
    addr.sin6_addr = in6addr_any; 
    addr.sin6_scope_id = ifindex;
    
    int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) perror("setsockopt REUSEADDR");
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) perror("setsockopt REUSEPORT");
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(sock);
        return 1;
    }
    
    // Destination: All_DHCP_Relay_Agents_and_Servers (ff02::1:2)
    struct sockaddr_in6 dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin6_family = AF_INET6;
    dest_addr.sin6_port = htons(SERVER_PORT);
    inet_pton(AF_INET6, "ff02::1:2", &dest_addr.sin6_addr);
    dest_addr.sin6_scope_id = ifindex;
    
    // --- State Machine ---
    client_state_t state = STATE_INIT;
    uint32_t xid = 0x123456; // Simple static XID for demo purposes
    uint32_t iaid = 1;
    
    uint8_t buf[BUF_SIZE];
    uint8_t rx_buf[BUF_SIZE];
    
    // Server DUID Persistence
    uint8_t server_duid[128];
    uint16_t server_duid_len = 0;
    
    // =========================================================================
    // DHCPv6 Client State Machine
    // This loop handles the conversation: SOLICIT -> ADVERTISE -> REQUEST -> REPLY
    // =========================================================================
    while(1) {
        
        // ---------------------------------------------------------------------
        // STATE: INIT
        // Action: Yell "Is there any DHCPv6 server out there?" (SOLICIT)
        // ---------------------------------------------------------------------
        if (state == STATE_INIT) {
            printf("[INIT] Sending SOLICIT...\n");
            
            // Init Header
            dhcpv6_header_t* hdr = (dhcpv6_header_t*)buf;
            hdr->msg_type = MSG_SOLICIT;
            dhcpv6_set_xid(hdr, xid);
            int pos = sizeof(dhcpv6_header_t);
            
            // Append Client ID
            dhcpv6_option_t* opt = (dhcpv6_option_t*)(buf + pos);
            opt->code = htons(OPT_CLIENTID);
            opt->len = htons(client_duid_len);
            memcpy(opt->value, client_duid, client_duid_len);
            pos += sizeof(dhcpv6_option_t) + client_duid_len;
            
            // Append IA_NA (Identity Association for Non-temporary Addresses)
            opt = (dhcpv6_option_t*)(buf + pos);
            opt->code = htons(OPT_IA_NA);
            opt->len = htons(12); // IAID (4) + T1 (4) + T2 (4)
            uint32_t* p32 = (uint32_t*)opt->value;
            p32[0] = htonl(iaid);
            p32[1] = htonl(0); // T1
            p32[2] = htonl(0); // T2
            pos += sizeof(dhcpv6_option_t) + 12;
            
            // Append IA_PD (Identity Association for Prefix Delegation) if requested
            if (request_pd) {
                opt = (dhcpv6_option_t*)(buf + pos);
                opt->code = htons(OPT_IA_PD);
                opt->len = htons(12);
                p32 = (uint32_t*)opt->value;
                p32[0] = htonl(iaid + 1); // Distinct IAID for PD
                p32[1] = 0; p32[2] = 0; 
                pos += sizeof(dhcpv6_option_t) + 12;
            }
            
            if (sendto(sock, buf, pos, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                perror("sendto failed");
            }
            state = STATE_SOLICITING;
        }
        
        // ---------------------------------------------------------------------
        // STATE: SOLICITING
        // Action: Waiting for a server to Offer us something (ADVERTISE).
        // If we get an offer, we'll try to Request it.
        // ---------------------------------------------------------------------
        else if (state == STATE_SOLICITING) {
            printf("[SOLICITING] Waiting for ADVERTISE...\n");
            struct sockaddr_in6 srv_addr;
            socklen_t slen = sizeof(srv_addr);
            ssize_t len = recvfrom(sock, rx_buf, BUF_SIZE, 0, (struct sockaddr*)&srv_addr, &slen);
            
            if (len > 0) {
                printf("[DEBUG] Packet received (%zd bytes)\n", len);
                dhcpv6_packet_meta_t meta;
                if (dhcpv6_parse(rx_buf, len, &meta) == 0) {
                    printf("[DEBUG] Parsed: Type=%d, XID=0x%x (Expected XID=0x%x)\n", 
                           meta.msg_type, meta.transaction_id, xid);
                    if (meta.msg_type == MSG_ADVERTISE && meta.transaction_id == xid) {
                        printf("[SOLICITING] Received ADVERTISE. Transitioning to REQUEST...\n");
                        
                        // Build REQUEST message based on ADVERTISE
                        dhcpv6_header_t* hdr = (dhcpv6_header_t*)buf;
                        hdr->msg_type = MSG_REQUEST;
                        dhcpv6_set_xid(hdr, xid+1); // Increment XID for new transaction
                        int pos = sizeof(dhcpv6_header_t);
                        
                        // Client ID
                        dhcpv6_option_t* opt = (dhcpv6_option_t*)(buf + pos);
                        opt->code = htons(OPT_CLIENTID);
                        opt->len = htons(client_duid_len);
                        memcpy(opt->value, client_duid, client_duid_len);
                        pos += sizeof(dhcpv6_option_t) + client_duid_len;
                        
                        // Server ID (Must include what we received)
                        if (meta.server_duid && meta.server_duid_len > 0) {
                            opt = (dhcpv6_option_t*)(buf + pos);
                            opt->code = htons(OPT_SERVERID);
                            opt->len = htons(meta.server_duid_len);
                            memcpy(opt->value, meta.server_duid, meta.server_duid_len);
                            pos += sizeof(dhcpv6_option_t) + meta.server_duid_len;
                            
                            // Save Server DUID for Renew
                            if (meta.server_duid_len < sizeof(server_duid)) {
                                memcpy(server_duid, meta.server_duid, meta.server_duid_len);
                                server_duid_len = meta.server_duid_len;
                            }
                        }
                        
                        // Request IA_NA
                        opt = (dhcpv6_option_t*)(buf + pos);
                        opt->code = htons(OPT_IA_NA);
                        opt->len = htons(12); 
                        uint32_t* p32 = (uint32_t*)opt->value;
                        p32[0] = htonl(iaid);
                        p32[1] = 0; p32[2] = 0;
                        pos += sizeof(dhcpv6_option_t) + 12;

                        // Request IA_PD if enabled
                        if (request_pd) {
                            opt = (dhcpv6_option_t*)(buf + pos);
                            opt->code = htons(OPT_IA_PD);
                            opt->len = htons(12); 
                            p32 = (uint32_t*)opt->value;
                            p32[0] = htonl(iaid + 1);
                            p32[1] = 0; p32[2] = 0;
                            pos += sizeof(dhcpv6_option_t) + 12;
                        }
                        
                        if (sendto(sock, buf, pos, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                             perror("send request failed");
                        }
                        
                        state = STATE_REQUESTING;
                        xid++;
                    }
                }
            }
        }
        
        // ---------------------------------------------------------------------
        // STATE: REQUESTING
        // Action: Officially request the IP we were offered.
        // Wait for final confirmation (REPLY).
        // If positive, we set the IP on the interface.
        // ---------------------------------------------------------------------
        else if (state == STATE_REQUESTING) {
            printf("[REQUESTING] Waiting for REPLY...\n");
            struct sockaddr_in6 srv_addr;
            socklen_t slen = sizeof(srv_addr);
            ssize_t len = recvfrom(sock, rx_buf, BUF_SIZE, 0, (struct sockaddr*)&srv_addr, &slen);
            
             if (len > 0) {
                dhcpv6_packet_meta_t meta;
                if (dhcpv6_parse(rx_buf, len, &meta) == 0) {
                    if (meta.msg_type == MSG_REPLY && meta.transaction_id == xid) {
                         printf("[REQUESTING] Received REPLY.\n");
                         
                         // Update Server DUID if present (it should be)
                         if (meta.server_duid && meta.server_duid_len > 0) {
                              if (meta.server_duid_len < sizeof(server_duid)) {
                                  memcpy(server_duid, meta.server_duid, meta.server_duid_len);
                                  server_duid_len = meta.server_duid_len;
                              }
                         }
                         
                         // --- Process IA_NA (IP Address) ---
                         if (meta.has_requested_ip) {
                             char ip_str[INET6_ADDRSTRLEN];
                             inet_ntop(AF_INET6, &meta.requested_ip, ip_str, sizeof(ip_str));
                             printf("  [IA_NA] Assigned IP: %s\n", ip_str);
                             
                             char cmd[512];
                             snprintf(cmd, sizeof(cmd), "ip -6 addr add %s/64 dev %s", ip_str, ifname);
                             printf("  [EXEC] %s\n", cmd);
                             if (system(cmd) == 0) {
                                 printf("  -> IP assigned successfully.\n");
                             } else {
                                 fprintf(stderr, "  -> Failed to assign IP.\n");
                             }
                         }
                         
                         // --- Process IA_PD (Prefix Delegation) ---
                         if (request_pd && meta.has_ia_pd && meta.has_requested_prefix) {
                             char pfx_str[INET6_ADDRSTRLEN];
                             inet_ntop(AF_INET6, &meta.requested_prefix, pfx_str, sizeof(pfx_str));
                             printf("  [IA_PD] Delegated Prefix: %s/%d\n", pfx_str, meta.requested_plen);
                             
                             // Construct address: <prefix>::1
                             struct in6_addr pd_addr = meta.requested_prefix;
                             pd_addr.s6_addr[15] = 1; 
                             
                             char full_ip[INET6_ADDRSTRLEN];
                             inet_ntop(AF_INET6, &pd_addr, full_ip, sizeof(full_ip));
                             
                             char cmd[512];
                             snprintf(cmd, sizeof(cmd), "ip -6 addr add %s/64 dev %s", full_ip, ifname);
                             printf("  [EXEC] %s\n", cmd);
                             if (system(cmd) == 0) {
                                 printf("  -> Prefix address assigned successfully.\n");
                             } else {
                                 fprintf(stderr, "  -> Failed to assign prefix address.\n");
                             }
                         }
                         
                         state = STATE_BOUND;
                    }
                }
             }
        }
        
        // ---------------------------------------------------------------------
        // STATE: BOUND
        // Action: We have an IP! Life is good.
        // Just sleep until T1 expires (usually 50% of lease time).
        // Then we try to renew.
        // ---------------------------------------------------------------------
        else if (state == STATE_BOUND) {
             printf("[BOUND] Lease acquired. Sleeping for T1 (simulated 15s)...\n");
             sleep(15); 
             // In real app, we parse T1. Here we simulate.
             
             printf("[BOUND] T1 expired. Transitioning to RENEWING...\n");
             state = STATE_RENEWING;
             xid++;
        }
        
        // ---------------------------------------------------------------------
        // STATE: RENEWING
        // Action: Send a RENEW packet to extend our time.
        // Server should reply and give us a new T1/T2.
        // ---------------------------------------------------------------------
        else if (state == STATE_RENEWING) {
             printf("[RENEWING] Sending RENEW...\n");
             
             // Build RENEW message
             dhcpv6_header_t* hdr = (dhcpv6_header_t*)buf;
             hdr->msg_type = MSG_RENEW;
             dhcpv6_set_xid(hdr, xid);
             int pos = sizeof(dhcpv6_header_t);
             
             // Client ID
             dhcpv6_option_t* opt = (dhcpv6_option_t*)(buf + pos);
             opt->code = htons(OPT_CLIENTID);
             opt->len = htons(client_duid_len);
             memcpy(opt->value, client_duid, client_duid_len);
             pos += sizeof(dhcpv6_option_t) + client_duid_len;
             
             // Server ID (Required for RENEW)
             if (server_duid_len > 0) {
                 opt = (dhcpv6_option_t*)(buf + pos);
                 opt->code = htons(OPT_SERVERID);
                 opt->len = htons(server_duid_len);
                 memcpy(opt->value, server_duid, server_duid_len);
                 pos += sizeof(dhcpv6_option_t) + server_duid_len;
             }
             
             // IA_NA
             opt = (dhcpv6_option_t*)(buf + pos);
             opt->code = htons(OPT_IA_NA);
             opt->len = htons(12);
             uint32_t* p32 = (uint32_t*)opt->value;
             p32[0] = htonl(iaid);
             p32[1] = 0; p32[2] = 0;
             pos += sizeof(dhcpv6_option_t) + 12;
             
             // IA_PD
             if (request_pd) {
                 opt = (dhcpv6_option_t*)(buf + pos);
                 opt->code = htons(OPT_IA_PD);
                 opt->len = htons(12);
                 p32 = (uint32_t*)opt->value;
                 p32[0] = htonl(iaid + 1);
                 p32[1] = 0; p32[2] = 0;
                 pos += sizeof(dhcpv6_option_t) + 12;
             }
             
             if (sendto(sock, buf, pos, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                 perror("send renew failed");
             }
             
             // Wait for REPLY
             printf("[RENEWING] Waiting for REPLY...\n");
             struct sockaddr_in6 srv_addr;
             socklen_t slen = sizeof(srv_addr);
             ssize_t len = recvfrom(sock, rx_buf, BUF_SIZE, 0, (struct sockaddr*)&srv_addr, &slen);
             
             if (len > 0) {
                 dhcpv6_packet_meta_t meta;
                 if (dhcpv6_parse(rx_buf, len, &meta) == 0) {
                     if (meta.msg_type == MSG_REPLY && meta.transaction_id == xid) {
                         printf("[RENEWING] Received REPLY. Lease renewed.\n");
                         
                         // Loop back to BOUND
                         state = STATE_BOUND;
                         xid++;
                     }
                 }
             } else {
                 // Timeout or error: Keep retrying or go to REBIND. 
                 // For demo, just retry RENEW loop or go back to BOUND to retry later
                 printf("[RENEWING] No reply / Timeout. Retrying later...\n");
                 sleep(5);
             }
        }
    }
    
    close(sock);
    return 0;
}
