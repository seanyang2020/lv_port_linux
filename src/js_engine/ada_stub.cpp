/**
 * @file ada_stub.cpp — Minimal stub for the Ada URL parser library.
 *
 * Provides empty implementations of all ada_* symbols that txiki.js
 * references from src/url.c.  Our JS demos don't use URL parsing.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern "C" {

#define ada_url_omitted 0xffffffff

typedef struct { const char* d; size_t n; } ada_string;
typedef struct { const char* d; size_t n; } ada_owned_string;
typedef void* ada_url;
typedef void* ada_url_search_params;
typedef void* ada_search_params_entries_iter;
typedef void* ada_search_params_keys_iter;
typedef void* ada_search_params_values_iter;
typedef void* ada_strings;
typedef struct { uint32_t a,b,c,d,e,f,g,h; } ada_url_components;

static ada_owned_string eo(void) { return {NULL,0}; }
static ada_string       es(void) { return {NULL,0}; }

/* ---- URL ---- */
ada_url ada_parse(const char*, size_t)          { return NULL; }
ada_url ada_parse_with_base(const char*, size_t, const char*, size_t) { return NULL; }
bool    ada_can_parse(const char*, size_t)       { return false; }
bool    ada_can_parse_with_base(const char*, size_t, const char*, size_t) { return false; }
void    ada_free(ada_url)                        { }
void    ada_free_owned_string(ada_owned_string)  { }
ada_url ada_copy(ada_url)                        { return NULL; }
bool    ada_is_valid(ada_url)                    { return false; }

ada_owned_string ada_get_origin(ada_url)    { return eo(); }
ada_string       ada_get_href(ada_url)      { return es(); }
ada_string       ada_get_username(ada_url)  { return es(); }
ada_string       ada_get_password(ada_url)  { return es(); }
ada_string       ada_get_port(ada_url)      { return es(); }
ada_string       ada_get_hash(ada_url)      { return es(); }
ada_string       ada_get_host(ada_url)      { return es(); }
ada_string       ada_get_hostname(ada_url)  { return es(); }
ada_string       ada_get_pathname(ada_url)  { return es(); }
ada_string       ada_get_search(ada_url)    { return es(); }
ada_string       ada_get_protocol(ada_url)  { return es(); }
uint8_t          ada_get_host_type(ada_url) { return 0; }
uint8_t          ada_get_scheme_type(ada_url) { return 0; }

bool ada_set_href(ada_url, const char*, size_t)     { return false; }
bool ada_set_host(ada_url, const char*, size_t)     { return false; }
bool ada_set_hostname(ada_url, const char*, size_t) { return false; }
bool ada_set_protocol(ada_url, const char*, size_t) { return false; }
bool ada_set_username(ada_url, const char*, size_t) { return false; }
bool ada_set_password(ada_url, const char*, size_t) { return false; }
bool ada_set_port(ada_url, const char*, size_t)     { return false; }
bool ada_set_pathname(ada_url, const char*, size_t) { return false; }
bool ada_set_search(ada_url, const char*, size_t)   { return false; }
bool ada_set_hash(ada_url, const char*, size_t)     { return false; }
void ada_clear_search(ada_url)                      { }

ada_url_components ada_get_components(ada_url) { return {}; }

/* ---- Search params ---- */
ada_url_search_params ada_parse_search_params(const char*, size_t) { return NULL; }
void ada_free_search_params(ada_url_search_params) { }
void ada_search_params_append(ada_url_search_params, const char*, size_t, const char*, size_t) { }
void ada_search_params_remove(ada_url_search_params, const char*, size_t) { }
void ada_search_params_set(ada_url_search_params, const char*, size_t, const char*, size_t) { }
bool ada_search_params_has(ada_url_search_params, const char*, size_t) { return false; }
bool ada_search_params_has_value(ada_url_search_params, const char*, size_t, const char*, size_t) { return false; }
void ada_search_params_remove_value(ada_url_search_params, const char*, size_t, const char*, size_t) { }
ada_owned_string ada_search_params_get(ada_url_search_params, const char*, size_t) { return eo(); }
ada_owned_string ada_search_params_to_string(ada_url_search_params) { return eo(); }
int  ada_search_params_size(ada_url_search_params) { return 0; }
void ada_search_params_sort(ada_url_search_params) { }
void ada_search_params_reset(ada_url_search_params) { }

/* Search param iterators */
ada_search_params_entries_iter ada_search_params_get_entries(ada_url_search_params) { return NULL; }
bool ada_search_params_entries_iter_has_next(ada_search_params_entries_iter) { return false; }
void ada_search_params_entries_iter_next(ada_search_params_entries_iter) { }
void ada_free_search_params_entries_iter(ada_search_params_entries_iter) { }

ada_search_params_keys_iter ada_search_params_get_keys(ada_url_search_params) { return NULL; }
bool ada_search_params_keys_iter_has_next(ada_search_params_keys_iter) { return false; }
void ada_search_params_keys_iter_next(ada_search_params_keys_iter) { }
void ada_free_search_params_keys_iter(ada_search_params_keys_iter) { }

ada_search_params_values_iter ada_search_params_get_values(ada_url_search_params) { return NULL; }
bool ada_search_params_values_iter_has_next(ada_search_params_values_iter) { return false; }
void ada_search_params_values_iter_next(ada_search_params_values_iter) { }
void ada_free_search_params_values_iter(ada_search_params_values_iter) { }

/* Search param "all" */
ada_strings ada_search_params_get_all(ada_url_search_params, const char*, size_t) { return NULL; }
int  ada_strings_size(ada_strings) { return 0; }
ada_owned_string ada_strings_get(ada_strings, int) { return eo(); }
void ada_free_strings(ada_strings) { }

} /* extern "C" */
