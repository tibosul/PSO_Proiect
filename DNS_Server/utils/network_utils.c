#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
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
#include "error_codes.h"

#define MAX_QNAME_TEXT_LEN 256
#define MAX_PACKET_SIZE 512

int initialize_udp_socket(const char* ip, uint16_t port)
{
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) // creare socket udp
    {
        perror("Network error: failed to create socket!\n");
        return ERR_INPUT_OUTPUT;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) 
    {
        printf("Network error: Invalid or unsupported address: %s!\n", ip);
        close(sockfd);
        return ERR_INVALID_ARGUMENT;
    }


    if(bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Network error: Failed to bind socket!\n");
        close(sockfd);
        return ERR_FAILED_TO_BIND_SOCKET;
    }

    printf("Server listening on: %s:%d\n", ip, port);
    return sockfd;
}

size_t forward_to_upstream(const char* upstream_ip, const unsigned char* query_buf, size_t query_len, unsigned char* response_buf, int timeout_seconds)
{
    int sockfd;
    struct sockaddr_in upstream_addr;
    ssize_t recv_len;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0)
    {
        return 0;
    }

    // creare timeout pentru prevenirea blocajelor infinite
    struct timeval tv;
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        close(sockfd);
        return 0;
    }

    memset(&upstream_addr, 0, sizeof(upstream_addr));
    upstream_addr.sin_family = AF_INET;
    upstream_addr.sin_port = htons(53); // portul DNS default atat pentru TCP, cat si pentru UDP - 53

    if(inet_pton(AF_INET, upstream_ip, &upstream_addr.sin_addr) <= 0)
    {
        close(sockfd);
        return 0;
    }

    if(sendto(sockfd, query_buf, query_len, 0, (const struct sockaddr *)&upstream_addr, sizeof(upstream_addr)) < 0)
    {
        perror("Failed to send query to upstream");
        close(sockfd);
        return 0;
    }

    recv_len = recvfrom(sockfd, response_buf, MAX_PACKET_SIZE, 0, NULL, NULL);

    close(sockfd);

    if(recv_len < 0)
    {
        return 0;
    }

    return (size_t)recv_len;
}
