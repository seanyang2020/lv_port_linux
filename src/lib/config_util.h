/**
 * @file config_util.h
 *
 * Minimal JSON config file reader/writer built on cJSON.
 * Designed for the lv_port_linux config system:
 *   - config-entry.json   (tab order, cycle, js_apps_path)
 *   - js-apps/config.json (auto_start_app)
 *
 * All paths are resolved by the caller; this module only handles
 * file I/O and JSON parsing.
 */

#ifndef CONFIG_UTIL_H
#define CONFIG_UTIL_H

#include <stdbool.h>
#include "cjson/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Look up a config file using a priority chain.
 * Tries `primary_path` first, then `fallback_path`.
 * @return  malloc'd path of the first existing file,
 *          or NULL if neither exists.  Caller must free().
 */
char * config_find(const char * primary_path, const char * fallback_path);

/**
 * Read and parse a JSON file.
 * @param filepath  absolute or relative path to the JSON file
 * @return cJSON object on success (caller must cJSON_Delete),
 *         NULL if the file doesn't exist or is malformed.
 */
cJSON * config_json_read(const char * filepath);

/**
 * Serialise a cJSON object and write it to disk.
 * Creates parent directories if they don't exist.
 * @param filepath  destination file path
 * @param json      cJSON object to write
 * @return true on success, false on failure (logged to stderr).
 */
bool config_json_write(const char * filepath, const cJSON * json);

/* ---- Typed accessors with defaults ---- */

const char * config_get_str(const cJSON * json, const char * key,
                            const char * default_val);
bool         config_get_bool(const cJSON * json, const char * key,
                             bool default_val);
int          config_get_int(const cJSON * json, const char * key,
                            int default_val);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_UTIL_H */
