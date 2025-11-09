#ifndef DNS_CONFIG_H
#define DNS_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CONFIG_OPTIONS,
    CONFIG_ZONE,
    CONFIG_INCLUDE,
    CONFIG_UNKNOWN
} config_node_type;

typedef struct {
    char *key;
    char *value;         
    config_node *sub_block;  
} config_pair;

typedef struct {
    config_node_type type;
    char *name;              
    char *zone_file;         
    config_pair *pairs;          
    config_node *next;          
} config_node;

config_node *config_parse_file(const char *path);
void config_free(config_node *root);
void config_dump(config_node *root); 

#endif