# Plan: 百度网盘登录 Tab (LVGL Tab + OAuth Device Flow)

**Source**: 用户需求 + lvgl-baidupan.md 参考实现
**Complexity**: Medium

## Summary

在现有 3 个 tab (Profile/Analytics/Shop) 基础上增加第 4 个 "BaiduPan" tab，
实现百度网盘 OAuth 2.0 Device Flow 登录。全部用纯 C + libcurl + pthread，
不依赖外部 JSON 库。两份参考实现（.claude/plans/baidu-pan-login.plan.md
和 /mnt/d/work/lvglsim/lvgl-baidupan.md）已综合，针对 LVGL v9.6 和
qm10xd 交叉编译环境做了修正。

## 前置条件（已验证）

| 条件 | 状态 |
|------|:---:|
| sysroot 中有 `libcurl.a` + `curl/curl.h` | ✅ 已验证 |
| LVGL 有 `LV_USE_LODEPNG` 可启用 | ✅ 模板中存在 |
| qm10xd 设备有网络 | ✅ 待确认 |
| 百度开放平台已注册应用 (client_id/secret) | ❌ 需申请 |

## LVGL v9 vs v8 关键差异（文档代码需修正）

| 项目 | 用户文档 (v8) | lv_port_linux (v9.6) |
|------|--------------|---------------------|
| 图像控件 | `lv_img_*`, `lv_img_dsc_t` | `lv_image_*`, `lv_image_dsc_t` |
| 图像结构体 | `header.always_zero`, `header.cf` | `header.cf`, `header.w`, `header.h`, `header.stride` |
| spinner | `lv_spinner_create(parent, time, steps)` | `lv_spinner_create(parent)` v9 简化 |
| tabs 创建 | `lv_tabview_add_tab(tv, "Name")` | 同 v8，但需通过 `lv_tabview_get_content(tv, idx)` 获取 tab 内容 |
| image set src | `lv_img_set_src` | `lv_image_set_src` |

## Files to Create

| File | Why |
|------|-----|
| `baidu_oauth.h` | OAuth 客户端 API（与用户文档一致，无需改） |
| `baidu_oauth.c` | OAuth 实现（修复 poll_token 重复调用 bug） |
| `lvgl/demos/widgets/lv_demo_widgets_baidu_pan.h` | Tab 声明 |
| `lvgl/demos/widgets/lv_demo_widgets_baidu_pan.c` | Tab UI（v9 API 适配） |

## Files to Modify

| File | Action |
|------|--------|
| `lvgl/demos/widgets/lv_demo_widgets.c` | 添加 include + 第 4 个 tab + create 调用 |
| `configs/xosfb.defaults` | 添加 `LV_USE_LODEPNG 1` |
| `CMakeLists.txt` | 添加 `pkg_check_modules(CURL REQUIRED libcurl)` + include/link |

## baidu_oauth.h API（与用户文档一致）

```c
#ifndef BAIDU_OAUTH_H
#define BAIDU_OAUTH_H

typedef void (*baidu_oauth_qr_cb_t)(const uint8_t *png_data, size_t png_len,
                                     const char *user_code, void *user_data);

typedef enum {
    BAIDU_OAUTH_PENDING,      /* 等待扫码 */
    BAIDU_OAUTH_SUCCESS,      /* 登录成功 */
    BAIDU_OAUTH_DECLINED,     /* 用户拒绝 */
    BAIDU_OAUTH_EXPIRED,      /* 二维码过期 */
    BAIDU_OAUTH_ERROR         /* 网络/其他错误 */
} baidu_oauth_status_t;

typedef void (*baidu_oauth_status_cb_t)(baidu_oauth_status_t status,
                                         const char *access_token,
                                         const char *refresh_token,
                                         void *user_data);

int  baidu_oauth_login_start(baidu_oauth_qr_cb_t qr_cb,
                              baidu_oauth_status_cb_t status_cb,
                              void *user_data);
void baidu_oauth_login_cancel(void);
void baidu_oauth_cleanup(void);

#endif
```

## baidu_oauth.c 修正要点

用户文档中的实现基本可用，以下需要修正：

1. **poll_token 重复调用 bug**（文档第 367-376 行）：轮询循环中 `poll_token` 被莫名调用了两次
2. **curl_global_init 位置**：移到 `baidu_oauth_login_start()` 开头（而非 pthread 内部）
3. **client_id/secret 配置**：改为环境变量读取，避免硬编码到源码

```c
// client_id 优先从环境变量读取
#define BAIDU_CLIENT_ID  (getenv("BAIDU_CLIENT_ID")  ?: "YOUR_CLIENT_ID")
```

## lv_demo_widgets_baidu_pan.c — LVGL v9 修正版关键差异

```c
// v8 (文档错误):
lv_img_dsc_t img_dsc;
img_dsc.header.always_zero = 0;
img_dsc.header.cf = LV_IMG_CF_RAW_ALPHA;

// v9 (正确):
lv_image_dsc_t img_dsc;
img_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
img_dsc.header.w = 200;
img_dsc.header.h = 200;
img_dsc.header.stride = 200 * 4;
img_dsc.data_size = png_len;
img_dsc.data = qr_png_data;

// v9 使用 lv_image_set_src 而非 lv_img_set_src
lv_image_set_src(qr_img, &img_dsc);
```

## QR 码显示策略

两种方案，根据 PNGLite 可用性选择：

**方案 A（推荐）**: 启用 `LV_USE_LODEPNG 1`，LVGL 自动解码 PNG
```
1. curl 下载 PNG 到内存
2. 保存到 "/A:/tmp/baidu_qr.png" (LVGL STDIO FS)
3. lv_image_set_src(img, "A:/tmp/baidu_qr.png")
```

**方案 B（备选）**: 不依赖 decoder，raw RGBA buffer
```
1. curl 下载 PNG 到内存  
2. 在 PC 端预编译 lodepng 或手动解析 PNG → RGBA pixel buffer
3. 创建 lv_image_dsc_t 指向 raw RGBA 数据
4. lv_image_set_src(img, &img_dsc)
```

## LVGL Tab UI 状态机

```
[INIT] 登录按钮
   │
   ▼ (点击登录 → baidu_oauth_login_start)
[LOADING] spinner + "正在获取二维码..."
   │
   ▼ (qr_cb → qr_ready flag)
[QR_SHOW] QR 图片 + user_code + "请用百度APP扫描"
   │
   ▼ (status_cb → status_changed flag)
   ├─ PENDING  → [轮询中] 更新提示文字 (UI 不变)
   ├─ SUCCESS  → [成功]   绿色 "✓ 登录成功"
   ├─ DECLINED → [失败]   红色 "授权被拒绝" + [重试]
   ├─ EXPIRED  → [过期]   黄色 "二维码已过期" + [刷新]
   └─ ERROR    → [错误]   红色 "网络错误" + [重试]
```

## 线程安全

```
pthread (curl I/O)                  LVGL 主线程 (lv_timer 200ms)
────────────────────                ───────────────────────────────
qr_cb():
  memcpy PNG → global_buf
  global_flag = QR_READY ──────→   检测到 QR_READY
                                      lv_obj_clean(container)
                                      创建 qr_img + label
                                      UI_STATE = QR_SHOW

status_cb():
  copy status + tokens
  global_flag = STATUS_CHANGED ──→ 检测到 STATUS_CHANGED
                                      switch(status):
                                        SUCCESS → 绿色成功页
                                        DECLINED/EXPIRED/ERROR → 失败页+重试
```

## Tasks

### Task 1: 创建 baidu_oauth.c/h

- **Action**: 基于用户文档代码，修正 poll_token bug、curl_global_init 位置、环境变量读取 client_id
- **Source**: `/mnt/d/work/lvglsim/lvgl-baidupan.md` 中的 baidu_oauth.c/h
- **Place**: 放在 lv_port_linux 根目录下的 `baidu_oauth.c` 和 `baidu_oauth.h`
- **Validate**: 编译通过 `baidu_oauth.o`

### Task 2: 创建 lv_demo_widgets_baidu_pan.c/h

- **Action**: 基于用户文档代码，全部 API 改为 LVGL v9 (`lv_image_*`, `lv_image_dsc_t` 结构修正，spinner API)
- **Place**: `lvgl/demos/widgets/lv_demo_widgets_baidu_pan.c` 和 `.h`
- **Validate**: 编译通过

### Task 3: 集成到构建系统

- **Action**:
  - `lv_demo_widgets.c`: 添加 `#include "lv_demo_widgets_baidu_pan.h"` + 第 4 个 tab
  - `configs/xosfb.defaults`: 添加 `LV_USE_LODEPNG 1`
  - `CMakeLists.txt`: `pkg_check_modules(CURL REQUIRED libcurl)` + include/link
- **Validate**: `./lvgl_build.sh` 编译成功

### Task 4: 设备验证

- **Action**: 
  1. 申请百度开放平台 client_id/secret
  2. 部署到 qm10xd，设置环境变量后运行
  ```bash
  BAIDU_CLIENT_ID=xxx BAIDU_CLIENT_SECRET=xxx ./lvglsim
  ```
  3. 切换到 BaiduPan tab，点击登录
  4. 扫码验证 token 获取

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| 百度 API client_id 未申请 | 高 | 需注册百度开放平台；先用 mock 数据验证 UI |
| lodepng 在 ARM 上内存不足 | 中 | 方案 B: raw RGBA buffer，或设更小的 QR 图 |
| 设备无网络/DNS | 中 | 先 ping openapi.baidu.com 验证 |
| poll_token 死循环阻塞 pthread 退出 | 低 | g_oauth_running 标志 + mutex 保护 |

## Acceptance

- [ ] baidu_oauth.c/h 编译通过且 poll_token bug 已修正
- [ ] LVGL 第 4 个 "BaiduPan" tab 可用，UI 状态机正常
- [ ] 二维码显示正确（lodepng 或 raw RGBA）
- [ ] 完整登录流程：点击→二维码→扫码→token
