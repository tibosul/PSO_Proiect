#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dns_packet.h"
#include "dns_config.h"
#include "zone_manager.h"
#include "dns_cache.h"
#include "network_utils.h"
#include "dns_parser.h"
#include "error_codes.h"

#define BUFFER_SIZE 512
#define DEFAULT_PORT 53
#define DEFAULT_IP "0.0.0.0"

static int sockfd = -1;
static config_node* config_root = NULL;

void handle_signal(int signal)
{
    printf("Server shutting down (caught signal: %d)\n", signal);

    if(sockfd > 0)
    {
        close(sockfd);
    }

    if(config_root != NULL)
    {
        free_config(config_root);
    }

    exit(0);
}

const char* get_global_option(config_node* root, const char* key)
{
    config_node* current = root;

    while(current != NULL)
    {
        if(current->type == CONFIG_OPTIONS && current->pairs != NULL)
        {
            for(int i = 0; current->pairs[i].key != NULL; i++)
            {
                if(strcmp(current->pairs[i].key, key) == 0)
                {
                    return current->pairs[i].value;
                }
            }
        }

        current = current->next;
    }

    return NULL;
}

int main()
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Loading DNS Server configuration...\n");
    config_root = parse_config_file("config/dns.conf");

    if(config_root == NULL)
    {
        printf("Error: Failed to load '../config/dns.conf'! Default values will be used.\n");   
    }

    const char* conf_ip = get_global_option(config_root, "listen_ip");
    const char* conf_port = get_global_option(config_root, "port");

    const char* listen_ip;

    if(conf_ip == NULL)
    {
        listen_ip = DEFAULT_IP;
    } else {
        listen_ip = conf_ip;
    }

    uint16_t port;

    if(conf_port == NULL)
    {
        port = DEFAULT_PORT;
    } else {
        port = (uint16_t)atoi(conf_port);
    }

    printf("Initializing DNS Zone Manager...\n");
    zone_manager_init(config_root);

    printf("Initializing DNS Cache...\n");
    cache_initialize();

    sockfd = initialize_udp_socket(listen_ip, port);

    if(sockfd < 0)
    {
        printf("Error: Failed to bind to %s:%d\n", listen_ip, port);
        
        if(config_root != NULL)
        {
            free_config(config_root);
        }

        return ERR_FAILED_TO_BIND_SOCKET;
    }

    printf("DNS Server running on %s:%d\n", listen_ip, port);

    unsigned char buffer[BUFFER_SIZE];
    unsigned char response_buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while(1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        memset(response_buffer, 0, BUFFER_SIZE);

        ssize_t len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);

        if(len < 0)
        {
            printf("Warning: Something went wrong while reading.\n");
        }

        char qname[256] = {0};
        uint16_t qtype = 0;

        int parse_res = parse_dns_request(buffer, (size_t)len, qname, &qtype);

        if(parse_res != 0)
        {
            printf("Warning: Problem parsing packet, packet data might be corrupt or incomplete.\n");
        }

        printf("Query: %s asked for '%s' (Type: %d)\n", inet_ntoa(client_addr.sin_addr), qname, qtype);

        cache_entry* cached_entry = cache_lookup(qname);

        if(cached_entry != NULL)
        {
            printf("Cache hit: Sending cached response.\n");

            memcpy(response_buffer, cached_entry->response_buffer, cached_entry->response_length);
            
            dns_header* req_hdr = (dns_header*)buffer;
            dns_header* res_hdr = (dns_header*)response_buffer;
            res_hdr->identification = req_hdr->identification; 

            sendto(sockfd, response_buffer, cached_entry->response_length, 0, (struct sockaddr *)&client_addr, addr_len);
        }

        size_t resp_len = 0;
        bool local_found = handle_local_zone_query(qname, qtype, buffer, (size_t)len, response_buffer, &resp_len);

        if(local_found == true)
        {
            // se poate da raspuns local
            printf("Local zone hit: Sending authoritative response.\n");
            sendto(sockfd, response_buffer, resp_len, 0, (struct sockaddr *)&client_addr, addr_len);
        }
        else 
        {
            // redirectionare catre DNS-ul Google
            const char* upstream_dns = "8.8.8.8";

            size_t fwd_len = forward_to_upstream(upstream_dns, buffer, (size_t)len, response_buffer, 2); // timeout 2 secunde

            if(fwd_len > 0)
            {
                printf("Got forward response!\n");

                cache_insert(qname, (const char*)response_buffer, (uint16_t)fwd_len, 60);

                sendto(sockfd, response_buffer, fwd_len, 0, (struct sockaddr *)&client_addr, addr_len);
            }
            else
            {
                printf("Failed to get forward response (timeout or error)!\n");
            }
        }
    }

    close(sockfd);

    if(config_root != NULL)
    {
        free_config(config_root);
    }

    return 0;
}