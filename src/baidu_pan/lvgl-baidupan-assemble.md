# 百度网盘 LVGL 集成 — 完整落地文档

> **项目**: lv_port_linux (LVGL v9.6) + qm10xd ARM Linux
> **日期**: 2026-06-03
> **覆盖范围**: XOSFB 驱动适配 + 百度 OAuth 登录 + 文件管理 + 图片预览
> 
> **本文档合并、修正了以下 3 份参考文档，并整合全部实际落地经验：**
> - `百度网盘设备码模式与LVGL集成实现方案.md`
> - `百度网盘登录标签页 (LVGL Tab + OAuth设备流程).md`
> - `生成扫码登录二维码 — OAuth2.0 设备码模式（Device Flow）【硬件应用】.md`

---

## 目录

1. [参考文档信息验证与修正](#1-参考文档信息验证与修正)
2. [项目背景与改造目标](#2-项目背景与改造目标)
3. [百度 OAuth Device Flow 完整流程](#3-百度-oauth-device-flow-完整流程)
4. [百度开放平台 API 完整参考](#4-百度开放平台-api-完整参考)
5. [最终落地架构](#5-最终落地架构)
6. [原始问题清单与根因分析](#6-原始问题清单与根因分析)
7. [最终完整解决方案](#7-最终完整解决方案)
8. [最终落地配置与命令全集](#8-最终落地配置与命令全集)
9. [标准化操作流程](#9-标准化操作流程)
10. [已知限制与后续优化](#10-已知限制与后续优化)

---

## 1. 参考文档信息验证与修正

### 1.1 文档 A: "百度网盘设备码模式与LVGL集成实现方案"

| 内容 | 验证结果 | 说明 |
|------|:---:|------|
| API 端点 `/device/code` | ✅ 正确 | URL、参数均准确 |
| 必填参数 3 个 (client_id, response_type, scope) | ✅ 正确 | 实际测试通过 |
| `device_id` 可选参数 | ✅ 正确 | 已集成到环境变量 `BAIDU_DEVICE_ID` |
| 返回 JSON 结构 | ✅ 正确 | `device_code`, `user_code`, `qrcode_url`, `expires_in`, `interval` 均与实测一致 |
| `user_code` 为 6 位字符 | ✅ 正确 | 实际 `em28dcrp` 等格式 |
| **必须硬件应用** | ✅ **关键** | 软件应用返回 401 `unknown client id`，此条极其重要 |
| SecretKey 不要暴露前端 | ✅ 正确 | 通过环境变量传入，不硬编码源码 |
| 轮询间隔 ≥5s | ✅ 正确 | 源码中已设为 `POLL_INTERVAL_S 5` |

**需修正/补充的内容**:

| 内容 | 原文 | 实际 |
|------|------|------|
| QR 渲染方式 | 建议方式 2 "下载 qrcode_url 图片" | ✅ 采用此方案，但需注意 `qrcode_url` 含 JSON 转义 `\/`，下载前必须反转义 |
| filemetas API | 未提及 | 实际编码中发现的差异：响应是 `{"info":[...]}` 非 `"list"`；需 `fsids=[fs_id]` 非 `path` |
| JSON 解析 | 未涉及 | 文件列表 API 返回嵌套 JSON，需使用 cJSON（sysroot 自带），手写解析器不够 |
| libjpeg-turbo | 未涉及 | JPEG 预览需此库；TJPGD 不支持相机照片的 restart markers |
| 超大图片 (>1920px) | 未提及 | 需在解码前检查尺寸，防止 OOM |

### 1.2 文档 B: "百度网盘登录标签页 (LVGL Tab + OAuth设备流程)"

| 内容 | 验证结果 | 说明 |
|------|:---:|------|
| OAuth 流程图 | ✅ 正确 | device_code → QR → poll → token 四步 |
| API 端点 | ✅ 正确 | 3 个端点准确 |
| 轮询响应状态 | ✅ 正确 | `authorization_pending/declined/expired` 三种都有 |
| 线程模型 (pthread + LVGL timer) | ✅ 正确 | 实际采用此模式 |
| Tab 创建模式 | ✅ 正确 | 参照 LVGL widgets demo 模式 |

**需修正的内容**:

| 内容 | 原文 (错误) | 实际 (正确) |
|------|-------------|-------------|
| `poll_token` 函数 | 被调用两次（先 return 后再次调用获取 token） | 已在实现中修正：一次调用同时返回状态码和 token |
| 轮询间隔 | 3 秒 (`sleep(3)`) | **5 秒**（百度 spec: ≥5s） |
| LVGL API 版本 | v8 (`lv_img_*`, `lv_spinner_create(parent, time, steps)`) | **v9.6** (`lv_image_*`, `lv_spinner_create(parent)`, `lv_image_dsc_t` 结构体不同) |
| client_id/secret | 硬编码宏 `#define BAIDU_CLIENT_ID "xxx"` | **环境变量** `getenv("BAIDU_CLIENT_ID")`，符合文档 A 的"不暴露 SecretKey"原则 |
| JSON 库 | 手写 `json_extract()` 字符串查找 | 最终使用 **cJSON 1.7.19**（sysroot 自带）处理嵌套 JSON |
| 硬件应用类型 | 文档 B 未强调 | **必须硬件应用**（文档 A 已指出），实测软件应用返回 401 |
| SSL 证书 | 未涉及 | 嵌入式无 CA 证书，需 `CURLOPT_SSL_VERIFYPEER=0` |
| `qrcode_url` 格式 | 未提及转义 | 百度返回 `https:\/\/openapi...`，JSON 解析时必须反转 `\/` → `/` |
| 下载函数 `download_qr` | 未设置 SSL 跳过 | 必须也调用 `curl_set_common_opts()` (与 device_code 请求一致) |
| filemetas API 字段 | 未涉及 | `"info"` 非 `"list"`；`fsids=[fs_id]` 非 path |

### 1.3 文档 C: "生成扫码登录二维码 — OAuth2.0 设备码模式（Device Flow）【硬件应用】"

第三份文档内容较短（2017 字节），核心信息与文档 A 重叠。其关键要点均已整合到本文档中。

---

## 2. 项目背景与改造目标

### 2.1 原始工程

- **平台**: qm10xd ARM Cortex-A7, Linux (uclibc)
- **显示**: 800×1280 LCD, 通过私有 framebuffer 驱动
- **输入**: gslx680 触摸屏 + gpio_keys 物理按键
- **基础工程**: lv_port_linux (LVGL v9.6-dev, 标准 Linux fbdev)

### 2.2 改造目标

| 序号 | 目标 | 状态 |
|------|------|:--:|
| 1 | 封装 qm10xd 平台 fb 为 `libxosfb.a` 自包含静态库 | ✅ |
| 2 | lv_port_linux 新增 XOSFB 显示后端 | ✅ |
| 3 | 百度 OAuth Device Flow 二维码登录 | ✅ |
| 4 | Token 持久化（重启保持登录） | ✅ |
| 5 | 网盘文件列表 + 目录导航 | ✅ |
| 6 | 文件下载（含速度显示） | ✅ |
| 7 | 图片本地预览（JPG/PNG，自动缩放） | ✅ |
| 8 | 中文文件名显示 (CJK 字体) | ✅ |

---

## 3. 百度 OAuth Device Flow 完整流程

```
┌──────────┐     ┌──────────────┐     ┌──────────┐
│  LVGL UI │     │  baidu_oauth │     │ Baidu API│
└────┬─────┘     └──────┬───────┘     └────┬─────┘
     │                   │                  │
     │  点击"Login"      │                  │
     │─────────────────>│                  │
     │                   │ GET /device/code │
     │                   │ (client_id,      │
     │                   │  response_type,  │
     │                   │  scope,          │
     │                   │  device_id)      │
     │                   │─────────────────>│
     │                   │  {device_code,   │
     │                   │   user_code,     │
     │                   │   qrcode_url,    │
     │                   │   expires_in:300,│
     │                   │   interval:5}    │
     │                   │<─────────────────│
     │                   │                  │
     │                   │ GET qrcode_url   │
     │                   │ (SSL verify=0)   │
     │                   │─────────────────>│
     │                   │   PNG binary     │
     │                   │<─────────────────│
     │  显示 QR + user_code                 │
     │<─────────────────│                  │
     │                   │                  │
     │  (pthread 轮询)   │                  │
     │                   │ GET /token       │
     │                   │ (每 5s, SSL=0)   │
     │                   │─────────────────>│
     │                   │  authorization_  │
     │                   │  pending...      │
     │                   │<─────────────────│
     │                   │  ...             │
     │                   │  {access_token,  │
     │                   │   refresh_token} │
     │  登录成功, 保存token                 │
     │<─────────────────│                  │
     │  显示文件列表                        │
     │                   │ GET /xpan/file?  │
     │                   │ method=list      │
     │                   │─────────────────>│
```

### 3.1 流程图关键修正

对比原始参考文档 B，以下为实际实现中的修正：

| 步骤 | 文档 B | 实际实现 | 原因 |
|------|--------|---------|------|
| 轮询间隔 | 3s | **5s** | 百度规范 |
| 下载 QR | 默认 SSL | **SSL verify=0** | 嵌入式无 CA |
| 轮询 token | 默认 SSL | **SSL verify=0** | 同上 |
| JSON 解析 | 手写 | **cJSON** | 文件列表 API 复杂嵌套 |

---

## 4. 百度开放平台 API 完整参考

### 4.1 获取设备码 & 二维码

```
GET https://openapi.baidu.com/oauth/2.0/device/code

参数:
  client_id      = AppKey (必填)
  response_type  = device_code (固定)
  scope          = basic,netdisk (固定)
  device_id      = AppID (硬件应用可选，已通过 BAIDU_DEVICE_ID 支持)

成功响应 (HTTP 200):
{
    "device_code":      "d586679af4bba7db06f6c7c7edc07d53",
    "user_code":        "em28dcrp",
    "qrcode_url":       "https:\/\/openapi.baidu.com\/device\/qrcode\/xxx\/em28dcrp",
    "verification_url": "https://openapi.baidu.com/device",
    "expires_in":       300,
    "interval":         5
}

错误响应 (HTTP 401):
{
    "error":             "invalid_client",
    "error_description": "unknown client id"
}
```

**关键注意事项**:
- `qrcode_url` 含 `\/` JSON 转义 → 下载前必须反转义为 `/`
- **必须使用硬件应用**的 AppKey → 软件应用返回 401
- 有效期 300 秒，过期需重新请求

### 4.2 轮询获取 Token

```
GET https://openapi.baidu.com/oauth/2.0/token

参数:
  grant_type    = device_token (固定)
  code          = device_code (上一步获取)
  client_id     = AppKey
  client_secret = SecretKey

三种响应:

① 等待扫码:
{ "error": "authorization_pending" }
→ 继续轮询 (间隔 ≥5s)

② 用户拒绝:
{ "error": "authorization_declined" }
→ 终止, 提示用户

③ 授权成功:
{
    "access_token":  "126.38bcf05d...",
    "refresh_token": "127.8b9d9f...",
    "expires_in":    2592000,
    "scope":         "basic netdisk"
}
→ 保存 token, 进入文件列表
```

### 4.3 文件列表

```
GET https://pan.baidu.com/rest/2.0/xpan/file

参数:
  method       = list
  access_token = {access_token}
  dir          = {目录路径, e.g. "/"}
  start        = 0
  limit        = 200

成功响应:
{
    "errno": 0,
    "list": [
        {
            "fs_id":           974494280397831,   // 数字, 用于下载
            "path":            "/iPhone-favorite/DSC04920.jpg",
            "server_filename": "DSC04920.jpg",
            "isdir":           0,
            "size":            5174326,
            "server_ctime":    1780025269
        },
        ...
    ]
}
```

### 4.4 获取下载链接 (filemetas)

```
GET https://pan.baidu.com/rest/2.0/xpan/file

参数:
  method       = filemetas
  access_token = {access_token}
  fsids        = [{fs_id}]       // 注意: 是 fs_id 数组, 不是 path
  dlink        = 1

成功响应:
{
    "errno": 0,
    "info": [                     // 注意: 是 "info" 不是 "list"
        {
            "fs_id":    974494280397831,
            "dlink":    "https://d.pcs.baidu.com/file/xxx?fid=xxx&rt=pr&sign=xxx",
            "filename": "DSC04920.jpg",
            "size":     5174326
        }
    ]
}
```

**关键差异（与最初实现对比）**:

| 字段 | 错误实现 | 正确实现 |
|------|---------|---------|
| 文件列表嵌套数组 | `"list"` | ✅ `"list"` — 这个是对的 |
| filemetas 嵌套数组 | `"list"` | **`"info"`** |
| 文件标识 | `path` (字符串) | **`fs_id`** (数字) |
| filemetas 参数 | `fsids=["/path/to/file"]` | **`fsids=[974494280397831]`** |

### 4.5 下载文件

```
GET {dlink}&access_token={access_token}

dlink 来自 filemetas 响应, 已在查询参数中附加 access_token.
注意: libcurl 需设置 FOLLOWLOCATION (重定向), TIMEOUT=300 (大文件).
```

---

## 5. 最终落地架构

### 5.1 目录结构

```
lv_port_linux/
├── src/
│   ├── baidu_pan/
│   │   ├── baidu_oauth.h       ← OAuth/列表/下载 API
│   │   └── baidu_oauth.c       ← curl + cJSON + pthread 实现
│   ├── lib/
│   │   ├── backends.h          ← 后端注册声明
│   │   ├── driver_backends.c   ← 后端注册 (XOSFB > FBDEV)
│   │   ├── display_backends/
│   │   │   ├── xosfb.c         ← XOSFB 显示后端 (FULL mode)
│   │   │   └── fbdev.c         ← 标准 fbdev (保留)
│   │   └── indev_backends/
│   │       └── evdev.c         ← 触摸 fallback 机制
│   └── main.c
├── lvgl/demos/widgets/
│   ├── lv_demo_widgets.c              ← +BaiduPan tab +slideshow暂停
│   └── lv_demo_widgets_baidu_pan.c/h  ← BaiduPan Tab UI
├── xosfb/
│   ├── libxosfb.a              ← qm10xd fb 自包含库
│   └── include/xosfb.h
├── configs/xosfb.defaults      ← LVGL 配置
├── CMakeLists.txt
└── lvgl_build.sh
```

### 5.2 线程模型

```
主线程 (LVGL UI):   事件处理 + 控件渲染 + 图片解码
         ↕ (volatile flag + lv_timer 200ms)
pthread (curl I/O): OAuth / 文件列表 / 下载
```

- curl 全部在 pthread 中，不阻塞 UI
- LVGL API 仅在主线程调用
- 跨线程数据传递: `volatile flag` + 全局缓冲区

### 5.3 依赖项 (sysroot)

| 包 | 版本 | 用途 |
|----|------|------|
| libcurl | 8.15.0 | HTTPS |
| libcjson | 1.7.19 | JSON 解析 |
| libjpeg-turbo | 3.1.3 | JPEG 解码 |
| libevdev | 1.13.4 | 输入 |
| libssp | — | cJSON 链接 |

---

## 6. 原始问题清单与根因分析

### 6.1 显示后端

| # | 现象 | 根因 | 修复 |
|---|------|------|------|
| H1 | 分辨率 800x480 (实际 800x1280) | 硬编码默认值 | 环境变量 `LV_XOSFB_WIDTH/HEIGHT` 默认 800x1280 |
| H2 | 画面逐帧下移 | DIRECT mode 下 flush_cb stride 用 `area_width` 非 `hor_res` | 改为 FULL mode + 正确 stride |
| H3 | 移除 PAN 后全白 | BUF_NONE 模式 PAN 是 commit 信号 | FULL mode 仅 last_flush 调用一次 PAN |
| H4 | `lv_tick_inc()` 未调用 | 未注册 tick callback | `lv_tick_set_cb(tick_get_cb)` |
| H5 | gslx680 不响应 | KEY=0 被 LVGL 自动发现过滤 | evdev 增加 fallback: access() + lv_evdev_create 逐个尝试 event0/1/2 |
| H6 | slideshow 切走 tab | 无人为干预暂停机制 | `LV_EVENT_VALUE_CHANGED` 回调暂停 5 分钟 |

### 6.2 OAuth

| # | 现象 | 根因 | 修复 |
|---|------|------|------|
| A1 | 401 unknown client | 非硬件应用类型 | 百度平台创建硬件应用 |
| A2/A5 | SSL 证书错误 | 嵌入式无 CA | `CURLOPT_SSL_VERIFYPEER=0` (所有 curl 请求统一设置) |
| A3 | raw response (null) | `free_mem_buf` 在打印前调用 | 调换顺序 |
| A4 | QR URL bad format | `\/` JSON 转义 | `json_extract` 增加 `\/` → `/` 反转义 |
| A6 | QR 太大或裁剪 | 固定尺寸 180/400px | 自动适配 PNG 尺寸 + 白色边距 |
| A7 | 中文乱码 | Montserrat 无 CJK | `lv_font_source_han_sans_sc_16_cjk` |

### 6.3 文件操作

| # | 现象 | 根因 | 修复 |
|---|------|------|------|
| F1 | `no list array` | filemetas 响应 key 为 `"info"` | `cJSON_GetObjectItem(root, "info")` |
| F2 | Failed to get dlink | `fsids` 传了 path 字符串 | 从列表提取 `fs_id`，传 `fsids=[{fs_id}]` |
| F3 | 文件名始终 download | name 参数传 `""` | 通过 fs_id 匹配 `list_cached[]` 获取原名 |
| F4 | 文件不全 | 超时 15s | 下载时 set `CURLOPT_TIMEOUT 300L` |
| F5 | 重复备份 | download 到两个同级目录 | 移除备份复制逻辑 |
| F6/F8 | 目录无导航 | 无 on_dir_click + ".." 按钮 | 实现目录导航 + 返回按钮 |
| F7 | 目录 0 B | 目录无 size | 目录不显示大小 |
| F9 | [View] 换行 | 独立 list button | 作为独立按钮放在文件下方 "   -> [View]" |

### 6.4 图片预览

| # | 现象 | 根因 | 修复 |
|---|------|------|------|
| V1 | `jd_restart error: 6` | TJPGD 不支持 DRI markers | 换用 libjpeg-turbo 3.1.3 |
| V2 | 72MB OOM | 6000×4000 全尺寸解码 | 解码前检查 header，>1920px 拒绝显示 |
| V3 | 大图只显示部分 | 未缩放 | `lv_image_set_scale()`, 自动适配屏幕 |
| V4/V6 | Back 不消失/回根 | ui_clear_widgets 后状态丢失 | `on_dir_click` 检测 spinner=NULL → `ui_set_state(FILE_LIST)`; Back 用 `current_dir` |
| V5 | 界面卡死 | lv_image_decoder_get_info 同步解码大图 | 先尺寸检查再决定是否解码 |

---

## 7. 最终完整解决方案

### 7.1 XOSFB 显示后端核心要点

```c
// 文件: src/lib/display_backends/xosfb.c

// 1. 渲染模式: FULL (不是 DIRECT)
lv_display_set_buffers(disp, buf, buf2, size, LV_DISPLAY_RENDER_MODE_FULL);

// 2. flush_cb: 仅 last_flush 处理
if (!lv_display_flush_is_last(disp)) {
    lv_display_flush_ready(disp);
    return;
}
// ... copy draw buffer to mmap fb ...
xosfb_pan_display(drv->xosfb);   // 必须调用!
lv_display_flush_ready(disp);

// 3. stride 修正: FULL mode 下 color_p 是全屏 buffer
int src_stride = lv_display_get_horizontal_resolution(disp) * px_size;

// 4. Tick 时钟
static uint32_t tick_get_cb(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
```

### 7.2 evdev 触摸 fallback

```c
// 文件: src/lib/indev_backends/evdev.c

// 探测顺序:
// 1. LV_LINUX_EVDEV_POINTER_DEVICE env
// 2. /dev/input/event0/event1/event2 逐个 access() + lv_evdev_create
// 3. lv_evdev_discovery_start() 兜底
```

### 7.3 百度 OAuth curl 配置

```c
// 所有 curl handle 统一配置
static void curl_set_common_opts(CURL *curl) {
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    if (!getenv("BAIDU_SSL_VERIFY")) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
}
// 下载时额外覆盖超时:
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
```

### 7.4 图片预览

```c
// 文件: lv_demo_widgets_baidu_pan.c, on_view_click()

// 先读取 header, 检查尺寸
lv_image_header_t hdr = {0};
lv_image_decoder_get_info(path, &hdr);

if (hdr.w > 1920 || hdr.h > 1920) {
    // 太大 → 不解码, 显示提示
    lv_label_set_text_fmt(lbl, "%s\n%dx%d\nToo large!", nm, hdr.w, hdr.h);
} else {
    // 正常显示 + 自动缩放
    lv_obj_t *img = lv_image_create(container);
    lv_image_set_src(img, path);
    // 计算缩放比例 ...
    lv_image_set_scale(img, zoom);
}
```

---

## 8. 最终落地配置与命令全集

### 8.1 configs/xosfb.defaults

```ini
LV_COLOR_DEPTH       32

LV_USE_LODEPNG        1
LV_USE_LIBJPEG_TURBO  1

LV_USE_LINUX_FBDEV    1
LV_LINUX_FBDEV_RENDER_MODE   LV_DISPLAY_RENDER_MODE_FULL
LV_LINUX_FBDEV_BUFFER_COUNT  1

LV_USE_EVDEV          1

LV_BUILD_EXAMPLES     1
LV_BUILD_DEMOS        1

LV_USE_DEMO_WIDGETS         1
LV_USE_DEMO_BENCHMARK       1
LV_USE_DEMO_STRESS          1
LV_USE_DEMO_MUSIC           1

LV_USE_LOG           1
LV_LOG_LEVEL         LV_LOG_LEVEL_WARN

LV_USE_SYSMON              1
LV_USE_PERF_MONITOR        1

LV_USE_FLOAT            1
LV_USE_MATRIX           1
LV_USE_VECTOR_GRAPHIC   1
LV_USE_THORVG_INTERNAL  1
LV_USE_LOTTIE           1

LV_USE_FS_STDIO         1
LV_FS_DEFAULT_DRIVER_LETTER 'A'
LV_FS_STDIO_LETTER      'A'

LV_DRAW_LAYER_SIMPLE_BUF_SIZE    (256 * 1024)
LV_OBJ_STYLE_CACHE      1
LV_USE_DRAW_SW_COMPLEX_GRADIENTS 1

LV_FONT_SOURCE_HAN_SANS_SC_16_CJK  1
LV_FONT_FMT_TXT_LARGE       1

LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
LV_USE_STDLIB_STRING LV_STDLIB_CLIB
LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
```

### 8.2 CMakeLists.txt 关键段

```cmake
# XOSFB + curl + cJSON (在 LV_USE_LINUX_FBDEV 条件内)
if (CONFIG_LV_USE_LINUX_FBDEV)
    # XOSFB 平台库
    set(XOSFB_LIB "${CMAKE_SOURCE_DIR}/xosfb/libxosfb.a")
    set(XOSFB_INC "${CMAKE_SOURCE_DIR}/xosfb/include")
    if(EXISTS "${XOSFB_LIB}" AND EXISTS "${XOSFB_INC}/xosfb.h")
        list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/xosfb.c)
        list(APPEND PKG_CONFIG_INC ${XOSFB_INC})
        list(APPEND PKG_CONFIG_LIB ${XOSFB_LIB})
    endif()

    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/fbdev.c)

    # libcurl + cJSON
    pkg_check_modules(LIBCURL REQUIRED libcurl)
    list(APPEND PKG_CONFIG_INC ${LIBCURL_INCLUDE_DIRS})
    list(APPEND PKG_CONFIG_LIB ${LIBCURL_LIBRARIES})

    pkg_check_modules(LIBCJSON REQUIRED libcjson)
    list(APPEND PKG_CONFIG_INC ${LIBCJSON_INCLUDE_DIRS})
    list(APPEND PKG_CONFIG_LIB ${LIBCJSON_LIBRARIES} ssp)
endif()

# lvgl 需访问项目根目录的 src/baidu_pan/
target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 可执行文件
add_executable(lvglsim src/main.c src/baidu_pan/baidu_oauth.c)
```

### 8.3 lvgl_build.sh

```bash
#!/bin/bash
. qmenv qm10xd
SYSROOT=/home/scm/prebuilt/xos_toolchain/arm-molv2-linux-uclibcgnueabi/arm-molv2-linux-uclibcgnueabi/sysroot
export SYSROOT
unset PKG_CONFIG_PATH C_INCLUDE_PATH LIBRARY_PATH
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig"
cmake -B build -DCONFIG=xosfb
make -C build -j$(nproc)
```

### 8.4 编译部署命令

```bash
# Step 1: 编译 libxosfb.a
cd /mnt/qm/xos/base/soc/qm10xd/linux/media/sample/modules/xosfb
make clean && make
cp libxosfb.a ~/github_gitee/lv_port_linux/xosfb/

# Step 2: 编译 lv_port_linux
cd ~/github_gitee/lv_port_linux
./lvgl_build.sh

# Step 3: 部署
cp build/bin/lvglsim /mnt/sdcard/

# Step 4: 运行
cd /mnt/sdcard
BAIDU_CLIENT_ID=你的AppKey BAIDU_CLIENT_SECRET=你的SecretKey ./lvglsim
```

### 8.5 运行环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BAIDU_CLIENT_ID` | (必填) | 百度硬件应用 AppKey |
| `BAIDU_CLIENT_SECRET` | (必填) | 百度硬件应用 SecretKey |
| `BAIDU_DEVICE_ID` | 无 | 硬件应用 AppID (可选) |
| `BAIDU_SSL_VERIFY` | 不验证 | 设 1 启用 SSL 证书验证 |
| `BAIDU_DOWNLOAD_DIR` | `/mnt/sdcard/baidu-xkphoto/img` | 下载保存目录 |
| `XOSFB_FORMAT` | ARGB8888 | 像素格式: ARGB8888/ARGB1555/ARGB0565 |
| `LV_XOSFB_WIDTH` | 800 | 屏幕宽度 |
| `LV_XOSFB_HEIGHT` | 1280 | 屏幕高度 |
| `LV_LINUX_EVDEV_POINTER_DEVICE` | 自动探测 | 触摸屏设备路径 |

---

## 9. 标准化操作流程

### 从零复现: 7 步

**Step 1**: 百度开放平台创建**硬件应用** → 获取 AppKey + SecretKey + AppID。

**Step 2**: 编译安装 libxosfb.a 到 lv_port_linux/xosfb/。

**Step 3**: 确认 sysroot 依赖:
```bash
pkg-config --exists libcurl libcjson libevdev
ls "$SYSROOT/include/jpegint.h"     # libjpeg-turbo 需要
ls "$SYSROOT/include/cjson/cJSON.h" # cJSON 头文件
```

**Step 4**: 编译 lv_port_linux:
```bash
cd ~/github_gitee/lv_port_linux && ./lvgl_build.sh
```

**Step 5**: 部署 `build/bin/lvglsim` 到设备 `/mnt/sdcard/`。

**Step 6**: 运行:
```bash
cd /mnt/sdcard
BAIDU_CLIENT_ID=xxx BAIDU_CLIENT_SECRET=yyy ./lvglsim
```

**Step 7**: 切换到第 4 个 "BaiduPan" tab:
- 首次: 点击 Login → QR 码 → 百度 APP 扫码
- 再次启动: 自动登录 → 文件列表 → [DL] 下载 → [View] 预览

---

## 10. 已知限制与后续优化

### 10.1 已知限制

| 项目 | 限制 |
|------|------|
| 应用类型 | **必须硬件应用** |
| QR 有效期 | 300 秒，过期需刷新重试 |
| 图片预览 | ≥1920px 任一维度的图不显示 |
| 中文 | CJK 字体仅 1338 字符，部分生僻字不支持 |
| 文件列表 | limit=200，不支持翻页 |
| 下载 | 单线程，大文件 (>10MB) 耗时数分钟 |
| OAuth | 单实例，不支持同时多登录 |

### 10.2 后续优化建议 (优先级排序)

| 优先级 | 建议 |
|--------|------|
| 高 | Refresh Token 自动续期 (30天过期) |
| 高 | 文件列表分页 |
| 中 | 大图降采样解码 (scale 1/8) 而非直接拒绝 |
| 中 | 下载队列 + 断点续传 |
| 中 | 上传文件到网盘 |
| 中 | 目录导航 true ".." (parent_dir 链) |
| 低 | 完整 CJK 字体 |
| 低 | 多文件并行下载 |
| 低 | 视频缩略图 |

---

## 附录 A: 文件变更清单

### 新建文件

| 文件 | 说明 |
|------|------|
| `src/baidu_pan/baidu_oauth.h` | OAuth + 列表 + 下载 API |
| `src/baidu_pan/baidu_oauth.c` | curl + cJSON + pthread 实现 |
| `lvgl/demos/widgets/lv_demo_widgets_baidu_pan.h` | BaiduPan Tab 声明 |
| `lvgl/demos/widgets/lv_demo_widgets_baidu_pan.c` | BaiduPan Tab UI (状态机) |
| `src/lib/display_backends/xosfb.c` | XOSFB 显示后端 |
| `configs/xosfb.defaults` | XOSFB LVGL 配置 |

### 修改文件

| 文件 | 变更 |
|------|------|
| `src/lib/backends.h` | +`backend_init_xosfb()` |
| `src/lib/driver_backends.c` | XOSFB 注册 + slideshow 暂停 |
| `src/lib/indev_backends/evdev.c` | 触摸 fallback 机制 |
| `lvgl/demos/widgets/lv_demo_widgets.c` | +BaiduPan tab + slideshow 暂停 |
| `CMakeLists.txt` | +curl/cJSON/ssp/xosfb |
| `lvgl_build.sh` | `CONFIG=xosfb` |

## 附录 B: 硬件设备

| 设备 | Linux 路径 | 用途 |
|------|-----------|------|
| Framebuffer | `/dev/fb0` | 显示 |
| VO | `/dev/vo` | 视频输出 |
| 触摸屏 | `/dev/input/event0` | gslx680 |
| 按键 | `/dev/input/event1` | gpio_keys |

---

> **文档版本**: v2.0 (合并修正版)
> **合并来源**: 
> - `百度网盘设备码模式与LVGL集成实现方案.md` (经验证修正)
> - `百度网盘登录标签页 (LVGL Tab + OAuth设备流程).md` (已修正 LVGL v9 + API 差异)  
> - `lvgl-baidupan-assemble.md` (落地文档)
> **适用范围**: lv_port_linux + qm10xd ARM Linux
