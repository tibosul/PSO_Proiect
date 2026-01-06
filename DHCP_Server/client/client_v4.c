/*
 * DHCPv4 Client Implementation
 * 
 * Features:
 * - DISCOVER, OFFER, REQUEST, ACK handshake.
 * - IPv4 Address Assignment.
 * - System integration via 'ip' command.
 * - Lease renewal (RENEWING state).
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

#include "../DHCPv4/include/src/dhcp_common.h"
#include "client_v4.h"

/**
 * @brief Retrieves the MAC address of the specified network interface.
 * 
 * @param ifname Name of the interface (e.g., "eth0").
 * @param mac Buffer to store the 6-byte MAC address.
 * @return int 0 on success, -1 on failure.
 */
int get_mac_address_v4(const char* ifname, uint8_t* mac) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    
    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';
    
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
int get_if_index_v4(const char* ifname) {
    return if_nametoindex(ifname);
}

/**
 * @brief Parse DHCP options to extract message type
 */
uint8_t get_dhcp_message_type(const struct dhcp_packet* packet) {
    int offset = 4; // Skip magic cookie
    
    // Verify magic cookie
    uint32_t cookie;
    memcpy(&cookie, packet->options, 4);
    if (ntohl(cookie) != DHCP_MAGIC_COOKIE) {
        return 0;
    }
    
    while (offset < DHCP_OPTIONS_SIZE) {
        uint8_t code = packet->options[offset];
        
        if (code == DHCP_OPT_END) break;
        if (code == DHCP_OPT_PAD) {
            offset++;
            continue;
        }
        
        uint8_t len = packet->options[offset + 1];
        
        if (code == DHCP_OPT_MESSAGE_TYPE && len == 1) {
            return packet->options[offset + 2];
        }
        
        offset += 2 + len;
    }
    
    return 0;
}

/**
 * @brief Get requested IP address from DHCP option
 */
int get_dhcp_requested_ip(const struct dhcp_packet* packet, struct in_addr* ip) {
    int offset = 4;
    
    uint32_t cookie;
    memcpy(&cookie, packet->options, 4);
    if (ntohl(cookie) != DHCP_MAGIC_COOKIE) {
        return -1;
    }
    
    while (offset < DHCP_OPTIONS_SIZE) {
        uint8_t code = packet->options[offset];
        
        if (code == DHCP_OPT_END) break;
        if (code == DHCP_OPT_PAD) {
            offset++;
            continue;
        }
        
        uint8_t len = packet->options[offset + 1];
        
        if (code == DHCP_OPT_SERVER_ID && len == 4) {
            memcpy(&ip->s_addr, &packet->options[offset + 2], 4);
            return 0;
        }
        
        offset += 2 + len;
    }
    
    return -1;
}

/**
 * @brief Add a DHCP option to the packet
 */
int add_dhcp_option(struct dhcp_packet* packet, uint8_t option_code, uint8_t len, void* data) {
    int offset = 4;
    
    // Find end of options
    while (offset < (DHCP_OPTIONS_SIZE - 4)) {
        if (packet->options[offset] == DHCP_OPT_END) {
            break;
        }
        
        if (packet->options[offset] == DHCP_OPT_PAD) {
            offset++;
        } else {
            uint8_t opt_len = packet->options[offset + 1];
            offset += 2 + opt_len;
        }
    }
    
    // Check space
    if (offset + 2 + len + 1 > DHCP_OPTIONS_SIZE) {
        return -1;
    }
    
    packet->options[offset] = option_code;
    packet->options[offset + 1] = len;
    memcpy(&packet->options[offset + 2], data, len);
    packet->options[offset + 2 + len] = DHCP_OPT_END;
    
    return 0;
}

/**
 * @brief Build a DHCP DISCOVER packet
 */
void build_discover(struct dhcp_packet* packet, uint32_t xid, uint8_t* mac) {
    memset(packet, 0, sizeof(struct dhcp_packet));
    
    packet->op = BOOTREQUEST;
    packet->htype = HTYPE_ETHER;
    packet->hlen = 6;
    packet->xid = htonl(xid);
    memcpy(packet->chaddr, mac, 6);
    
    // Set magic cookie
    uint32_t cookie = htonl(DHCP_MAGIC_COOKIE);
    memcpy(packet->options, &cookie, 4);
    packet->options[4] = DHCP_OPT_END;
    
    // Add message type option
    uint8_t msg_type = DHCP_DISCOVER;
    add_dhcp_option(packet, DHCP_OPT_MESSAGE_TYPE, 1, &msg_type);
}

/**
 * @brief Build a DHCP REQUEST packet
 */
void build_request(struct dhcp_packet* packet, uint32_t xid, uint8_t* mac, 
                   struct in_addr requested_ip, struct in_addr server_id) {
    memset(packet, 0, sizeof(struct dhcp_packet));
    
    packet->op = BOOTREQUEST;
    packet->htype = HTYPE_ETHER;
    packet->hlen = 6;
    packet->xid = htonl(xid);
    memcpy(packet->chaddr, mac, 6);
    
    // Set magic cookie
    uint32_t cookie = htonl(DHCP_MAGIC_COOKIE);
    memcpy(packet->options, &cookie, 4);
    packet->options[4] = DHCP_OPT_END;
    
    // Add message type option
    uint8_t msg_type = DHCP_REQUEST;
    add_dhcp_option(packet, DHCP_OPT_MESSAGE_TYPE, 1, &msg_type);
    
    // Add requested IP
    add_dhcp_option(packet, DHCP_OPT_REQUESTED_IP, 4, &requested_ip.s_addr);
    
    // Add server ID
    add_dhcp_option(packet, DHCP_OPT_SERVER_ID, 4, &server_id.s_addr);
}

/**
 * @brief Build a DHCP RENEW packet (REQUEST with ciaddr set)
 */
void build_renew(struct dhcp_packet* packet, uint32_t xid, uint8_t* mac, 
                 struct in_addr current_ip) {
    memset(packet, 0, sizeof(struct dhcp_packet));
    
    packet->op = BOOTREQUEST;
    packet->htype = HTYPE_ETHER;
    packet->hlen = 6;
    packet->xid = htonl(xid);
    packet->ciaddr = current_ip; // Set current IP for renewal
    memcpy(packet->chaddr, mac, 6);
    
    // Set magic cookie
    uint32_t cookie = htonl(DHCP_MAGIC_COOKIE);
    memcpy(packet->options, &cookie, 4);
    packet->options[4] = DHCP_OPT_END;
    
    // Add message type option
    uint8_t msg_type = DHCP_REQUEST;
    add_dhcp_option(packet, DHCP_OPT_MESSAGE_TYPE, 1, &msg_type);
}

int main(int argc, char** argv) {
    // --- Argument Parsing ---
    char* ifname = NULL;
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        return 1;
    }
    
    ifname = argv[1];
    
    // --- Interface Setup ---
    int ifindex = get_if_index_v4(ifname);
    if (ifindex == 0) {
        perror("Error getting interface index");
        return 1;
    }
    
    uint8_t mac[6];
    if (get_mac_address_v4(ifname, mac) != 0) {
        perror("Error getting MAC address");
        return 1;
    }
    
    printf("Starting DHCPv4 Client on %s (Index %d)\n", ifname, ifindex);
    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // --- Socket Setup ---
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    // Set socket to allow broadcast
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt SO_BROADCAST");
        close(sock);
        return 1;
    }
    
    // Bind to client port
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(DHCP_CLIENT_PORT);
    client_addr.sin_addr.s_addr = INADDR_ANY;
    
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR");
    }
    
    if (bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind failed");
        close(sock);
        return 1;
    }
    
    // Destination: Broadcast to server
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DHCP_SERVER_PORT);
    dest_addr.sin_addr.s_addr = INADDR_BROADCAST;
    
    // --- State Machine ---
    client_state_v4_t state = STATE_V4_INIT;
    uint32_t xid = (uint32_t)time(NULL);
    
    struct dhcp_packet tx_packet;
    struct dhcp_packet rx_packet;
    
    struct in_addr offered_ip = {0};
    struct in_addr server_id = {0};
    struct in_addr assigned_ip = {0};
    
    // =========================================================================
    // DHCPv4 Client State Machine
    // This loop handles: DISCOVER -> OFFER -> REQUEST -> ACK -> BOUND -> RENEW
    // =========================================================================
    while(1) {
        
        // ---------------------------------------------------------------------
        // STATE: INIT
        // Action: Send DHCPDISCOVER to find available DHCP servers
        // ---------------------------------------------------------------------
        if (state == STATE_V4_INIT) {
            printf("[INIT] Sending DHCPDISCOVER...\n");
            
            build_discover(&tx_packet, xid, mac);
            
            if (sendto(sock, &tx_packet, sizeof(tx_packet), 0, 
                      (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                perror("sendto discover failed");
            }
            
            state = STATE_V4_SELECTING;
        }
        
        // ---------------------------------------------------------------------
        // STATE: SELECTING
        // Action: Wait for DHCPOFFER from server
        // ---------------------------------------------------------------------
        else if (state == STATE_V4_SELECTING) {
            printf("[SELECTING] Waiting for DHCPOFFER...\n");
            
            struct sockaddr_in srv_addr;
            socklen_t slen = sizeof(srv_addr);
            
            // Set timeout
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            ssize_t len = recvfrom(sock, &rx_packet, sizeof(rx_packet), 0, 
                                  (struct sockaddr*)&srv_addr, &slen);
            
            if (len > 0) {
                uint32_t rx_xid = ntohl(rx_packet.xid);
                uint8_t msg_type = get_dhcp_message_type(&rx_packet);
                
                printf("[DEBUG] Received message type=%d, XID=0x%x (Expected=0x%x)\n",
                       msg_type, rx_xid, xid);
                
                if (msg_type == DHCP_OFFER && rx_xid == xid) {
                    offered_ip = rx_packet.yiaddr;
                    get_dhcp_requested_ip(&rx_packet, &server_id);
                    
                    printf("[SELECTING] Received DHCPOFFER: IP=%s\n", 
                           inet_ntoa(offered_ip));
                    
                    // Send REQUEST
                    printf("[SELECTING] Sending DHCPREQUEST...\n");
                    xid++;
                    build_request(&tx_packet, xid, mac, offered_ip, server_id);
                    
                    if (sendto(sock, &tx_packet, sizeof(tx_packet), 0,
                              (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                        perror("sendto request failed");
                    }
                    
                    state = STATE_V4_REQUESTING;
                }
            } else {
                printf("[SELECTING] Timeout waiting for OFFER, retrying...\n");
                state = STATE_V4_INIT;
                sleep(1);
            }
        }
        
        // ---------------------------------------------------------------------
        // STATE: REQUESTING
        // Action: Wait for DHCPACK to confirm the lease
        // ---------------------------------------------------------------------
        else if (state == STATE_V4_REQUESTING) {
            printf("[REQUESTING] Waiting for DHCPACK...\n");
            
            struct sockaddr_in srv_addr;
            socklen_t slen = sizeof(srv_addr);
            
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            ssize_t len = recvfrom(sock, &rx_packet, sizeof(rx_packet), 0,
                                  (struct sockaddr*)&srv_addr, &slen);
            
            if (len > 0) {
                uint32_t rx_xid = ntohl(rx_packet.xid);
                uint8_t msg_type = get_dhcp_message_type(&rx_packet);
                
                if (msg_type == DHCP_ACK && rx_xid == xid) {
                    assigned_ip = rx_packet.yiaddr;
                    
                    printf("[REQUESTING] Received DHCPACK: IP=%s\n",
                           inet_ntoa(assigned_ip));
                    
                    // Configure the interface with the assigned IP
                    char cmd[256];
                    snprintf(cmd, sizeof(cmd), "ip addr add %s/24 dev %s",
                            inet_ntoa(assigned_ip), ifname);
                    printf("[EXEC] %s\n", cmd);
                    
                    if (system(cmd) == 0) {
                        printf("-> IP assigned successfully.\n");
                    } else {
                        fprintf(stderr, "-> Failed to assign IP.\n");
                    }
                    
                    state = STATE_V4_BOUND;
                } else if (msg_type == DHCP_NAK) {
                    printf("[REQUESTING] Received DHCPNAK. Restarting...\n");
                    state = STATE_V4_INIT;
                    xid++;
                    sleep(1);
                }
            } else {
                printf("[REQUESTING] Timeout waiting for ACK, retrying REQUEST...\n");
                sleep(1);
                // Retry REQUEST
                if (sendto(sock, &tx_packet, sizeof(tx_packet), 0,
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                    perror("sendto request retry failed");
                }
            }
        }
        
        // ---------------------------------------------------------------------
        // STATE: BOUND
        // Action: Lease acquired. Wait for T1 (renewal time) then try to renew
        // ---------------------------------------------------------------------
        else if (state == STATE_V4_BOUND) {
            printf("[BOUND] Lease acquired. Sleeping for T1 (simulated 30s)...\n");
            sleep(30);
            
            printf("[BOUND] T1 expired. Transitioning to RENEWING...\n");
            state = STATE_V4_RENEWING;
            xid++;
        }
        
        // ---------------------------------------------------------------------
        // STATE: RENEWING
        // Action: Send REQUEST (with ciaddr) to renew the lease
        // ---------------------------------------------------------------------
        else if (state == STATE_V4_RENEWING) {
            printf("[RENEWING] Sending DHCP REQUEST to renew lease...\n");
            
            build_renew(&tx_packet, xid, mac, assigned_ip);
            
            if (sendto(sock, &tx_packet, sizeof(tx_packet), 0,
                      (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                perror("sendto renew failed");
            }
            
            struct sockaddr_in srv_addr;
            socklen_t slen = sizeof(srv_addr);
            
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            ssize_t len = recvfrom(sock, &rx_packet, sizeof(rx_packet), 0,
                                  (struct sockaddr*)&srv_addr, &slen);
            
            if (len > 0) {
                uint32_t rx_xid = ntohl(rx_packet.xid);
                uint8_t msg_type = get_dhcp_message_type(&rx_packet);
                
                if (msg_type == DHCP_ACK && rx_xid == xid) {
                    printf("[RENEWING] Lease renewed successfully.\n");
                    state = STATE_V4_BOUND;
                    xid++;
                } else if (msg_type == DHCP_NAK) {
                    printf("[RENEWING] Received NAK. Releasing and restarting...\n");
                    // Should release the IP here
                    char cmd[256];
                    snprintf(cmd, sizeof(cmd), "ip addr del %s/24 dev %s",
                            inet_ntoa(assigned_ip), ifname);
                    system(cmd);
                    
                    state = STATE_V4_INIT;
                    xid++;
                    sleep(1);
                }
            } else {
                printf("[RENEWING] Timeout. Retrying later...\n");
                sleep(5);
            }
        }
    }
    
    close(sock);
    return 0;
}
