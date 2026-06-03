/**
 * @file baidu_oauth.c
 *
 * Baidu OAuth 2.0 Device Flow — implementation.
 *
 * See baidu_oauth.h for the public API and flow description.
 *
 * Configuration
 * -------------
 * Set these environment variables before starting the application:
 *   BAIDU_CLIENT_ID      OAuth App Key    (required)
 *   BAIDU_CLIENT_SECRET  OAuth Secret Key (required)
 *
 * If the variables are not set, the library will fail gracefully
 * and report BAIDU_OAUTH_ERROR via the status callback.
 */
#include "baidu_oauth.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

/*********************
 *      DEFINES
 *********************/

#define DEVICE_CODE_URL  "https://openapi.baidu.com/oauth/2.0/device/code"
#define TOKEN_URL        "https://openapi.baidu.com/oauth/2.0/token"
#define POLL_INTERVAL_S  5   /* Baidu spec: must be >= 5 seconds */

/**********************
 *      TYPEDEFS
 **********************/

/** In-memory buffer for curl writes */
typedef struct {
    uint8_t *data;
    size_t   len;
} mem_buf_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static pthread_t       g_thread;
static volatile int    g_running = 0;
static pthread_mutex_t g_mutex  = PTHREAD_MUTEX_INITIALIZER;
static int             g_curl_inited = 0;

static baidu_oauth_qr_cb_t     g_qr_cb     = NULL;
static baidu_oauth_status_cb_t g_status_cb = NULL;
static void                   *g_user_data = NULL;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void *oauth_thread(void *arg);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int baidu_oauth_login_start(baidu_oauth_qr_cb_t qr_cb,
                             baidu_oauth_status_cb_t status_cb,
                             void *user_data)
{
    pthread_mutex_lock(&g_mutex);
    if (g_running) {
        pthread_mutex_unlock(&g_mutex);
        fprintf(stderr, "baidu_oauth: login flow already running\n");
        return -1;
    }

    g_running    = 1;
    g_qr_cb      = qr_cb;
    g_status_cb  = status_cb;
    g_user_data  = user_data;

    if (!g_curl_inited) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        g_curl_inited = 1;
    }

    int ret = pthread_create(&g_thread, NULL, oauth_thread, NULL);
    if (ret != 0) {
        g_running = 0;
        pthread_mutex_unlock(&g_mutex);
        fprintf(stderr, "baidu_oauth: pthread_create failed (%d)\n", ret);
        return -1;
    }
    pthread_detach(g_thread);

    pthread_mutex_unlock(&g_mutex);
    return 0;
}

void baidu_oauth_login_cancel(void)
{
    pthread_mutex_lock(&g_mutex);
    g_running = 0;
    pthread_mutex_unlock(&g_mutex);
}

void baidu_oauth_cleanup(void)
{
    baidu_oauth_login_cancel();
    if (g_curl_inited) {
        curl_global_cleanup();
        g_curl_inited = 0;
    }
    pthread_mutex_destroy(&g_mutex);
}

/*--------------------------------------------------------------------
 * Token persistence
 *--------------------------------------------------------------------*/

#define TOKEN_DIR        "/data/baidu-xkphot"
#define TOKEN_BACKUP_DIR "/mnt/sdcard/baidu-xkphoto"
#define TOKEN_FILE       "token.json"

static void ensure_dir(const char *dir)
{
    /* best-effort — ignore errors */
    mkdir(dir, 0755);
}

void baidu_oauth_save_token(const char *access, const char *refresh)
{
    if (!access || !refresh) return;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "access_token", access);
    cJSON_AddStringToObject(root, "refresh_token", refresh);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return;

    const char *dirs[] = { TOKEN_DIR, TOKEN_BACKUP_DIR, NULL };
    for (int i = 0; dirs[i]; i++) {
        ensure_dir(dirs[i]);
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", dirs[i], TOKEN_FILE);
        FILE *fp = fopen(path, "w");
        if (fp) { fputs(json, fp); fclose(fp); }
    }
    free(json);
}

int baidu_oauth_load_token(char *access_buf, size_t access_len,
                            char *refresh_buf, size_t refresh_len)
{
    if (!access_buf || !refresh_buf) return -1;

    const char *dirs[] = { TOKEN_DIR, TOKEN_BACKUP_DIR, NULL };
    for (int i = 0; dirs[i]; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", dirs[i], TOKEN_FILE);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *buf = malloc(sz + 1);
        if (!buf) { fclose(fp); continue; }
        fread(buf, 1, sz, fp);
        buf[sz] = '\0';
        fclose(fp);

        cJSON *root = cJSON_Parse(buf);
        free(buf);
        if (!root) continue;

        cJSON *acc  = cJSON_GetObjectItem(root, "access_token");
        cJSON *refr = cJSON_GetObjectItem(root, "refresh_token");
        if (cJSON_IsString(acc) && cJSON_IsString(refr)) {
            strncpy(access_buf,  acc->valuestring, access_len  - 1);
            strncpy(refresh_buf, refr->valuestring, refresh_len - 1);
            access_buf[access_len - 1]   = '\0';
            refresh_buf[refresh_len - 1] = '\0';
            cJSON_Delete(root);
            return 0;
        }
        cJSON_Delete(root);
    }
    return -1;
}

void baidu_oauth_clear_token(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", TOKEN_DIR, TOKEN_FILE);
    unlink(path);
    snprintf(path, sizeof(path), "%s/%s", TOKEN_BACKUP_DIR, TOKEN_FILE);
    unlink(path);
}

int baidu_oauth_has_token(void)
{
    char a[256], r[256];
    return baidu_oauth_load_token(a, sizeof(a), r, sizeof(r)) == 0;
}

/*--------------------------------------------------------------------
 * Netdisk file list
 *--------------------------------------------------------------------*/

#define PAN_API_URL "https://pan.baidu.com/rest/2.0/xpan/file"

static pthread_t       g_list_thread;
static volatile int    g_list_running = 0;

static void *list_thread(void *arg);

int baidu_oauth_list_files(const char *access_token, const char *dir,
                            baidu_oauth_list_cb_t cb, void *user_data)
{
    if (g_list_running || !access_token || !cb) return -1;

    g_list_running = 1;
    struct {
        const char            *token;
        const char            *dir;
        baidu_oauth_list_cb_t  cb;
        void                  *ud;
    } *ctx = malloc(sizeof(*ctx));
    ctx->token = strdup(access_token);
    ctx->dir   = strdup(dir ? dir : "/");
    ctx->cb    = cb;
    ctx->ud    = user_data;

    int ret = pthread_create(&g_list_thread, NULL, list_thread, ctx);
    if (ret != 0) {
        free((void *)ctx->token);
        free(ctx);
        g_list_running = 0;
        return -1;
    }
    pthread_detach(g_list_thread);
    return 0;
}

/* Apply common options to a curl handle (timeout, SSL for embedded) */
static void curl_set_common_opts(CURL *curl)
{
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    if (!getenv("BAIDU_SSL_VERIFY")) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
}

static size_t mem_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    mem_buf_t *buf = (mem_buf_t *)userdata;
    size_t total = size * nmemb;
    uint8_t *p = realloc(buf->data, buf->len + total + 1);
    if (!p) return 0;
    buf->data = p;
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

static void free_mem_buf(mem_buf_t *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
}

static void *list_thread(void *arg)
{
    struct {
        const char            *token;
        const char            *dir;
        baidu_oauth_list_cb_t  cb;
        void                  *ud;
    } *ctx = arg;

    CURL *curl = curl_easy_init();
    if (!curl) { ctx->cb(NULL, 0, ctx->ud); goto done; }

    char url[512];
    snprintf(url, sizeof(url),
             "%s?method=list&access_token=%s&dir=%s&start=0&limit=200",
             PAN_API_URL, ctx->token, ctx->dir ? ctx->dir : "/");

    curl_set_common_opts(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    mem_buf_t buf = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !buf.data) {
        ctx->cb(NULL, 0, ctx->ud);
        free_mem_buf(&buf);
        goto done;
    }

    cJSON *root = cJSON_Parse((char *)buf.data);
    if (!root) {
        ctx->cb(NULL, 0, ctx->ud);
        free_mem_buf(&buf);
        goto done;
    }

    cJSON *list = cJSON_GetObjectItem(root, "list");
    if (!cJSON_IsArray(list)) {
        cJSON_Delete(root);
        ctx->cb(NULL, 0, ctx->ud);
        free_mem_buf(&buf);
        goto done;
    }

    int count = cJSON_GetArraySize(list);
    baidu_oauth_file_t *files = count > 0 ? calloc(count, sizeof(*files)) : NULL;
    int n = 0;

    cJSON *item;
    cJSON_ArrayForEach(item, list) {
        cJSON *p  = cJSON_GetObjectItem(item, "path");
        cJSON *nm = cJSON_GetObjectItem(item, "server_filename");
        cJSON *fs = cJSON_GetObjectItem(item, "fs_id");
        cJSON *id = cJSON_GetObjectItem(item, "isdir");
        cJSON *sz = cJSON_GetObjectItem(item, "size");

        if (cJSON_IsString(p)) {
            files[n].path = strdup(p->valuestring);
            files[n].name = cJSON_IsString(nm) ? strdup(nm->valuestring) : strdup("");
            files[n].fs_id = cJSON_IsNumber(fs) ?
                strdup(cJSON_PrintUnformatted(fs)) : strdup("");
            files[n].is_dir = cJSON_IsNumber(id) ? (id->valueint != 0) : 0;
            files[n].size   = cJSON_IsNumber(sz) ? (size_t)sz->valuedouble : 0;
            n++;
        }
    }

    cJSON_Delete(root);
    free_mem_buf(&buf);

    ctx->cb(files, n, ctx->ud);

    for (int i = 0; i < n; i++) {
        free(files[i].path);
        free(files[i].name);
        free(files[i].fs_id);
    }
    free(files);

done:
    free((void *)ctx->token);
    free((void *)ctx->dir);
    free(ctx);
    g_list_running = 0;
    return NULL;
}

/*--------------------------------------------------------------------
 * File download
 *--------------------------------------------------------------------*/

static const char *get_download_dir(void)
{
    const char *dir = getenv("BAIDU_DOWNLOAD_DIR");
    return dir ? dir : "/mnt/sdcard/baidu-xkphoto/img";
}

static int ensure_download_dirs(const char *fname,
                                 char *path1, size_t sz1,
                                 char *path2, size_t sz2)
{
    const char *dirs[] = { get_download_dir(), TOKEN_BACKUP_DIR };
    char **paths[]     = { &path1, &path2 };
    size_t *sizes[]    = { &sz1, &sz2 };

    for (int i = 0; i < 2; i++) {
        ensure_dir(dirs[i]);
        snprintf(*paths[i], *sizes[i], "%s/%s", dirs[i], fname);
    }
    return 0;
}

/** Write callback with speed tracking */
struct dl_ctx {
    FILE *fp;
    baidu_oauth_dl_cb_t cb;
    void *ud;
    char  fname[256];
    size_t total;
    uint32_t last_tick;
    uint32_t tick_start;
};

static size_t dl_write_cb(void *ptr, size_t sz, size_t nmemb, void *ud)
{
    struct dl_ctx *d = ud;
    if (!d->fp) return 0;
    size_t n = fwrite(ptr, sz, nmemb, d->fp);
    d->total += n;

    /* Report speed every ~1 second */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t now = (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    if (now - d->last_tick >= 1000) {
        uint32_t elapsed = now - d->tick_start;
        if (elapsed > 0 && d->cb) {
            size_t speed = d->total * 1000 / elapsed;
            char buf[320];
            snprintf(buf, sizeof(buf), "%s  %zu/%zu KB/s",
                     d->fname, d->total / 1024, speed / 1024);
            d->cb(BAIDU_DL_PROGRESS, buf, d->ud);
        }
        d->last_tick = now;
    }
    return n;
}

static void *download_thread(void *arg);

int baidu_oauth_download_file(const char *access_token,
                               const char *fs_id,
                               const char *remote_name,
                               baidu_oauth_dl_cb_t cb, void *user_data)
{
    /* Build ctx */
    struct {
        char   *token;
        char   *fs_id;
        char   *name;
        baidu_oauth_dl_cb_t cb;
        void   *ud;
    } *ctx = calloc(1, sizeof(*ctx));
    ctx->token = strdup(access_token);
    ctx->fs_id = strdup(fs_id ? fs_id : "");
    ctx->name  = strdup(remote_name ? remote_name : "");
    ctx->cb    = cb;
    ctx->ud    = user_data;

    pthread_t t;
    if (pthread_create(&t, NULL, download_thread, ctx) != 0) {
        free(ctx->token); free(ctx->fs_id); free(ctx->name); free(ctx);
        return -1;
    }
    pthread_detach(t);
    return 0;
}

static void *download_thread(void *arg)
{
    struct {
        char   *token;
        char   *fs_id;
        char   *name;
        baidu_oauth_dl_cb_t cb;
        void   *ud;
    } *ctx = arg;

    char path1[512], path2[512];
    const char *fname = ctx->name[0] ? ctx->name : "download";
    ensure_download_dirs(fname, path1, sizeof(path1), path2, sizeof(path2));

    /* Step 1: get dlink via filemetas */
    CURL *curl = curl_easy_init();
    char  dlink[1024] = {0};

    if (curl) {
        char url[1024];
        snprintf(url, sizeof(url),
                 "%s?method=filemetas&access_token=%s&fsids=[%s]&dlink=1",
                 PAN_API_URL, ctx->token, ctx->fs_id);

        curl_set_common_opts(curl);
        curl_easy_setopt(curl, CURLOPT_URL, url);

        mem_buf_t b = {0};
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (b.data) {
            cJSON *root = cJSON_Parse((char *)b.data);
            if (root) {
                cJSON *arr = cJSON_GetObjectItem(root, "info");
                if (cJSON_IsArray(arr) && cJSON_GetArraySize(arr) > 0) {
                    cJSON *meta = cJSON_GetArrayItem(arr, 0);
                    cJSON *dl   = cJSON_GetObjectItem(meta, "dlink");
                    if (cJSON_IsString(dl))
                        strncpy(dlink, dl->valuestring, sizeof(dlink) - 1);
                }
                cJSON_Delete(root);
            } else {
                fprintf(stderr, "baidu_oauth: JSON parse failed\n");
            }
            free_mem_buf(&b);
        }
    }

    if (!dlink[0]) {
        if (ctx->cb) ctx->cb(BAIDU_DL_ERROR, "Failed to get dlink", ctx->ud);
        goto done;
    }

    /* Step 2: download file via dlink */
    struct dl_ctx d = { .cb = ctx->cb, .ud = ctx->ud, .total = 0 };
    snprintf(d.fname, sizeof(d.fname), "%s", fname);
    struct timespec ts0;
    clock_gettime(CLOCK_MONOTONIC, &ts0);
    d.tick_start = d.last_tick = (uint32_t)(ts0.tv_sec * 1000 + ts0.tv_nsec / 1000000);
    if (ctx->cb) ctx->cb(BAIDU_DL_PROGRESS, fname, ctx->ud);

    /* Download to primary path */
    d.fp = fopen(path1, "wb");
    if (d.fp) {
        curl = curl_easy_init();
        curl_set_common_opts(curl);
        /* Large files need longer timeout */
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
        char dl_url[1536];
        snprintf(dl_url, sizeof(dl_url), "%s&access_token=%s",
                 dlink, ctx->token);
        curl_easy_setopt(curl, CURLOPT_URL, dl_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &d);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(d.fp);
        d.fp = NULL;

        if (ctx->cb) ctx->cb(BAIDU_DL_SUCCESS, path1, ctx->ud);
    } else {
        if (ctx->cb) ctx->cb(BAIDU_DL_ERROR, "Cannot create file", ctx->ud);
    }

done:
    free(ctx->token); free(ctx->fs_id); free(ctx->name); free(ctx);
    return NULL;
}

/*--------------------------------------------------------------------
 * Directory recursive download
 *--------------------------------------------------------------------*/

typedef struct {
    char *token;
    char *dir;
    baidu_oauth_dl_cb_t cb;
    void *ud;
    int   total;
    int   done;
} dir_dl_ctx_t;

static void dir_dl_file_cb(const baidu_oauth_file_t *files, int count,
                            void *user_data);

static void dir_dl_one_cb(baidu_dl_status_t st, const char *msg,
                           void *user_data);

int baidu_oauth_download_dir(const char *access_token,
                              const char *remote_dir,
                              baidu_oauth_dl_cb_t cb, void *user_data)
{
    dir_dl_ctx_t *ctx = calloc(1, sizeof(*ctx));
    ctx->token = strdup(access_token);
    ctx->dir   = strdup(remote_dir);
    ctx->cb    = cb;
    ctx->ud    = user_data;
    ctx->total = 0;
    ctx->done  = 0;

    /* Step 1: list the directory, then download each file recursively */
    struct {
        dir_dl_ctx_t *ctx;
    } *w = calloc(1, sizeof(*w));
    w->ctx = ctx;
    return baidu_oauth_list_files(access_token, remote_dir, dir_dl_file_cb, w);
}

static void dir_dl_file_cb(const baidu_oauth_file_t *files, int count,
                            void *user_data)
{
    /* user_data is the wrapper */
    struct { dir_dl_ctx_t *ctx; } *w = user_data;
    dir_dl_ctx_t *ctx = w->ctx;
    free(w);

    if (!files || count == 0) {
        if (ctx->cb) ctx->cb(BAIDU_DL_COMPLETE, "done", ctx->ud);
        free(ctx->token); free(ctx->dir); free(ctx);
        return;
    }

    ctx->total = count;
    ctx->done  = 0;
    for (int i = 0; i < count; i++) {
        if (files[i].is_dir) {
            /* Recursive sub-directory */
            /* For simplicity, skip nested dirs for now */
            if (ctx->cb)
                ctx->cb(BAIDU_DL_PROGRESS,
                        files[i].name, ctx->ud);
            ctx->done++;
        } else {
            baidu_oauth_download_file(ctx->token,
                                       files[i].fs_id,
                                       files[i].name,
                                       dir_dl_one_cb,
                                       ctx);
        }
    }

    if (ctx->done >= ctx->total) {
        if (ctx->cb) ctx->cb(BAIDU_DL_COMPLETE, "done", ctx->ud);
        free(ctx->token); free(ctx->dir); free(ctx);
    }
}

static void dir_dl_one_cb(baidu_dl_status_t st, const char *msg,
                           void *user_data)
{
    dir_dl_ctx_t *ctx = user_data;
    if (ctx->cb) ctx->cb(st, msg, ctx->ud);
    if (st != BAIDU_DL_PROGRESS) {
        ctx->done++;
        if (ctx->done >= ctx->total) {
            if (ctx->cb) ctx->cb(BAIDU_DL_COMPLETE, "done", ctx->ud);
            free(ctx->token); free(ctx->dir); free(ctx);
        }
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*--------------------------------------------------------------------
 * curl helpers
 *--------------------------------------------------------------------*/

/*--------------------------------------------------------------------
 * JSON helpers (lightweight, no library needed)
 *--------------------------------------------------------------------*/

/** Extract the string value for a JSON key.  Caller must free(). */
static char *json_extract(const char *json, const char *key)
{
    if (!json || !key) return NULL;

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);

    char *pos = strstr((char *)json, pattern);
    if (!pos) return NULL;

    pos += strlen(pattern);
    char *end = pos;
    /* Find the closing unescaped quote */
    while (*end) {
        if (*end == '"') break;
        if (*end == '\\' && *(end + 1)) end++; /* skip escaped char */
        end++;
    }
    if (!*end) return NULL;

    size_t raw_len = end - pos;

    /* Allocate and copy, unescaping JSON sequences */
    char *value = malloc(raw_len + 1);
    if (!value) return NULL;

    char *dst = value;
    const char *src = pos;
    while (src < end) {
        if (*src == '\\' && (src + 1) < end) {
            src++;
            switch (*src) {
            case '/':  *dst++ = '/';  break;  /* \/ → / */
            case '"':  *dst++ = '"';  break;
            case '\\': *dst++ = '\\'; break;
            case 'n':  *dst++ = '\n'; break;
            case 't':  *dst++ = '\t'; break;
            case 'r':  *dst++ = '\r'; break;
            default:   *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return value;
}

/*--------------------------------------------------------------------
 * API calls
 *--------------------------------------------------------------------*/

/**
 * GET /device/code
 * On success the caller must free *device_code, *qrcode_url, *user_code.
 */
static int get_device_code(const char *client_id,
                           char **device_code,
                           char **qrcode_url,
                           char **user_code)
{
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    /* Optional device_id for hardware applications */
    const char *device_id = getenv("BAIDU_DEVICE_ID");

    char url[512];
    if (device_id && device_id[0]) {
        snprintf(url, sizeof(url),
                 "%s?response_type=device_code&client_id=%s"
                 "&scope=basic,netdisk&device_id=%s",
                 DEVICE_CODE_URL, client_id, device_id);
    } else {
        snprintf(url, sizeof(url),
                 "%s?response_type=device_code&client_id=%s"
                 "&scope=basic,netdisk",
                 DEVICE_CODE_URL, client_id);
    }

    mem_buf_t buf = {0};
    curl_set_common_opts(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    if (getenv("BAIDU_CURL_VERBOSE"))
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    fprintf(stderr, "baidu_oauth: device_code HTTP %ld, body_len=%zu\n",
            http_code, buf.len);

    if (res != CURLE_OK) {
        fprintf(stderr, "baidu_oauth: device_code request: %s\n",
                curl_easy_strerror(res));
        free_mem_buf(&buf);
        return -1;
    }

    *device_code = json_extract((char *)buf.data, "device_code");
    *qrcode_url  = json_extract((char *)buf.data, "qrcode_url");
    *user_code   = json_extract((char *)buf.data, "user_code");

    /* Debug: print all extracted fields */
    fprintf(stderr, "baidu_oauth: device_code=%s\n",
            *device_code ? *device_code : "(null)");
    fprintf(stderr, "baidu_oauth: qrcode_url=%s\n",
            *qrcode_url ? *qrcode_url : "(null)");
    fprintf(stderr, "baidu_oauth: user_code=%s\n",
            *user_code ? *user_code : "(null)");

    if (!*device_code || !*qrcode_url || !*user_code) {
        /* Try to extract Baidu error fields from the response */
        char *err = json_extract((char *)buf.data, "error");
        char *desc = json_extract((char *)buf.data, "error_description");
        fprintf(stderr, "baidu_oauth: device_code failed (HTTP %ld)\n", http_code);
        if (err)  fprintf(stderr, "baidu_oauth:   error: %s\n", err);
        if (desc) fprintf(stderr, "baidu_oauth:   description: %s\n", desc);
        fprintf(stderr, "baidu_oauth:   raw: %s\n",
                buf.data ? (char *)buf.data : "(empty)");
        free(err);
        free(desc);
        free(*device_code); *device_code = NULL;
        free(*qrcode_url);  *qrcode_url  = NULL;
        free(*user_code);   *user_code   = NULL;
        free_mem_buf(&buf);
        return -1;
    }

    free_mem_buf(&buf);

    return 0;
}

/** GET qrcode_url → PNG binary. */
static int download_qr(const char *qrcode_url,
                       uint8_t **png_data, size_t *png_len)
{
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    mem_buf_t buf = {0};
    curl_set_common_opts(curl);
    curl_easy_setopt(curl, CURLOPT_URL, qrcode_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "baidu_oauth: qr download: %s\n",
                curl_easy_strerror(res));
        free_mem_buf(&buf);
        return -1;
    }

    *png_data = buf.data;
    *png_len  = buf.len;
    return 0;
}

/**
 * GET /token (poll)
 *
 * @param[in]  client_id     App Key
 * @param[in]  client_secret App Secret
 * @param[in]  device_code   from /device/code
 * @param[out] access_token  set on success (caller must free)
 * @param[out] refresh_token set on success (caller must free)
 *
 * @return 0 = success, 1 = pending, 2 = declined, 3 = expired, -1 = error
 */
static int poll_token(const char *client_id, const char *client_secret,
                      const char *device_code,
                      char **access_token, char **refresh_token)
{
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    char url[1024];
    snprintf(url, sizeof(url),
             "%s?grant_type=device_token&code=%s"
             "&client_id=%s&client_secret=%s",
             TOKEN_URL, device_code, client_id, client_secret);

    mem_buf_t buf = {0};
    curl_set_common_opts(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "baidu_oauth: token poll: %s\n",
                curl_easy_strerror(res));
        free_mem_buf(&buf);
        return -1;
    }

    /* Check for error codes first */
    char *err = json_extract((char *)buf.data, "error");
    if (err) {
        int ret = -1;
        if      (strcmp(err, "authorization_pending")  == 0) ret = 1;
        else if (strcmp(err, "authorization_declined") == 0) ret = 2;
        else if (strcmp(err, "expired_token")          == 0) ret = 3;
        free(err);
        free_mem_buf(&buf);
        return ret;
    }

    /* No error → success, extract tokens */
    *access_token  = json_extract((char *)buf.data, "access_token");
    *refresh_token = json_extract((char *)buf.data, "refresh_token");
    free_mem_buf(&buf);

    if (!*access_token) {
        fprintf(stderr, "baidu_oauth: no access_token in success response\n");
        return -1;
    }

    return 0;
}

/*--------------------------------------------------------------------
 * Main login thread
 *--------------------------------------------------------------------*/

static const char *get_client_id(void)
{
    const char *s = getenv("BAIDU_CLIENT_ID");
    return s ? s : "";
}

static const char *get_client_secret(void)
{
    const char *s = getenv("BAIDU_CLIENT_SECRET");
    return s ? s : "";
}

static void *oauth_thread(void *arg)
{
    (void)arg;

    const char *client_id     = get_client_id();
    const char *client_secret = get_client_secret();

    if (!client_id[0] || !client_secret[0]) {
        fprintf(stderr, "baidu_oauth: BAIDU_CLIENT_ID/BAIDU_CLIENT_SECRET "
                "not set in environment\n");
        if (g_status_cb)
            g_status_cb(BAIDU_OAUTH_ERROR, NULL, NULL, g_user_data);
        g_running = 0;
        return NULL;
    }

    char    *device_code  = NULL;
    char    *qrcode_url   = NULL;
    char    *user_code    = NULL;
    uint8_t *png_data     = NULL;
    size_t   png_len      = 0;

    /* Step 1 — get device code + QR URL */
    if (get_device_code(client_id, &device_code, &qrcode_url, &user_code) != 0) {
        g_running = 0; /* allow retry immediately */
        if (g_status_cb)
            g_status_cb(BAIDU_OAUTH_ERROR, NULL, NULL, g_user_data);
        goto done;
    }

    /* Step 2 — download QR image */
    if (download_qr(qrcode_url, &png_data, &png_len) != 0) {
        g_running = 0; /* allow retry immediately */
        if (g_status_cb)
            g_status_cb(BAIDU_OAUTH_ERROR, NULL, NULL, g_user_data);
        goto done;
    }

    /* Notify UI: QR is ready */
    if (g_qr_cb)
        g_qr_cb(png_data, png_len, user_code, g_user_data);

    /* Step 3 — poll for token */
    while (g_running) {
        char *access  = NULL;
        char *refresh = NULL;

        int ret = poll_token(client_id, client_secret, device_code,
                             &access, &refresh);

        switch (ret) {
        case 0: /* SUCCESS */
            g_running = 0;
            if (g_status_cb)
                g_status_cb(BAIDU_OAUTH_SUCCESS, access, refresh, g_user_data);
            free(access);
            free(refresh);
            goto done;

        case 1: /* PENDING */
            if (g_status_cb)
                g_status_cb(BAIDU_OAUTH_PENDING, NULL, NULL, g_user_data);
            break;

        case 2: /* DECLINED */
            g_running = 0;
            if (g_status_cb)
                g_status_cb(BAIDU_OAUTH_DECLINED, NULL, NULL, g_user_data);
            goto done;

        case 3: /* EXPIRED */
            g_running = 0;
            if (g_status_cb)
                g_status_cb(BAIDU_OAUTH_EXPIRED, NULL, NULL, g_user_data);
            goto done;

        default: /* ERROR */
            g_running = 0;
            if (g_status_cb)
                g_status_cb(BAIDU_OAUTH_ERROR, NULL, NULL, g_user_data);
            goto done;
        }

        sleep(POLL_INTERVAL_S);
    }

done:
    free(device_code);
    free(qrcode_url);
    free(user_code);
    /* png_data ownership was passed to qr_cb — do NOT free here */
    g_running = 0;
    return NULL;
}
