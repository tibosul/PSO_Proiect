#ifndef DHCP_MESSAGE_H
#define DHCP_MESSAGE_H

#include "config_v4.h"
#include "dhcp_common.h"
#include "lease_v4.h"
#include <netinet/in.h>
#include <stdint.h>

/**
 * @brief Initialize a DHCP packet structure.
 * @param packet Pointer to packet to initialize.
 * @param type DHCP message type (DHCP_OFFER, DHCP_ACK, etc.).
 */
void dhcp_message_init(struct dhcp_packet *packet, uint8_t op, uint32_t xid,
                       uint8_t *chaddr);

/**
 * @brief Add a DHCP option to the packet.
 * @param packet Pointer to packet.
 * @param option_code Option code (e.g., DHCP_OPT_SUBNET_MASK).
 * @param len Length of option data.
 * @param data Pointer to option data.
 * @return 0 on success, -1 on failure (e.g. overflow).
 */
int dhcp_message_add_option(struct dhcp_packet *packet, uint8_t option_code,
                            uint8_t len, void *data);

/**
 * @brief Add a 32-bit integer option (network byte order).
 */
int dhcp_message_add_option32(struct dhcp_packet *packet, uint8_t option_code,
                              uint32_t data);

/**
 * @brief Add an IP address option.
 */
int dhcp_message_add_option_ip(struct dhcp_packet *packet, uint8_t option_code,
                               struct in_addr ip);

/**
 * @brief Encapsulate a DHCPOFFER message.
 */
void dhcp_message_make_offer(struct dhcp_packet *offer,
                             const struct dhcp_packet *discover,
                             struct dhcp_lease_t *lease,
                             struct dhcp_subnet_t *subnet,
                             struct dhcp_global_options_t *global_opts);

/**
 * @brief Encapsulate a DHCPACK message.
 */
void dhcp_message_make_ack(struct dhcp_packet *ack,
                           const struct dhcp_packet *request,
                           struct dhcp_lease_t *lease,
                           struct dhcp_subnet_t *subnet,
                           struct dhcp_global_options_t *global_opts);

/**
 * @brief Encapsulate a DHCPNAK message.
 */
void dhcp_message_make_nak(struct dhcp_packet *nak,
                           const struct dhcp_packet *request,
                           struct in_addr server_id);

/**
 * @brief Validate a DHCP packet (magic cookie, etc.).
 * @return 0 on success, -1 on invalid packet.
 */
int dhcp_message_validate(struct dhcp_packet *packet, ssize_t len);

/**
 * @brief Extract a DHCP option from a packet.
 * @return Pointer to option data or NULL if not found.
 */
uint8_t *dhcp_message_get_option(const struct dhcp_packet *packet,
                                 uint8_t option_code, uint8_t *out_len);

/**
 * @brief Extract DHCP message type.
 */
uint8_t dhcp_message_get_type(const struct dhcp_packet *packet);

#endif // DHCP_MESSAGE_H
