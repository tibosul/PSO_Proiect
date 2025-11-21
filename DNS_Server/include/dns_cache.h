#ifndef DNS_CACHE_H
#define DNS_CACHE_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define CACHE_CHARS 27
#define MAX_PACKET_SIZE 512

typedef struct TrieNode{
    bool is_leaf;
    uint32_t expires_at;
    size_t response_length;
    char response_buffer[MAX_PACKET_SIZE];

    struct TrieNode* child[CACHE_CHARS];
}cache_entry;

int get_trie_index(char c);
cache_entry* new_cache_entry();
void cache_insert(char* query_name, const char* response_buffer, uint16_t response_length, uint32_t ttl);
cache_entry* cache_lookup(char* query_name);
void cache_initialize();

#endif