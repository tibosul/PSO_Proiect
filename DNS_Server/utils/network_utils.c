#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#include "dns_config.h"
#include "dns_packet.h"
#include "dns_cache.h"
#include "string_utils.h"
#include "zone_manager.h"
#include "network_utils.h"

#define MAX_QNAME_TEXT_LEN 256
#define MAX_PACKET_SIZE 512



