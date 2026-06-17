/**
 * @file config_util.c
 *
 * Implementation: JSON config file read / write / accessor helpers.
 * Uses cJSON (already a project dependency via libcjson).
 */

#include "config_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/**
 * Read an entire file into a malloc'd string.  Returns NULL on error.
 */
static char * read_file(const char * filepath)
{
    FILE * f = fopen(filepath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    char * buf = (char *)malloc((size_t)(sz + 1));
    if (!buf) { fclose(f); return NULL; }

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (n != (size_t)sz) {
        free(buf);
        return NULL;
    }
    buf[sz] = '\0';
    return buf;
}

/**
 * Create parent directories for `filepath` (like `mkdir -p`).
 */
static bool mkdir_p(const char * filepath)
{
    char tmp[1024];
    size_t len = strlen(filepath);
    if (len >= sizeof(tmp)) return false;

    strncpy(tmp, filepath, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    /* Walk forward, creating each directory component */
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (tmp[0] && access(tmp, F_OK) != 0) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    fprintf(stderr, "[config] mkdir '%s': %s\n",
                            tmp, strerror(errno));
                    return false;
                }
            }
            tmp[i] = '/';
        }
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

char * config_find(const char * primary_path, const char * fallback_path)
{
    if (primary_path && access(primary_path, R_OK) == 0) {
        return strdup(primary_path);
    }
    if (fallback_path && access(fallback_path, R_OK) == 0) {
        return strdup(fallback_path);
    }
    return NULL;
}

cJSON * config_json_read(const char * filepath)
{
    if (!filepath) return NULL;

    char * raw = read_file(filepath);
    if (!raw) {
        /* File not found is normal — caller handles NULL gracefully */
        return NULL;
    }

    /* Skip UTF-8 BOM if present */
    const char * text = raw;
    if ((unsigned char)text[0] == 0xEF &&
        (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF) {
        text += 3;
    }

    cJSON * json = cJSON_Parse(text);
    if (!json) {
        const char * err = cJSON_GetErrorPtr();
        fprintf(stderr, "[config] JSON parse error in '%s': %s\n",
                filepath, err ? err : "unknown");
    }

    free(raw);
    return json;
}

bool config_json_write(const char * filepath, const cJSON * json)
{
    if (!filepath || !json) return false;

    /* Ensure the parent directory exists */
    if (!mkdir_p(filepath)) return false;

    char * text = cJSON_Print(json);
    if (!text) {
        fprintf(stderr, "[config] cJSON_Print failed for '%s'\n", filepath);
        return false;
    }

    FILE * f = fopen(filepath, "wb");
    if (!f) {
        fprintf(stderr, "[config] cannot open '%s' for writing: %s\n",
                filepath, strerror(errno));
        free(text);
        return false;
    }

    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, f);
    fclose(f);
    free(text);

    if (written != len) {
        fprintf(stderr, "[config] short write to '%s' (%zu/%zu)\n",
                filepath, written, len);
        return false;
    }

    return true;
}

/* ---- Typed accessors ---- */

const char * config_get_str(const cJSON * json, const char * key,
                            const char * default_val)
{
    if (!json) return default_val;
    cJSON * item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsString(item)) return default_val;
    return item->valuestring;
}

bool config_get_bool(const cJSON * json, const char * key, bool default_val)
{
    if (!json) return default_val;
    cJSON * item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item) return default_val;
    if (cJSON_IsBool(item))  return cJSON_IsTrue(item);
    if (cJSON_IsNumber(item)) return item->valueint != 0;
    return default_val;
}

int config_get_int(const cJSON * json, const char * key, int default_val)
{
    if (!json) return default_val;
    cJSON * item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return default_val;
    return item->valueint;
}
