#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include "dns_cache.h" 

#define TEST_TTL 10

int main()
{
    char domain_name[][50] = {"www.mta.ro", "www.google.com", "wiki.mta.ro", "www.youtube.com"};
    char IP[][100] = {"192.124.249.79", "142.250.190.68", "213.177.4.166", "142.250.191.238"};
    
    cache_initialize(); 
    
    cache_insert(domain_name[0], IP[0], 15, TEST_TTL);
    cache_insert(domain_name[1], IP[1], 14, TEST_TTL + 5);
    cache_insert(domain_name[2], IP[2], 14, TEST_TTL + 10);
    cache_insert(domain_name[3], IP[3], 16, TEST_TTL + 15);

    char *lookup_domain_name = domain_name[0]; 
    cache_entry *entry = cache_lookup(lookup_domain_name);
    entry = cache_lookup(domain_name[2]);

    return 0;
}
