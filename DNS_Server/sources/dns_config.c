#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/dns_config.h"

static void trim(char *s)
{
    char *e = s + strlen(s) - 1;
    
    while(*s && isspace(*s)) 
    {
        s++;
    }
    
    while(e >= s && isspace(*e)) 
    {
        *e-- = '\0';
    }
    memmove(s, s, e - s + 2);
}

static char *dupstr(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    
    if(p != NULL) 
    {
        strcpy(p, s);
    }

    return p;
}

typedef enum {
    TOK_EOF, 
    TOK_IDENT,
    TOK_STRING,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMI,
    TOK_INCLUDE
}token_type;

typedef struct {
    token_type type;
    char *text;          
    size_t len;
}token;

static void next_token(const char **pp, token *t)
{
    const char *p = *pp;
    while (isspace(*p)) p++;

    if(*p == '\0' || *p == '#') { t->type = TOK_EOF; t->text = (char*)p; t->len = 0; *pp = p; return; }

    if(*p == '{') 
    { 
        t->type = TOK_LBRACE;
        t->text = (char*)p;
        t->len = 1; 
        *pp = p+1; 
        
        return; 
    }
    if(*p == '}') { t->type = TOK_RBRACE; t->text = (char*)p; t->len = 1; *pp = p+1; return; }
    if(*p == ';') { t->type = TOK_SEMI;   t->text = (char*)p; t->len = 1; *pp = p+1; return; }

    if(*p == '"') {
        const char *start = ++p;
        while (*p && *p != '"') p++;
        t->type = TOK_STRING;
        t->text = (char*)start;
        t->len  = p - start;
        if(*p == '"') p++;
        *pp = p;
        return;
    }

    /* identifier or keyword */
    const char *start = p;
    while (*p && !isspace(*p) && *p != '{' && *p != '}' && *p != ';') p++;
    t->type = TOK_IDENT;
    t->text = (char*)start;
    t->len  = p - start;
    if(t->len == 7 && memcmp(start, "include", 7) == 0)
        t->type = TOK_INCLUDE;
    *pp = p;
}

static config_pair *pair_new(const char *key, const char *value, config_node *sub)
{
    config_pair *p = calloc(1, sizeof(*p));
    p->key = dupstr(key);
    p->value = value ? dupstr(value) : NULL;
    p->sub_block = sub;
    return p;
}

static config_node *node_new(config_node_type type, const char *name)
{
    config_node *n = calloc(1, sizeof(*n));
    n->type = type;
    n->name = name ? dupstr(name) : NULL;
    return n;
}

static void node_append_pair(config_node *n, config_pair *p)
{
    size_t cnt = 0;
    for (config_pair *it = n->pairs; it; it = it[cnt].sub_block ? NULL : it + 1) cnt++;
    config_pair *new = realloc(n->pairs, sizeof(config_pair) * (cnt + 2));
    if(!new) { perror("realloc"); exit(1); }
    n->pairs = new;
    n->pairs[cnt] = *p;
    n->pairs[cnt+1].key = NULL;   /* sentinel */
    free(p);
}

static config_node *parse_file(const char *path);

static config_node *parse_block(const char *path, FILE *f, const char **linebuf, size_t *linecap,
                             token *first_tok, config_node_type block_type, const char *block_name)
{
    config_node *node = node_new(block_type, block_name);
    token t = *first_tok;

    while (1) {
        /* expect key */
        if(t.type == TOK_RBRACE) break;
        if(t.type == TOK_EOF) { fprintf(stderr, "%s: unexpected EOF in block\n", path); exit(1); }

        if(t.type != TOK_IDENT) {
            fprintf(stderr, "%s: expected identifier, got %d\n", path, t.type); exit(1);
        }
        char key[256];
        size_t keylen = t.len < sizeof(key)-1 ? t.len : sizeof(key)-1;
        memcpy(key, t.text, keylen); key[keylen] = '\0';
        next_token(&t.text + t.len, &t);   /* consume key */

        /* value or sub-block */
        if(t.type == TOK_LBRACE) {
            /* sub-block: key { ... } */
            config_node *sub = parse_block(path, f, linebuf, linecap, &t, CONFIG_UNKNOWN, NULL);
            node_append_pair(node, pair_new(key, NULL, sub));
            continue;
        }

        /* expect value (string or ident) */
        if(t.type != TOK_STRING && t.type != TOK_IDENT) {
            fprintf(stderr, "%s: expected value after '%s'\n", path, key); exit(1);
        }
        char value[1024];
        size_t vallen = t.len < sizeof(value)-1 ? t.len : sizeof(value)-1;
        memcpy(value, t.text, vallen); value[vallen] = '\0';
        if(t.type == TOK_STRING) {
            /* strip quotes */
            if(value[vallen-1] == '"') value[--vallen] = '\0';
            if(value[0] == '"') { memmove(value, value+1, vallen); vallen--; }
        }
        next_token(&t.text + t.len, &t);
        if(t.type != TOK_SEMI) { fprintf(stderr, "%s: missing ';' after value\n", path); exit(1); }
        node_append_pair(node, pair_new(key, value, NULL));
        next_token(&t.text + t.len, &t);   /* consume ';' */
    }
    return node;
}

/* ---------- top-level parser ---------- */
static config_node *parse_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if(!f) { perror(path); return NULL; }

    config_node head = {0}, *tail = &head;
    char *line = NULL;
    size_t linecap = 0;
    size_t linelen;

    while ((linelen = getline(&line, &linecap, f)) != -1) 
    {
        char *p = line;
        trim(p);
        if(!*p || *p == '#') 
        {
            continue;   
        }

        token t;
        next_token((const char**)&p, &t);

        if(t.type == TOK_INCLUDE) 
        {
            next_token(&p, &t);
            if(t.type != TOK_STRING) 
            { 
                fprintf(stderr, "%s: include needs quoted path\n", path); 
                exit(1); 
            }

            char incpath[512];
            size_t plen = t.len < sizeof(incpath)-1 ? t.len : sizeof(incpath)-1;
            memcpy(incpath, t.text, plen); incpath[plen] = '\0';
           
            if(incpath[plen-1] == '"') 
            {
                incpath[--plen] = '\0';
            }
            if(incpath[0] == '"') 
            { 
                memmove(incpath, incpath+1, plen); plen--; 
            }

            config_node *inc = parse_file(incpath);

            if(inc) {
                while (inc) {
                    config_node *next = inc->next;
                    inc->next = NULL;
                    tail->next = inc;
                    tail = inc;
                    inc = next;
                }
            }

            next_token(&p, &t);
            if(t.type != TOK_SEMI) 
            { 
                fprintf(stderr, "%s: missing ';' after include\n", path); 
                exit(1); 
            }
            continue;
        }

        if(t.type != TOK_IDENT) 
        {
            continue;
        }

        char block_name[256] = {0};
        config_node_type btype = CONFIG_UNKNOWN;
        if(strcmp(t.text, "options") == 0) {
            btype = CONFIG_OPTIONS;
            next_token(&p, &t);
        } else if(strcmp(t.text, "zone") == 0) {
            btype = CONFIG_ZONE;
            next_token(&p, &t);
            if(t.type != TOK_STRING) { fprintf(stderr, "%s: zone needs quoted name\n", path); exit(1); }
            size_t nlen = t.len < sizeof(block_name)-1 ? t.len : sizeof(block_name)-1;
            memcpy(block_name, t.text, nlen); block_name[nlen] = '\0';
            if(block_name[nlen-1] == '"') block_name[--nlen] = '\0';
            if(block_name[0] == '"') { memmove(block_name, block_name+1, nlen); }
            next_token(&p, &t);
        } else {
            fprintf(stderr, "%s: unknown top-level keyword '%.*s'\n", path, (int)t.len, t.text);
            continue;
        }

        if(t.type != TOK_LBRACE) { fprintf(stderr, "%s: expected '{' after %s\n", path, btype==CONFIG_OPTIONS?"options":"zone"); exit(1); }

        config_node *block = parse_block(path, f, &line, &linecap, &t, btype, block_name[0]?block_name:NULL);
        tail->next = block;
        tail = block;
    }
    free(line);
    fclose(f);
    return head.next;
}

config_node *config_parse_file(const char *path)
{
    return parse_file(path);
}

/* ---------- free ---------- */
static void free_pair(config_pair *p)
{
    if(!p) return;
    for (size_t i = 0; p[i].key; ++i) {
        free(p[i].key);
        free(p[i].value);
        config_free(p[i].sub_block);
    }
    free(p);
}

void config_free(config_node *n)
{
    while (n) {
        config_node *next = n->next;
        free(n->name);
        free_pair(n->pairs);
        free(n);
        n = next;
    }
}

static void dump_pair(const config_pair *p, int indent)
{
    for (size_t i = 0; p[i].key; ++i) {
        for (int k = 0; k < indent; ++k) printf("  ");
        printf("%s ", p[i].key);
        if(p[i].value) printf("\"%s\";\n", p[i].value);
        else {
            printf("{\n");
            config_dump(p[i].sub_block);
            for (int k = 0; k < indent; ++k) printf("  ");
            printf("}\n");
        }
    }
}

void config_dump(config_node *root)
{
    for (config_node *n = root; n; n = n->next) {
        if(n->type == CONFIG_OPTIONS) 
        {
            printf("options ");
        }
        else if(n->type == CONFIG_ZONE) 
        {
            printf("zone \"%s\" ", n->name);
        }
        else if(n->type == CONFIG_INCLUDE) 
        { 
            printf("include \"%s\";\n", n->name); 
            continue; 
        }
        else { 
            printf("??? ");
         }

        printf("{\n");
        dump_pair(n->pairs, 1);
        printf("}\n");
    }
}