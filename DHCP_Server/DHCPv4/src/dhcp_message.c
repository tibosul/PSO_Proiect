#include "../include/dhcp_message.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

void dhcp_message_init(struct dhcp_packet *packet, uint8_t op, uint32_t xid,
                       uint8_t *chaddr) {
  memset(packet, 0, sizeof(struct dhcp_packet));
  packet->op = op;
  packet->htype = HTYPE_ETHER;
  packet->hlen = 6;
  packet->xid = xid;
  if (chaddr) {
    memcpy(packet->chaddr, chaddr, 6);
  }

  // Set magic cookie
  uint32_t cookie = htonl(DHCP_MAGIC_COOKIE);
  memcpy(packet->options, &cookie, 4);

  // Set end option at start (empty options)
  packet->options[4] = DHCP_OPT_END;
}

int dhcp_message_add_option(struct dhcp_packet *packet, uint8_t option_code,
                            uint8_t len, void *data) {
  // Find current end of options
  int offset = 4; // Start after cookie

  while (offset < 308) { // 312 - 1 for new tag - 1 for len - 1 for END
    if (packet->options[offset] == DHCP_OPT_END) {
      break;
    }

    // Skip existing option: tag + 1 for len + len bytes
    if (packet->options[offset] == DHCP_OPT_PAD) {
      offset++;
    } else {
      uint8_t opt_len = packet->options[offset + 1];
      offset += 2 + opt_len;
    }
  }

  // Check if we have space: tag (1) + len (1) + data (len) + END (1)
  if (offset + 2 + len + 1 > 312) {
    return -1; // Overflow
  }

  packet->options[offset] = option_code;
  packet->options[offset + 1] = len;
  memcpy(&packet->options[offset + 2], data, len);
  packet->options[offset + 2 + len] = DHCP_OPT_END;

  return 0;
}

int dhcp_message_add_option32(struct dhcp_packet *packet, uint8_t option_code,
                              uint32_t data) {
  uint32_t net_data = htonl(data);
  return dhcp_message_add_option(packet, option_code, 4, &net_data);
}

int dhcp_message_add_option_ip(struct dhcp_packet *packet, uint8_t option_code,
                               struct in_addr ip) {
  return dhcp_message_add_option(packet, option_code, 4, &ip.s_addr);
}

uint8_t *dhcp_message_get_option(const struct dhcp_packet *packet,
                                 uint8_t option_code, uint8_t *out_len) {
  int offset = 4;

  // Check magic cookie
  uint32_t cookie;
  memcpy(&cookie, packet->options, 4);
  if (ntohl(cookie) != DHCP_MAGIC_COOKIE) {
    return NULL;
  }

  while (offset < 312) {
    uint8_t code = packet->options[offset];

    if (code == DHCP_OPT_END) {
      break;
    }

    if (code == DHCP_OPT_PAD) {
      offset++;
      continue;
    }

    uint8_t len = packet->options[offset + 1];

    if (code == option_code) {
      if (out_len)
        *out_len = len;
      return (uint8_t *)&packet->options[offset + 2];
    }

    offset += 2 + len;
  }

  return NULL;
}

uint8_t dhcp_message_get_type(const struct dhcp_packet *packet) {
  uint8_t *opt_ptr =
      dhcp_message_get_option(packet, DHCP_OPT_MESSAGE_TYPE, NULL);
  if (opt_ptr) {
    return *opt_ptr;
  }
  return 0;
}

int dhcp_message_validate(struct dhcp_packet *packet, ssize_t len) {
  if (len <
      (ssize_t)sizeof(struct dhcp_packet) - 312 + 4) { // Header + Magic Cookie
    return -1;
  }

  uint32_t cookie;
  memcpy(&cookie, packet->options, 4);
  if (ntohl(cookie) != DHCP_MAGIC_COOKIE) {
    return -1;
  }

  return 0;
}

void dhcp_message_make_offer(struct dhcp_packet *offer,
                             const struct dhcp_packet *discover,
                             struct dhcp_lease_t *lease,
                             struct dhcp_subnet_t *subnet,
                             struct dhcp_global_options_t *global_opts) {
  (void)global_opts;
  dhcp_message_init(offer, BOOTREPLY, discover->xid,
                    (uint8_t *)discover->chaddr);

  // Standard fields
  offer->yiaddr = lease->ip_address;
  offer->siaddr = subnet->next_server; // Assuming next_server is filled

  // Options
  uint8_t type = DHCP_OFFER;
  dhcp_message_add_option(offer, DHCP_OPT_MESSAGE_TYPE, 1, &type);

  dhcp_message_add_option_ip(offer, DHCP_OPT_SUBNET_MASK, subnet->netmask);
  dhcp_message_add_option_ip(offer, DHCP_OPT_ROUTER, subnet->router);
  dhcp_message_add_option32(offer, DHCP_OPT_LEASE_TIME,
                            (uint32_t)(lease->end_time - lease->start_time));
  dhcp_message_add_option_ip(offer, DHCP_OPT_SERVER_ID,
                             subnet->router); // Usually server IP

  if (subnet->dns_server_count > 0) {
    dhcp_message_add_option(offer, DHCP_OPT_DNS_SERVERS,
                            subnet->dns_server_count * 4, subnet->dns_servers);
  }
}

void dhcp_message_make_ack(struct dhcp_packet *ack,
                           const struct dhcp_packet *request,
                           struct dhcp_lease_t *lease,
                           struct dhcp_subnet_t *subnet,
                           struct dhcp_global_options_t *global_opts) {
  (void)global_opts;
  dhcp_message_init(ack, BOOTREPLY, request->xid, (uint8_t *)request->chaddr);

  ack->yiaddr = lease->ip_address;

  // Options
  uint8_t type = DHCP_ACK;
  dhcp_message_add_option(ack, DHCP_OPT_MESSAGE_TYPE, 1, &type);

  dhcp_message_add_option_ip(ack, DHCP_OPT_SUBNET_MASK, subnet->netmask);
  dhcp_message_add_option_ip(ack, DHCP_OPT_ROUTER, subnet->router);
  dhcp_message_add_option32(ack, DHCP_OPT_LEASE_TIME,
                            (uint32_t)(lease->end_time - lease->start_time));
  dhcp_message_add_option_ip(ack, DHCP_OPT_SERVER_ID,
                             subnet->router); // Usually server IP

  if (subnet->dns_server_count > 0) {
    dhcp_message_add_option(ack, DHCP_OPT_DNS_SERVERS,
                            subnet->dns_server_count * 4, subnet->dns_servers);
  }
}

void dhcp_message_make_nak(struct dhcp_packet *nak,
                           const struct dhcp_packet *request,
                           struct in_addr server_id) {
  dhcp_message_init(nak, BOOTREPLY, request->xid, (uint8_t *)request->chaddr);

  // No YIADDR for NAK

  // Options
  uint8_t type = DHCP_NAK;
  dhcp_message_add_option(nak, DHCP_OPT_MESSAGE_TYPE, 1, &type);
  dhcp_message_add_option_ip(nak, DHCP_OPT_SERVER_ID, server_id);
}
