/**
 * @file baidu_oauth.h
 *
 * Baidu OAuth 2.0 Device Flow Client
 *
 * Pure C + libcurl + pthread.  Handles the complete QR-code login flow:
 *   1. GET /device/code  →  device_code + qrcode_url + user_code
 *   2. GET qrcode_url    →  PNG image binary
 *   3. Poll GET /token    →  access_token + refresh_token
 *
 * All network I/O runs in a background pthread; callbacks notify the
 * caller of QR-code readiness and login status changes.
 */
#ifndef BAIDU_OAUTH_H
#define BAIDU_OAUTH_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      TYPEDEFS
 *********************/

/**
 * QR-code callback — called once the PNG image has been downloaded.
 * @param png_data   PNG binary (caller must NOT free; ownership transfers)
 * @param png_len    size in bytes
 * @param user_code  human-readable code to show the user
 * @param user_data  opaque pointer passed to baidu_oauth_login_start()
 * @note  Runs on the curl pthread — do NOT call LVGL APIs directly.
 */
typedef void (*baidu_oauth_qr_cb_t)(const uint8_t *png_data, size_t png_len,
                                     const char *user_code, void *user_data);

/** Login status values returned by the status callback */
typedef enum {
    BAIDU_OAUTH_PENDING,      /**< Waiting for user to scan QR code       */
    BAIDU_OAUTH_SUCCESS,      /**< Login succeeded, tokens are valid      */
    BAIDU_OAUTH_DECLINED,     /**< User declined the authorization        */
    BAIDU_OAUTH_EXPIRED,      /**< QR code expired                        */
    BAIDU_OAUTH_ERROR         /**< Network or other error                 */
} baidu_oauth_status_t;

/**
 * Status callback — called when the login state changes.
 * @param status        new state
 * @param access_token  valid only when status == BAIDU_OAUTH_SUCCESS
 * @param refresh_token valid only when status == BAIDU_OAUTH_SUCCESS
 * @param user_data     opaque pointer passed to baidu_oauth_login_start()
 * @note  Runs on the curl pthread — do NOT call LVGL APIs directly.
 */
typedef void (*baidu_oauth_status_cb_t)(baidu_oauth_status_t status,
                                         const char *access_token,
                                         const char *refresh_token,
                                         void *user_data);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Start the login flow asynchronously.
 *
 * Creates a background pthread that:
 *   1. Fetches a device_code + QR-code URL from Baidu
 *   2. Downloads the QR PNG → calls @p qr_cb
 *   3. Polls /token every 3 s → calls @p status_cb on each state change
 *
 * @param qr_cb      called when QR image is ready (on curl thread)
 * @param status_cb  called on every login-state transition (on curl thread)
 * @param user_data  passed through to both callbacks
 * @return 0 on success, -1 if a login flow is already running
 */
int baidu_oauth_login_start(baidu_oauth_qr_cb_t qr_cb,
                             baidu_oauth_status_cb_t status_cb,
                             void *user_data);

/** Cancel a running login flow.  Safe to call from any thread. */
void baidu_oauth_login_cancel(void);

/** Release global curl resources.  Call once at application exit. */
void baidu_oauth_cleanup(void);

/*--------------------------------------------------------------------
 * Token persistence
 *--------------------------------------------------------------------*/

/** Save access_token and refresh_token to persistent storage */
void baidu_oauth_save_token(const char *access_token,
                             const char *refresh_token);

/** Load saved tokens. Returns 0 on success, -1 if no saved token */
int baidu_oauth_load_token(char *access_buf, size_t access_len,
                            char *refresh_buf, size_t refresh_len);

/** Delete saved tokens (logout) */
void baidu_oauth_clear_token(void);

/** Check whether a saved token exists */
int baidu_oauth_has_token(void);

/*--------------------------------------------------------------------
 * Netdisk API
 *--------------------------------------------------------------------*/

/** A single file entry from the netdisk list API */
typedef struct {
    char  *path;       /**< full path */
    char  *name;       /**< filename */
    char  *fs_id;      /**< file system ID (for download) */
    int    is_dir;     /**< 1 = directory, 0 = file */
    size_t size;       /**< file size in bytes */
} baidu_oauth_file_t;

/** Callback for file list results.
 *  @param files  array of file entries (NULL if error)
 *  @param count  number of entries (0 if empty or error)
 *  @param user_data  opaque pointer
 *  @note  Runs on curl thread — do NOT call LVGL APIs directly. */
typedef void (*baidu_oauth_list_cb_t)(const baidu_oauth_file_t *files,
                                       int count, void *user_data);

/** Fetch file list from netdisk directory (async).
 *  @param access_token  valid access token
 *  @param dir           directory path, e.g. "/" or "/photos"
 *  @param cb            called with results (on curl thread)
 *  @param user_data     passed through to callback
 *  @return 0 on success, -1 if a request is already running */
int baidu_oauth_list_files(const char *access_token, const char *dir,
                            baidu_oauth_list_cb_t cb, void *user_data);

/*--------------------------------------------------------------------
 * File download
 *--------------------------------------------------------------------*/

typedef enum {
    BAIDU_DL_PROGRESS,      /**< downloading, msg = filename */
    BAIDU_DL_SUCCESS,       /**< single file done, msg = local path */
    BAIDU_DL_ERROR,         /**< error, msg = description */
    BAIDU_DL_COMPLETE       /**< all files done (dir download only) */
} baidu_dl_status_t;

typedef void (*baidu_oauth_dl_cb_t)(baidu_dl_status_t status,
                                     const char *msg, void *user_data);

/** Download a single file (async) using Baidu fs_id.
 *  Saves to BAIDU_DOWNLOAD_DIR (default /mnt/sdcard/baidu-xkphoto/img/)
 *  and backs up to /mnt/sdcard/baidu-xkphoto/ */
int baidu_oauth_download_file(const char *access_token,
                               const char *fs_id,
                               const char *remote_name,
                               baidu_oauth_dl_cb_t cb, void *user_data);

/** Recursively download a directory (async).
 *  @param remote_dir  remote directory path, e.g. "/photos" */
int baidu_oauth_download_dir(const char *access_token,
                              const char *remote_dir,
                              baidu_oauth_dl_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* BAIDU_OAUTH_H */
