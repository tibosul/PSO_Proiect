#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "../include/dns_config.h"

/* -------------------- helpers -------------------- */

static void trim(char *s)
{
    if (!s) return;
    char *start = s;
    char *end   = s + strlen(s);

    /* trim leading */
    while (*start && isspace((unsigned char)*start)) start++;

    /* empty line */
    if (*start == '\0') { *s = '\0'; return; }

    /* trim trailing */
    end--; /* last char */
    while (end >= start && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    if (start != s) {
        /* memmove overlap-safe */
        size_t n = strlen(start) + 1;
        memmove(s, start, n);
    }
}

static char *dupstr(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* -------------------- tokenizer -------------------- */

typedef enum {
    TOK_EOF,
    TOK_IDENT,
    TOK_STRING,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMI,
    TOK_INCLUDE
} token_type;

typedef struct {
    token_type type;
    const char *text; /* not null-terminated */
    size_t      len;
} token;

/* Compare IDENT token with a C string safely */
static int tok_eq_ident(const token* t, const char* s) {
    if (!t || t->type != TOK_IDENT || !s) return 0;
    size_t n = strlen(s);
    return (t->len == n) && (memcmp(t->text, s, n) == 0);
}

/* Advance to next token from *pp.
   - Skips whitespace
   - Treats '#' as comment-to-end-of-line (returns TOK_EOF for this line)
   - Strings are returned WITHOUT quotes in t->text/t->len
*/
static void next_token(const char **pp, token *t)
{
    const char *p = *pp;

    /* skip spaces */
    while (*p && isspace((unsigned char)*p)) p++;

    /* inline comments starting with '#' until EOL */
    if (*p == '#') {
        while (*p) p++; /* jump to end of line buffer */
        t->type = TOK_EOF;
        t->text = p;
        t->len  = 0;
        *pp     = p;
        return;
    }

    if (*p == '\0') {
        t->type = TOK_EOF; t->text = p; t->len = 0; *pp = p; return;
    }

    switch (*p) {
        case '{':
            t->type = TOK_LBRACE; t->text = p; t->len = 1; *pp = p + 1; return;
        case '}':
            t->type = TOK_RBRACE; t->text = p; t->len = 1; *pp = p + 1; return;
        case ';':
            t->type = TOK_SEMI;   t->text = p; t->len = 1; *pp = p + 1; return;
        case '"': {
            /* quoted string (no escapes supported in this subset) */
            const char *start = ++p;
            while (*p && *p != '"') p++;
            t->type = TOK_STRING;
            t->text = start;
            t->len  = (size_t)(p - start);
            if (*p == '"') p++;
            *pp = p;
            return;
        }
        default: {
            const char *start = p;
            while (*p && !isspace((unsigned char)*p) && *p != '{' && *p != '}' && *p != ';' && *p != '#') {
                p++;
            }
            t->type = TOK_IDENT;
            t->text = start;
            t->len  = (size_t)(p - start);
            if (t->len == 7 && memcmp(start, "include", 7) == 0) {
                t->type = TOK_INCLUDE;
            }
            *pp = p;
            return;
        }
    }
}

/* -------------------- AST builders -------------------- */

static config_pair *pair_new(const char *key, const char *value, config_node *sub)
{
    config_pair *p = (config_pair*)calloc(1, sizeof(*p));
    if (!p) { perror("calloc pair_new"); exit(1); }
    p->key = dupstr(key);
    p->value = value ? dupstr(value) : NULL;
    p->sub_block = sub;
    return p;
}

static config_node *new_node(config_node_type type, const char *name)
{
    config_node *n = (config_node*)calloc(1, sizeof(*n));
    if (!n) { perror("calloc new_node"); exit(1); }
    n->type = type;
    n->name = name ? dupstr(name) : NULL;
    return n;
}

static void node_append_pair(config_node *n, config_pair *p)
{
    size_t cnt = 0;
    if (n->pairs) {
        while (n->pairs[cnt].key != NULL) cnt++;
    }
    config_pair *arr = (config_pair*)realloc(n->pairs, sizeof(config_pair) * (cnt + 2));
    if (!arr) { perror("realloc node_append_pair"); exit(1); }
    n->pairs = arr;
    n->pairs[cnt] = *p;               /* struct copy */
    n->pairs[cnt + 1].key = NULL;     /* sentinel */
    free(p);
}

/* forward decl */
static config_node *parse_file(const char *path);

/* -------------------- block parser -------------------- */

static config_node *parse_block(const char *path, FILE *f, char **linebuf, size_t *linecap,
                                const char *initial_line, const char **initial_pos,
                                config_node_type block_type, const char *block_name)
{
    (void)initial_line;
    config_node *node = new_node(block_type, block_name);

    char   *line           = *linebuf;
    size_t  linecap_local  = *linecap;
    const char *curpos     = *initial_pos;

    for (;;) {
        if (*curpos == '\0') {
            ssize_t linelen = getline(&line, &linecap_local, f);
            if (linelen == -1) {
                fprintf(stderr, "%s: unexpected EOF in block\n", path);
                exit(1);
            }
            trim(line);
            if (*line == '\0' || *line == '#') { curpos = line; continue; }
            curpos = line;
        }

        token t;
        next_token(&curpos, &t);

        /* allow end-of-line (# comment) to trigger next getline */
        if (t.type == TOK_EOF) { curpos = ""; continue; }

        if (t.type == TOK_RBRACE) {
            /* end of this block */
            break;
        }

        /* -------- LIST ITEM MODE --------
           Allow blocks that contain only values like: { "val1"; "val2"; }
           e.g., forwarders { "8.8.8.8"; "1.1.1.1"; };
        */
        if (t.type == TOK_STRING) {
            char value[1024];
            size_t vallen = t.len < sizeof(value)-1 ? t.len : sizeof(value)-1;
            memcpy(value, t.text, vallen);
            value[vallen] = '\0';

            /* require trailing ';' */
            token t2; next_token(&curpos, &t2);
            if (t2.type == TOK_EOF) { curpos = ""; continue; }
            if (t2.type != TOK_SEMI) {
                fprintf(stderr, "%s: missing ';' after list item \"%s\"\n", path, value);
                exit(1);
            }

            /* store as an item (key marker "__item") */
            node_append_pair(node, pair_new("__item", value, NULL));
            continue;
        }

        /* Also allow bare IDENT items in list blocks, e.g., { yes; no; } */
        if (t.type == TOK_IDENT) {
            /* Peek next token to decide if this IDENT starts a key/value
               or it's a bare list item (IDENT ';') */
            const char *save = curpos;
            token tpeek; next_token(&curpos, &tpeek);

            if (tpeek.type == TOK_SEMI) {
                /* Treat IDENT as a list item value */
                char value[256];
                size_t vlen = t.len < sizeof(value)-1 ? t.len : sizeof(value)-1;
                memcpy(value, t.text, vlen);
                value[vlen] = '\0';

                node_append_pair(node, pair_new("__item", value, NULL));
                continue;
            }

            /* Not a bare item → restore and proceed as key/value or sub-block */
            curpos = save;

            /* copy key into a null-terminated buffer */
            char key[256];
            size_t keylen = t.len < sizeof(key)-1 ? t.len : sizeof(key)-1;
            memcpy(key, t.text, keylen);
            key[keylen] = '\0';

            /* fetch either a value or a sub-block */
            next_token(&curpos, &t); /* after key */

            if (t.type == TOK_LBRACE) {
                /* Nested sub-block: parse it and preserve its name */
                const char *sub_pos = curpos;
                config_node *sub = parse_block(path, f, &line, &linecap_local, line, &sub_pos,
                                               CONFIG_UNKNOWN, NULL);
                if (sub && !sub->name) sub->name = dupstr(key);
                node_append_pair(node, pair_new(key, NULL, sub));
                curpos = sub_pos;

                /* optional semicolon after sub-block */
                const char *sav2 = curpos;
                token t2; next_token(&curpos, &t2);
                if (t2.type != TOK_SEMI) curpos = sav2;
                continue;
            }

            if (t.type != TOK_STRING && t.type != TOK_IDENT) {
                fprintf(stderr, "%s: expected value after '%s'\n", path, key);
                exit(1);
            }

            /* copy value exactly (strings already unquoted by lexer) */
            char value[1024];
            size_t vallen = t.len < sizeof(value)-1 ? t.len : sizeof(value)-1;
            memcpy(value, t.text, vallen);
            value[vallen] = '\0';

            /* require trailing ';' (accept newline-then-';' too) */
            next_token(&curpos, &t);
            if (t.type == TOK_EOF) {
                do {
                    ssize_t linelen = getline(&line, &linecap_local, f);
                    if (linelen == -1) {
                        fprintf(stderr, "%s: missing ';' after value for key '%s' (EOF)\n", path, key);
                        exit(1);
                    }
                    trim(line);
                } while (*line == '\0' || *line == '#');
                const char *p2 = line;
                next_token(&p2, &t);
                curpos = p2;
            }
            if (t.type != TOK_SEMI) {
                fprintf(stderr, "%s: missing ';' after value for key '%s'\n", path, key);
                exit(1);
            }

            node_append_pair(node, pair_new(key, value, NULL));
            continue;
        }

        /* If we reach here, token type is unexpected inside a block */
        fprintf(stderr, "%s: expected identifier or list item inside block, got token type %d\n",
                path, (int)t.type);
        exit(1);
    }

    *linebuf     = line;
    *linecap     = linecap_local;
    *initial_pos = curpos;
    return node;
}


/* -------------------- top-level parser -------------------- */

static config_node *parse_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return NULL;
    }

    config_node head = {0};
    config_node *tail = &head;

    char  *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, f)) != -1)
    {
        (void)linelen;
        trim(line);
        if (*line == '\0' || *line == '#') continue;

        const char *curpos = line;
        token t;
        next_token(&curpos, &t);

        /* allow end-of-line comments */
        if (t.type == TOK_EOF) continue;

        /* include "path"; */
        if (t.type == TOK_INCLUDE) {
            next_token(&curpos, &t);
            if (t.type != TOK_STRING) {
                fprintf(stderr, "%s: include needs quoted path\n", path);
                exit(1);
            }
            /* t.text..t.len is the path (already without quotes) */
            char incpath[512];
            size_t plen = t.len < sizeof(incpath)-1 ? t.len : sizeof(incpath)-1;
            memcpy(incpath, t.text, plen);
            incpath[plen] = '\0';

            /* require trailing semicolon */
            next_token(&curpos, &t);
            if (t.type != TOK_SEMI) {
                fprintf(stderr, "%s: missing ';' after include\n", path);
                exit(1);
            }

            config_node *inc = parse_file(incpath);
            while (inc) {
                config_node *next = inc->next;
                inc->next = NULL;
                tail->next = inc;
                tail = inc;
                inc = next;
            }
            continue;
        }

        if (t.type != TOK_IDENT) {
            /* ignore unknown garbage line gracefully */
            continue;
        }

        char block_name[256] = {0};
        config_node_type btype = CONFIG_UNKNOWN;

        if (tok_eq_ident(&t, "options")) {
            btype = CONFIG_OPTIONS;
            next_token(&curpos, &t);
        } else if (tok_eq_ident(&t, "zone")) {
            btype = CONFIG_ZONE;
            next_token(&curpos, &t);
            if (t.type != TOK_STRING) {
                fprintf(stderr, "%s: zone needs quoted name\n", path);
                exit(1);
            }
            size_t nlen = t.len < sizeof(block_name)-1 ? t.len : sizeof(block_name)-1;
            memcpy(block_name, t.text, nlen);
            block_name[nlen] = '\0';
            next_token(&curpos, &t);
        } else {
            fprintf(stderr, "%s: unknown top-level keyword '%.*s' (ignored)\n",
                    path, (int)t.len, t.text);
            continue;
        }

        if (t.type != TOK_LBRACE) {
            fprintf(stderr, "%s: expected '{' after %s\n", path,
                    (btype == CONFIG_OPTIONS ? "options" : "zone"));
            exit(1);
        }

        const char *block_start_pos = curpos;

        const char *name_to_pass = block_name[0] ? block_name : NULL;

        config_node *block = parse_block(path, f, &line, &linecap,
                                         line, &block_start_pos, btype, name_to_pass);
        curpos = block_start_pos;

        tail->next = block;
        tail = block;
    }

    free(line);
    fclose(f);
    return head.next;
}

/* -------------------- public API -------------------- */

config_node *parse_config_file(const char *path)
{
    return parse_file(path);
}

static void free_pair(config_pair *p)
{
    if (!p) return;
    for (size_t i = 0; p[i].key != NULL; ++i) {
        free(p[i].key);
        free(p[i].value);
        free_config(p[i].sub_block);
    }
    free(p);
}

void free_config(config_node *n)
{
    while (n) {
        config_node *next = n->next;
        free(n->name);
        free_pair(n->pairs);
        free(n);
        n = next;
    }
}

/* pretty-printer (debug) */

static void dump_pair(const config_pair *p, int indent)
{
    for (size_t i = 0; p[i].key != NULL; ++i) {
        for (int k = 0; k < indent; ++k) printf(" ");
        printf("%s ", p[i].key);
        if (p[i].value) {
            printf("\"%s\";\n", p[i].value);
        } else {
            printf("{\n");
            config_dump(p[i].sub_block);
            for (int k = 0; k < indent; ++k) printf(" ");
            printf("}\n");
        }
    }
}

void config_dump(config_node *root)
{
    for (config_node *n = root; n != NULL; n = n->next) {
        if (n->type == CONFIG_OPTIONS) {
            printf("options ");
        } else if (n->type == CONFIG_ZONE) {
            printf("zone \"%s\" ", n->name ? n->name : "");
        } else if (n->type == CONFIG_INCLUDE) {
            printf("include \"%s\";\n", n->name ? n->name : "");
            continue;
        } else {
            printf("unknown ");
        }
        printf("{\n");
        dump_pair(n->pairs, 1);
        printf("}\n");
    }
}
