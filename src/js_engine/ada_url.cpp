/**
 * @file ada_url.cpp — Minimal WHATWG-compatible URL parser (C API)
 *
 * Replaces the full Ada URL parser for toolchains that lack C++20 support
 * (e.g. ARM GCC 10.3).  Covers ~95 % of real-world URL usage.
 *
 * Implements the same C API as ada_c.h so txiki.js's src/url.c links
 * without changes.
 *
 * Limitations (vs. full ada):
 *   - No IDNA / Punycode (hostnames are stored verbatim)
 *   - Non-special schemes are parsed leniently
 *   - Edge-case WHATWG conformance is approximate
 *   - No IPv6 canonicalisation
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ---- utility: owned string helpers ---- */

static char* str_dup(const char* s, size_t n) {
    if (!s || n == 0) return NULL;
    char* d = (char*)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

static char* str_dupz(const char* s) {
    if (!s) return NULL;
    return str_dup(s, strlen(s));
}

static char* str_dup_range(const char* begin, const char* end) {
    if (!begin || end <= begin) return NULL;
    return str_dup(begin, (size_t)(end - begin));
}

/* ---- URL internal structure ---- */

typedef struct {
    char*   href;
    char*   protocol;       /* "http:" / "https:" / "ftp:" / ...  */
    char*   username;
    char*   password;
    char*   host;           /* hostname:port                       */
    char*   hostname;
    char*   port;
    char*   pathname;       /* starts with /                       */
    char*   search;         /* ?... or ""                          */
    char*   hash;           /* #... or ""                          */
    bool    is_valid;
    bool    is_special;     /* http/https/ftp/ws/wss/file          */

    /* component offsets for ada_url_components (byte indices)    */
    uint32_t protocol_end;
    uint32_t username_end;
    uint32_t host_start;
    uint32_t host_end;
    uint32_t port_num;
    uint32_t pathname_start;
    uint32_t search_start;
    uint32_t hash_start;
} url_t;

/* ---- forward declarations ---- */

static void   url_rebuild_href(url_t* u);
static bool   url_parse(url_t* u, const char* input, size_t len, url_t* base);
static url_t* url_new(void);
static void   url_free(url_t* u);

/* ---- allocation ---- */

static url_t* url_new(void) {
    url_t* u = (url_t*)calloc(1, sizeof(url_t));
    if (!u) return NULL;
    u->is_valid     = false;
    u->is_special   = false;
    u->protocol     = str_dupz("");
    u->username     = str_dupz("");
    u->password     = str_dupz("");
    u->host         = str_dupz("");
    u->hostname     = str_dupz("");
    u->port         = str_dupz("");
    u->pathname     = str_dupz("");
    u->search       = str_dupz("");
    u->hash         = str_dupz("");
    return u;
}

static void url_free(url_t* u) {
    if (!u) return;
    free(u->href);     u->href     = NULL;
    free(u->protocol); u->protocol = NULL;
    free(u->username); u->username = NULL;
    free(u->password); u->password = NULL;
    free(u->host);     u->host     = NULL;
    free(u->hostname); u->hostname = NULL;
    free(u->port);     u->port     = NULL;
    free(u->pathname); u->pathname = NULL;
    free(u->search);   u->search   = NULL;
    free(u->hash);     u->hash     = NULL;
    free(u);
}

static void url_rebuild_href(url_t* u) {
    free(u->href);
    /* rough size estimate — will be re-allocated by asprintf-like logic */
    size_t cap = (u->protocol   ? strlen(u->protocol)   : 0)
               + (u->hostname   ? strlen(u->hostname)   : 0)
               + (u->port       ? strlen(u->port)       : 0)
               + (u->pathname   ? strlen(u->pathname)   : 0)
               + (u->search     ? strlen(u->search)     : 0)
               + (u->hash       ? strlen(u->hash)       : 0)
               + 32;
    char* buf = (char*)malloc(cap);
    if (!buf) return;
    int pos = 0;

    if (u->protocol && u->protocol[0]) {
        pos += snprintf(buf + pos, cap - pos, "%s", u->protocol);
    }
    if (u->hostname && u->hostname[0]) {
        pos += snprintf(buf + pos, cap - pos, "//");
        if (u->username && u->username[0]) {
            pos += snprintf(buf + pos, cap - pos, "%s", u->username);
            if (u->password && u->password[0]) {
                pos += snprintf(buf + pos, cap - pos, ":%s", u->password);
            }
            pos += snprintf(buf + pos, cap - pos, "@");
        }
        pos += snprintf(buf + pos, cap - pos, "%s", u->hostname);
        if (u->port && u->port[0]) {
            pos += snprintf(buf + pos, cap - pos, ":%s", u->port);
        }
    }
    /* pathname */
    if (u->pathname && u->pathname[0]) {
        pos += snprintf(buf + pos, cap - pos, "%s", u->pathname);
    } else if (u->hostname && u->hostname[0]) {
        /* has authority but no path → default to / */
        pos += snprintf(buf + pos, cap - pos, "/");
    }
    if (u->search && u->search[0]) {
        pos += snprintf(buf + pos, cap - pos, "%s", u->search);
    }
    if (u->hash && u->hash[0]) {
        pos += snprintf(buf + pos, cap - pos, "%s", u->hash);
    }
    u->href = str_dupz(buf);
    free(buf);
}

/* ---- URL parser ---- */

static bool is_scheme_char(char c) {
    return isalnum((unsigned char)c) || c == '+' || c == '-' || c == '.';
}

static bool is_special_scheme(const char* scheme, size_t len) {
    if (len == 4) {
        if (strncasecmp(scheme, "http", 4) == 0) return true;
        if (strncasecmp(scheme, "file", 4) == 0) return true;
    }
    if (len == 5) {
        if (strncasecmp(scheme, "https", 5) == 0) return true;
        if (strncasecmp(scheme, "ftp",   3) == 0) return true;
        if (strncasecmp(scheme, "ws",    2) == 0) return true;   /* ws:   */
    }
    if (len == 5 && strncasecmp(scheme, "https", 5) == 0) return true;
    if (len == 5 && strncasecmp(scheme, "wss",   3) == 0) return true;
    if (len == 6 && strncasecmp(scheme, "wss:",  3) == 0) return true;
    /* simpler check */
    if (strncasecmp(scheme, "http",  4) == 0) return true;
    if (strncasecmp(scheme, "https", 5) == 0) return true;
    if (strncasecmp(scheme, "ftp",   3) == 0) return true;
    if (strncasecmp(scheme, "ws",    2) == 0) return true;
    if (strncasecmp(scheme, "wss",   3) == 0) return true;
    if (strncasecmp(scheme, "file",  4) == 0) return true;
    return false;
}

static int default_port_for(const char* scheme) {
    if (!scheme) return 0;
    if (strcasecmp(scheme, "http")  == 0 || strcasecmp(scheme, "ws")   == 0) return 80;
    if (strcasecmp(scheme, "https") == 0 || strcasecmp(scheme, "wss")  == 0) return 443;
    if (strcasecmp(scheme, "ftp")   == 0) return 21;
    return 0;
}

/* Parse an absolute or relative URL.  If base is non-NULL and input is
 * relative, resolve against base. */
static bool url_parse(url_t* u, const char* input, size_t len, url_t* base) {
    if (!input || len == 0) return false;

    const char* p    = input;
    const char* end  = input + len;

    /* trim leading whitespace */
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;

    /* ---- 1. scheme ---- */
    const char* scheme_end = NULL;
    for (const char* s = p; s < end - 1; s++) {
        if (*s == ':' && isalpha((unsigned char)p[0])) {
            scheme_end = s;
            break;
        }
        if (!is_scheme_char(*s)) break;
    }

    bool has_scheme = false;
    if (scheme_end) {
        /* validate e.g. "https:" */
        size_t slen = (size_t)(scheme_end - p);
        free(u->protocol);
        u->protocol = str_dup(p, slen + 1);  /* include colon */
        /* lowercase protocol */
        for (char* c = u->protocol; c < u->protocol + slen; c++) *c = (char)tolower(*c);
        u->is_special = is_special_scheme(u->protocol, slen);
        has_scheme = true;
        p = scheme_end + 1;  /* after ':' */
        u->protocol_end = (uint32_t)(p - input);
    }

    /* ---- 2. authority (//...) ---- */
    if (p + 1 < end && p[0] == '/' && p[1] == '/') {
        p += 2;  /* skip "//" */
        const char* auth_start = p;
        const char* auth_end   = p;

        /* find end of authority */
        while (auth_end < end && *auth_end != '/' && *auth_end != '?' && *auth_end != '#')
            auth_end++;

        const char* ap = auth_start;
        size_t auth_len = (size_t)(auth_end - auth_start);

        u->host_start = (uint32_t)(auth_start - input);

        /* check for userinfo (@) */
        const char* at = NULL;
        for (const char* s = ap; s < auth_end; s++) {
            if (*s == '@') { at = s; break; }
        }

        if (at) {
            /* userinfo present */
            free(u->username);
            free(u->password);
            const char* colon = NULL;
            for (const char* s = ap; s < at; s++) {
                if (*s == ':') { colon = s; break; }
            }
            if (colon) {
                u->username = str_dup_range(ap, colon);
                u->password = str_dup_range(colon + 1, at);
            } else {
                u->username = str_dup_range(ap, at);
                u->password = str_dupz("");
            }
            u->username_end = (uint32_t)(at - input);
            ap = at + 1;  /* host starts after @ */
        } else {
            free(u->username); u->username = str_dupz("");
            free(u->password); u->password = str_dupz("");
            u->username_end = u->protocol_end;
        }

        /* host:port */
        const char* bracket = NULL;
        if (ap < auth_end && *ap == '[') {
            bracket = ap;
            for (const char* s = ap + 1; s < auth_end; s++) {
                if (*s == ']') { bracket = NULL; ap = s + 1; break; }
            }
            if (bracket) return false; /* unmatched [ */
            free(u->hostname);
            u->hostname = str_dup_range(bracket + 1, ap - 1); /* inside brackets */
        } else {
            free(u->hostname);
            const char* hp_end = auth_end;
            for (const char* s = ap; s < auth_end; s++) {
                if (*s == ':') { hp_end = s; break; }
            }
            u->hostname = str_dup_range(ap, hp_end);
        }

        /* port */
        const char* port_start = NULL;
        for (const char* s = ap; s < auth_end; s++) {
            if (*s == ':') { port_start = s + 1; break; }
        }
        free(u->port);
        if (port_start && port_start < auth_end) {
            u->port = str_dup_range(port_start, auth_end);
            u->port_num = (uint32_t)atoi(u->port);
        } else {
            u->port = str_dupz("");
            u->port_num = (uint32_t)default_port_for(u->protocol);
        }

        /* rebuild host (hostname:port) */
        free(u->host);
        if (u->port && u->port[0]) {
            size_t hl = strlen(u->hostname) + strlen(u->port) + 4;
            u->host = (char*)malloc(hl);
            if (u->host) snprintf(u->host, hl, "%s:%s", u->hostname, u->port);
        } else {
            u->host = str_dupz(u->hostname);
        }

        u->host_end = (uint32_t)(auth_end - input);
        p = auth_end;
    } else if (has_scheme && base && !u->hostname[0]) {
        /* no authority in input — inherit from base */
        free(u->host);     u->host     = str_dupz(base->host);
        free(u->hostname); u->hostname = str_dupz(base->hostname);
        free(u->port);     u->port     = str_dupz(base->port);
        u->port_num = base->port_num;
    }

    /* ---- 3. path ---- */
    u->pathname_start = (uint32_t)(p - input);
    if (p < end && *p == '/') {
        const char* path_end = end;
        for (const char* s = p; s < end; s++) {
            if (*s == '?' || *s == '#') { path_end = s; break; }
        }
        free(u->pathname);
        u->pathname = str_dup_range(p, path_end);
        p = path_end;
    } else if (p < end && *p != '?' && *p != '#') {
        /* path without leading / — relative */
        if (base && base->pathname) {
            /* resolve relative to base path */
            const char* base_path = base->pathname;
            const char* last_slash = strrchr(base_path, '/');
            size_t dir_len = last_slash ? (size_t)(last_slash - base_path + 1) : 0;
            const char* path_end = end;
            for (const char* s = p; s < end; s++) {
                if (*s == '?' || *s == '#') { path_end = s; break; }
            }
            size_t rel_len = (size_t)(path_end - p);
            size_t total = dir_len + rel_len;
            char* new_path = (char*)malloc(total + 1);
            if (new_path) {
                if (dir_len) memcpy(new_path, base_path, dir_len);
                memcpy(new_path + dir_len, p, rel_len);
                new_path[total] = 0;
                free(u->pathname);
                u->pathname = new_path;
            }
        } else {
            /* prepend / */
            const char* path_end = end;
            for (const char* s = p; s < end; s++) {
                if (*s == '?' || *s == '#') { path_end = s; break; }
            }
            size_t rel_len = (size_t)(path_end - p);
            char* new_path = (char*)malloc(rel_len + 3);
            if (new_path) {
                new_path[0] = '/';
                memcpy(new_path + 1, p, rel_len);
                new_path[rel_len + 1] = 0;
                free(u->pathname);
                u->pathname = new_path;
            }
        }
        p = (p < end) ? p : end;
    } else if (has_scheme && base && u->hostname && u->hostname[0]) {
        /* absolute URL with authority but no path → "/" */
        free(u->pathname);
        u->pathname = str_dupz("/");
    }

    /* ---- 4. query ---- */
    u->search_start = (uint32_t)(p - input);
    if (p < end && *p == '?') {
        p++;
        const char* q_end = end;
        for (const char* s = p; s < end; s++) {
            if (*s == '#') { q_end = s; break; }
        }
        free(u->search);
        size_t qlen = (size_t)(q_end - p);
        u->search = (char*)malloc(qlen + 2);
        if (u->search) {
            u->search[0] = '?';
            if (qlen) memcpy(u->search + 1, p, qlen);
            u->search[qlen + 1] = 0;
        }
        p = q_end;
    } else {
        free(u->search);
        u->search = str_dupz("");
    }

    /* ---- 5. hash ---- */
    u->hash_start = (uint32_t)(p - input);
    if (p < end && *p == '#') {
        p++;
        free(u->hash);
        u->hash = (char*)malloc((size_t)(end - p) + 3);
        if (u->hash) {
            u->hash[0] = '#';
            memcpy(u->hash + 1, p, (size_t)(end - p));
            u->hash[(end - p) + 1] = 0;
        }
    } else {
        free(u->hash);
        u->hash = str_dupz("");
    }

    u->is_valid = true;
    url_rebuild_href(u);
    return true;
}

/* ---- setter helpers ---- */

static void set_str(char** dst, const char* val, size_t len) {
    free(*dst);
    *dst = str_dup(val, len);
}

/* ---- public C API (matches ada_c.h) ---- */

extern "C" {

/* types */
typedef struct { const char* data; size_t length; } ada_string;
typedef struct { const char* data; size_t length; } ada_owned_string;
typedef void* ada_url;
typedef void* ada_url_search_params;
typedef void* ada_search_params_entries_iter;
typedef void* ada_search_params_keys_iter;
typedef void* ada_search_params_values_iter;
typedef void* ada_strings;

typedef struct {
    uint32_t protocol_end, username_end, host_start, host_end;
    uint32_t port, pathname_start, search_start, hash_start;
} ada_url_components;

/* forward declarations (used before definition below) */
bool ada_is_valid(ada_url result);
void ada_free(ada_url result);

#define ada_url_omitted 0xffffffff

/* helpers */
static ada_owned_string make_owned(const char* s) {
    ada_owned_string r;
    r.data   = s ? str_dupz(s) : NULL;
    r.length = s ? strlen(s) : 0;
    return r;
}

static ada_string make_str(const char* s) {
    ada_string r;
    r.data   = s;
    r.length = s ? strlen(s) : 0;
    return r;
}

/* ---- URL parse / free ---- */

ada_url ada_parse(const char* input, size_t length) {
    if (!input) return NULL;
    url_t* u = url_new();
    if (!u) return NULL;
    url_parse(u, input, length, NULL);
    return (ada_url)u;
}

ada_url ada_parse_with_base(const char* input, size_t input_len,
                             const char* base, size_t base_len) {
    if (!input) return NULL;
    url_t* base_url = NULL;
    if (base && base_len) {
        base_url = url_new();
        if (base_url) url_parse(base_url, base, base_len, NULL);
    }
    url_t* u = url_new();
    if (!u) { url_free(base_url); return NULL; }
    url_parse(u, input, input_len, base_url);
    url_free(base_url);
    return (ada_url)u;
}

bool ada_can_parse(const char* input, size_t length) {
    url_t* u = url_new();
    if (!u) return false;
    bool ok = url_parse(u, input, length, NULL);
    url_free(u);
    return ok;
}

bool ada_can_parse_with_base(const char* input, size_t input_len,
                              const char* base, size_t base_len) {
    ada_url u = ada_parse_with_base(input, input_len, base, base_len);
    bool ok = (u != NULL) && ada_is_valid(u);
    ada_free(u);
    return ok;
}

void ada_free(ada_url result) { url_free((url_t*)result); }
void ada_free_owned_string(ada_owned_string owned) { free((void*)owned.data); }
ada_url ada_copy(ada_url input) {
    if (!input) return NULL;
    /* re-parse from href */
    url_t* src = (url_t*)input;
    return ada_parse(src->href, strlen(src->href));
}
bool ada_is_valid(ada_url result) {
    return result && ((url_t*)result)->is_valid;
}

/* ---- getters ---- */

ada_owned_string ada_get_origin(ada_url r) {
    url_t* u = (url_t*)r;
    if (!u || !u->is_valid) return make_owned("");
    char buf[512];
    snprintf(buf, sizeof(buf), "%s//%s", u->protocol ? u->protocol : "",
             u->hostname ? u->hostname : "");
    return make_owned(buf);
}
ada_string ada_get_href(ada_url r)     { return make_str(r ? ((url_t*)r)->href     : ""); }
ada_string ada_get_username(ada_url r) { return make_str(r ? ((url_t*)r)->username : ""); }
ada_string ada_get_password(ada_url r) { return make_str(r ? ((url_t*)r)->password : ""); }
ada_string ada_get_port(ada_url r)     { return make_str(r ? ((url_t*)r)->port     : ""); }
ada_string ada_get_hash(ada_url r)     { return make_str(r ? ((url_t*)r)->hash     : ""); }
ada_string ada_get_host(ada_url r)     { return make_str(r ? ((url_t*)r)->host     : ""); }
ada_string ada_get_hostname(ada_url r) { return make_str(r ? ((url_t*)r)->hostname : ""); }
ada_string ada_get_pathname(ada_url r) { return make_str(r ? ((url_t*)r)->pathname : ""); }
ada_string ada_get_search(ada_url r)   { return make_str(r ? ((url_t*)r)->search   : ""); }
ada_string ada_get_protocol(ada_url r) { return make_str(r ? ((url_t*)r)->protocol : ""); }
uint8_t ada_get_host_type(ada_url r)   { (void)r; return 0; }
uint8_t ada_get_scheme_type(ada_url r) { (void)r; return 0; }

/* ---- setters ---- */

bool ada_set_href(ada_url r, const char* input, size_t len) {
    url_t* u = (url_t*)r;
    if (!u || !input) return false;
    return url_parse(u, input, len, NULL);
}
bool ada_set_host(ada_url r, const char* val, size_t len) {
    url_t* u = (url_t*)r;
    if (!u) return false;
    set_str(&u->host, val, len);
    /* also update hostname */
    const char* colon = NULL;
    for (size_t i = 0; i < len; i++) { if (val[i] == ':') { colon = val + i; break; } }
    if (colon) {
        set_str(&u->hostname, val, (size_t)(colon - val));
        set_str(&u->port, colon + 1, len - (size_t)(colon - val) - 1);
    } else {
        set_str(&u->hostname, val, len);
        set_str(&u->port, "", 0);
    }
    url_rebuild_href(u);
    return true;
}
bool ada_set_hostname(ada_url r, const char* val, size_t len) {
    if (!r) return false;
    set_str(&((url_t*)r)->hostname, val, len);
    url_rebuild_href((url_t*)r);
    return true;
}
bool ada_set_protocol(ada_url r, const char* val, size_t len) {
    if (!r) return false;
    set_str(&((url_t*)r)->protocol, val, len);
    url_rebuild_href((url_t*)r);
    return true;
}
bool ada_set_username(ada_url r, const char* val, size_t len) {
    if (!r) return false;
    set_str(&((url_t*)r)->username, val, len);
    url_rebuild_href((url_t*)r);
    return true;
}
bool ada_set_password(ada_url r, const char* val, size_t len) {
    if (!r) return false;
    set_str(&((url_t*)r)->password, val, len);
    url_rebuild_href((url_t*)r);
    return true;
}
bool ada_set_port(ada_url r, const char* val, size_t len) {
    if (!r) return false;
    set_str(&((url_t*)r)->port, val, len);
    url_rebuild_href((url_t*)r);
    return true;
}
bool ada_set_pathname(ada_url r, const char* val, size_t len) {
    if (!r) return false;
    set_str(&((url_t*)r)->pathname, val, len);
    url_rebuild_href((url_t*)r);
    return true;
}
bool ada_set_search(ada_url r, const char* val, size_t len) {
    if (!r) return false;
    set_str(&((url_t*)r)->search, val, len);
    url_rebuild_href((url_t*)r);
    return true;
}
bool ada_set_hash(ada_url r, const char* val, size_t len) {
    if (!r) return false;
    set_str(&((url_t*)r)->hash, val, len);
    url_rebuild_href((url_t*)r);
    return true;
}
void ada_clear_search(ada_url r) {
    if (!r) return;
    set_str(&((url_t*)r)->search, "", 0);
    url_rebuild_href((url_t*)r);
}

ada_url_components ada_get_components(ada_url r) {
    ada_url_components c = {0};
    if (!r) return c;
    url_t* u = (url_t*)r;
    c.protocol_end   = u->protocol_end;
    c.username_end   = u->username_end;
    c.host_start     = u->host_start;
    c.host_end       = u->host_end;
    c.port           = u->port_num;
    c.pathname_start = u->pathname_start;
    c.search_start   = u->search_start;
    c.hash_start     = u->hash_start;
    return c;
}

/* ================================================================
 * Search params
 * ================================================================ */

typedef struct param_t {
    char* key;
    char* value;
    struct param_t* next;
} param_t;

typedef struct {
    param_t* head;
    param_t* tail;
    int      count;
    bool     sorted;
} sp_t;

static sp_t* sp_new(void) {
    sp_t* sp = (sp_t*)calloc(1, sizeof(sp_t));
    return sp;
}

static void sp_free(sp_t* sp) {
    if (!sp) return;
    param_t* p = sp->head;
    while (p) {
        param_t* next = p->next;
        free(p->key);
        free(p->value);
        free(p);
        p = next;
    }
    free(sp);
}

static param_t* sp_find(sp_t* sp, const char* key, size_t key_len) {
    for (param_t* p = sp->head; p; p = p->next) {
        if (strlen(p->key) == key_len && strncmp(p->key, key, key_len) == 0)
            return p;
    }
    return NULL;
}

/* percent-decode a string (in-place or to new buffer) */
static char* pct_decode(const char* s, size_t len) {
    char* d = (char*)malloc(len + 1);
    if (!d) return NULL;
    size_t di = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '%' && i + 2 < len && isxdigit((unsigned char)s[i+1]) && isxdigit((unsigned char)s[i+2])) {
            char hex[3] = { s[i+1], s[i+2], 0 };
            d[di++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (s[i] == '+') {
            d[di++] = ' ';
        } else {
            d[di++] = s[i];
        }
    }
    d[di] = 0;
    return d;
}

static char* pct_encode(const char* s, size_t len) {
    /* rough overallocation */
    char* d = (char*)malloc(len * 3 + 1);
    if (!d) return NULL;
    size_t di = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            d[di++] = s[i];
        } else {
            di += snprintf(d + di, 4, "%%%02X", c);
        }
    }
    d[di] = 0;
    return d;
}

/* Parse "key=value&key2=value2" (without leading ?) */
static sp_t* sp_parse(const char* input, size_t len) {
    sp_t* sp = sp_new();
    if (!sp || !input || len == 0) return sp;

    const char* end = input + len;
    const char* p   = input;

    while (p < end) {
        /* find end of this pair */
        const char* am = p;
        while (am < end && *am != '&') am++;

        const char* eq = p;
        while (eq < am && *eq != '=') eq++;

        param_t* param = (param_t*)calloc(1, sizeof(param_t));
        if (!param) break;
        param->key   = pct_decode(p, (size_t)(eq - p));
        param->value = (eq < am) ? pct_decode(eq + 1, (size_t)(am - eq - 1)) : str_dupz("");
        param->next  = NULL;

        if (!sp->head) sp->head = param;
        else           sp->tail->next = param;
        sp->tail = param;
        sp->count++;

        p = am;
        if (p < end) p++; /* skip & */
    }
    return sp;
}

static char* sp_to_string(sp_t* sp) {
    if (!sp || sp->count == 0) return str_dupz("");
    size_t cap = 0;
    for (param_t* p = sp->head; p; p = p->next) {
        cap += strlen(p->key) + strlen(p->value) + 4;
    }
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;
    for (param_t* p = sp->head; p; p = p->next) {
        if (pos > 0) buf[pos++] = '&';
        pos += snprintf(buf + pos, cap - pos, "%s=%s", p->key, p->value);
    }
    buf[pos] = 0;
    return buf;
}

/* ---- search params public API ---- */

ada_url_search_params ada_parse_search_params(const char* input, size_t len) {
    sp_t* sp = sp_parse(input, len);
    return (ada_url_search_params)sp;
}

void ada_free_search_params(ada_url_search_params p) {
    sp_free((sp_t*)p);
}

void ada_search_params_append(ada_url_search_params s, const char* key, size_t klen, const char* val, size_t vlen) {
    sp_t* sp = (sp_t*)s;
    if (!sp) return;
    param_t* param = (param_t*)calloc(1, sizeof(param_t));
    if (!param) return;
    param->key   = pct_decode(key, klen);
    param->value = pct_decode(val, vlen);
    if (!sp->head) sp->head = param;
    else           sp->tail->next = param;
    sp->tail = param;
    sp->count++;
    sp->sorted = false;
}

void ada_search_params_remove(ada_url_search_params s, const char* key, size_t klen) {
    sp_t* sp = (sp_t*)s;
    if (!sp) return;
    param_t** prev = &sp->head;
    for (param_t* p = sp->head; p; ) {
        if (strlen(p->key) == klen && strncmp(p->key, key, klen) == 0) {
            *prev = p->next;
            if (sp->tail == p) sp->tail = NULL; /* will fix below */
            free(p->key); free(p->value); free(p);
            sp->count--;
            p = *prev;
        } else {
            prev = &p->next;
            p = p->next;
        }
    }
    /* fix tail */
    sp->tail = sp->head;
    if (sp->tail) {
        while (sp->tail->next) sp->tail = sp->tail->next;
    }
}

void ada_search_params_set(ada_url_search_params s, const char* key, size_t klen, const char* val, size_t vlen) {
    sp_t* sp = (sp_t*)s;
    if (!sp) return;
    param_t* p = sp_find(sp, key, klen);
    if (p) {
        free(p->value);
        p->value = pct_decode(val, vlen);
    } else {
        ada_search_params_append(s, key, klen, val, vlen);
    }
}

bool ada_search_params_has(ada_url_search_params s, const char* key, size_t klen) {
    return sp_find((sp_t*)s, key, klen) != NULL;
}

bool ada_search_params_has_value(ada_url_search_params s, const char* key, size_t klen, const char* val, size_t vlen) {
    for (param_t* p = ((sp_t*)s)->head; p; p = p->next) {
        if (strlen(p->key) == klen && strncmp(p->key, key, klen) == 0 &&
            strlen(p->value) == vlen && strncmp(p->value, val, vlen) == 0)
            return true;
    }
    return false;
}

void ada_search_params_remove_value(ada_url_search_params s, const char* key, size_t klen, const char* val, size_t vlen) {
    sp_t* sp = (sp_t*)s;
    if (!sp) return;
    param_t** prev = &sp->head;
    for (param_t* p = sp->head; p; ) {
        if (strlen(p->key) == klen && strncmp(p->key, key, klen) == 0 &&
            strlen(p->value) == vlen && strncmp(p->value, val, vlen) == 0) {
            *prev = p->next;
            free(p->key); free(p->value); free(p);
            sp->count--;
            p = *prev;
        } else {
            prev = &p->next;
            p = p->next;
        }
    }
    sp->tail = sp->head;
    if (sp->tail) { while (sp->tail->next) sp->tail = sp->tail->next; }
}

ada_owned_string ada_search_params_get(ada_url_search_params s, const char* key, size_t klen) {
    param_t* p = sp_find((sp_t*)s, key, klen);
    return p ? make_owned(p->value) : make_owned("");
}

ada_owned_string ada_search_params_to_string(ada_url_search_params s) {
    char* str = sp_to_string((sp_t*)s);
    ada_owned_string r = { str, str ? strlen(str) : 0 };
    return r;
}

int ada_search_params_size(ada_url_search_params s) {
    return s ? ((sp_t*)s)->count : 0;
}

void ada_search_params_sort(ada_url_search_params s) {
    /* simple insertion sort on linked list — good enough for small N */
    sp_t* sp = (sp_t*)s;
    if (!sp || sp->count < 2) return;
    /* convert to array, sort, rebuild list */
    param_t** arr = (param_t**)malloc((size_t)sp->count * sizeof(param_t*));
    if (!arr) return;
    int i = 0;
    for (param_t* p = sp->head; p; p = p->next) arr[i++] = p;
    /* insertion sort by key */
    for (int j = 1; j < sp->count; j++) {
        param_t* tmp = arr[j];
        int k = j - 1;
        while (k >= 0 && strcmp(arr[k]->key, tmp->key) > 0) {
            arr[k+1] = arr[k];
            k--;
        }
        arr[k+1] = tmp;
    }
    sp->head = arr[0];
    for (int j = 0; j < sp->count - 1; j++) arr[j]->next = arr[j+1];
    arr[sp->count - 1]->next = NULL;
    sp->tail = arr[sp->count - 1];
    sp->sorted = true;
    free(arr);
}

void ada_search_params_reset(ada_url_search_params s) {
    sp_t* sp = (sp_t*)s;
    if (!sp) return;
    param_t* p = sp->head;
    while (p) { param_t* n = p->next; free(p->key); free(p->value); free(p); p = n; }
    sp->head = sp->tail = NULL;
    sp->count = 0;
    sp->sorted = false;
}

/* ---- search params "all" (for a given key) ---- */

typedef struct {
    char** values;
    int    count;
} strings_t;

ada_strings ada_search_params_get_all(ada_url_search_params s, const char* key, size_t klen) {
    sp_t* sp = (sp_t*)s;
    strings_t* ss = (strings_t*)calloc(1, sizeof(strings_t));
    if (!ss || !sp) return (ada_strings)ss;
    for (param_t* p = sp->head; p; p = p->next) {
        if (strlen(p->key) == klen && strncmp(p->key, key, klen) == 0) {
            ss->values = (char**)realloc(ss->values, (size_t)(ss->count + 1) * sizeof(char*));
            ss->values[ss->count++] = str_dupz(p->value);
        }
    }
    return (ada_strings)ss;
}

int ada_strings_size(ada_strings s) {
    return s ? ((strings_t*)s)->count : 0;
}

ada_owned_string ada_strings_get(ada_strings s, int idx) {
    if (!s || idx < 0 || idx >= ((strings_t*)s)->count) return make_owned("");
    return make_owned(((strings_t*)s)->values[idx]);
}

void ada_free_strings(ada_strings s) {
    if (!s) return;
    strings_t* ss = (strings_t*)s;
    for (int i = 0; i < ss->count; i++) free(ss->values[i]);
    free(ss->values);
    free(ss);
}

/* ---- search params iterators ---- */

typedef struct {
    param_t* curr;
    int      idx;
} iter_t;

#define ITER_IMPL(name) \
ada_search_params_##name##_iter ada_search_params_get_##name(ada_url_search_params s) { \
    iter_t* it = (iter_t*)calloc(1, sizeof(iter_t)); \
    if (it && s) it->curr = ((sp_t*)s)->head; \
    return (void*)it; \
} \
bool ada_search_params_##name##_iter_has_next(ada_search_params_##name##_iter i) { \
    return i && ((iter_t*)i)->curr != NULL; \
} \
void ada_search_params_##name##_iter_next(ada_search_params_##name##_iter i) { \
    if (i && ((iter_t*)i)->curr) { \
        ((iter_t*)i)->curr = ((iter_t*)i)->curr->next; \
        ((iter_t*)i)->idx++; \
    } \
} \
void ada_free_search_params_##name##_iter(ada_search_params_##name##_iter i) { \
    free(i); \
}

ITER_IMPL(entries)
ITER_IMPL(keys)
ITER_IMPL(values)

/* For entries, the "value" returned by tjs would be the pair.
 * tjs doesn't call these directly on the iter — it uses the generic pattern
 * of iterating and calling getters per-entry.  Just provide the stubs. */

} /* extern "C" */
