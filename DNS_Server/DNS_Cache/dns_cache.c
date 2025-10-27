#include "dns_cache.h"

cache_entry* root_node = NULL;

int get_trie_index(char c)
{
    if(c >= 'a' && c <= 'z')
    {
        return c - 'a';
    }
    else if(c == '.')
    {
        return 26;
    }
    else
    {
        return -1;
    }
}

cache_entry* new_cache_entry()
{
    cache_entry* node = (cache_entry*)malloc(sizeof(cache_entry));

    if(node == NULL)
    {
        perror("Error allocating memory for new cache entry!\n");
        exit(-1);
    }

    node->is_leaf = false;
    node->expires_at = 0;
    node->response_length = 0;

    for(int i = 0; i < CACHE_CHARS; i++)
    {
        node->child[i] = NULL;
    }
    return node;
}

void cache_initialize()
{
    root_node = new_cache_entry();
    printf("Initialized DNS cache.\n");
}

void cache_insert(char* query_name, const char* response_buffer, uint16_t response_length, uint32_t ttl)
{
    int len = strlen(query_name);
    cache_entry* search_node = root_node;

    for(int i = 0; i < len; i++)
    {
        int index = get_trie_index(query_name[i]);

        if(index == -1)
        {
            fprintf(stderr, "Invalid character detected in query name: '%c'!\n", query_name[i]);
            return;
        }   

        if(search_node->child[index] == NULL)
        {
            search_node->child[index] = new_cache_entry();
        }

        search_node = search_node->child[index];
    }

    search_node->is_leaf = true;
    search_node->expires_at = (uint32_t)time(NULL) + ttl;

    search_node->response_length = response_length;
    memcpy(search_node->response_buffer, response_buffer, response_length);

    printf("New cache entry has been saved: %s. Time to live: %u seconds.\n", query_name, ttl);
}

cache_entry* cache_lookup(char* query_name)
{
    cache_entry* search_node = root_node;
    int len = strlen(query_name);

    for(int i = 0; i < len; i++)
    {
        int index = get_trie_index(query_name[i]);

        if(index == -1 || search_node->child[index] == NULL)
        {
            return NULL; // nu a fost gasit
        }

        search_node = search_node->child[index];
    }

    if(search_node != NULL && search_node->is_leaf == true)
    {
        uint32_t current_time = (uint32_t)time(NULL);

        if(current_time < search_node->expires_at) 
        {
            printf("Cache entry found: %s. Expires in %u seconds.\n", query_name, search_node->expires_at - current_time);
            return search_node; // a fost gasit numele
        } else {
          printf("Cache entry %s was found but has expired.\n", query_name);
          search_node->is_leaf = false;
          search_node->expires_at = 0;
          
          return NULL; // expirat
        }
    }

    return NULL; // nu a fost gasit
}