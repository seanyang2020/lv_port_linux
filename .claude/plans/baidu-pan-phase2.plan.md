# Plan: 百度网盘 Phase 2 — Token 持久化 + 文件列表 + 下载

**Source**: 用户需求
**Complexity**: Medium

## Summary

在 Phase 1（QR 扫码登录）基础上，增加：
1. Token 持久化到 `/data/baidu-xkphot` + 备份到 `/mnt/sdcard/baidu-xkphoto`
2. 启动时自动检查登录状态，已登录直接显示文件列表
3. 注销功能
4. 文件列表展示（调用 Baidu PAN API）
5. 文件下载功能

## JSON 库

**cJSON 1.7.19** 已在 sysroot 中（`pkg-config libcjson`），直接用，无需手写 JSON 解析。

```c
#include <cjson/cJSON.h>   // 解析 + 构建
// CMake: pkg_check_modules(LIBCJSON REQUIRED libcjson)
// Link:  -lcjson
```

## Baidu PAN API

| 操作 | URL | 参数 |
|------|-----|------|
| 文件列表 | `GET pan.baidu.com/rest/2.0/xpan/file?method=list` | `access_token=xxx&dir={path}&start=0&limit=50` |
| 递归列表 | 同上，`dir={subdir}` | 递归遍历所有子目录 |
| 文件下载 | 先 GET `filemetas` 获取 `dlink`，再 GET `dlink` | `dlink=xxx&access_token=xxx` |

### 下载路径

默认: `/data/baidu-xkphot/` + 备份 `/mnt/sdcard/baidu-xkphoto/`
可通过 `BAIDU_DOWNLOAD_DIR` 环境变量覆盖。

### 目录下载

递归流程:
1. `list_files(dir)` → 遍历每个 entry
2. 如果 `isdir=1` → 递归 `download_dir(subdir)`
3. 如果是文件 → `download_file(dlink, local_path)`

## UI 状态机更新

```
Tab 打开
  │
  ├─ 有保存的 token → [FILE_LIST] 文件列表 + [Logout]
  │                     │
  │                     ├─ 点击文件 → [下载] 保存到本地
  │                     └─ 点击 Logout → [INIT] 登录按钮
  │
  └─ 无 token → [INIT] 登录按钮
       │
       ▼ (OAuth 登录流程，同 Phase 1)
     [SUCCESS] → 保存 token → [FILE_LIST]
```

## Files to Modify

| File | Change |
|------|--------|
| `baidu_oauth.h` | 新增: `save_token`, `load_token`, `clear_token`, `has_token`, `list_files`, `download_file` |
| `baidu_oauth.c` | 实现 token JSON 读写、PAN API 列表/下载 |
| `lv_demo_widgets_baidu_pan.c` | 新增 UI_STATE_FILE_LIST 状态, 文件列表展示, 下载按钮 |

## Tasks

### Task 1: Token 持久化 (baidu_oauth.c)

- `baidu_oauth_save_token(access, refresh)` — 保存 JSON 到两个目录
- `baidu_oauth_load_token(access_buf, refresh_buf)` — 先读主目录，失败读备份
- `baidu_oauth_clear_token()` — 删除两个目录下的 token 文件
- `baidu_oauth_has_token()` — 是否已有保存的 token

### Task 2: 文件列表 API (baidu_oauth.c)

- `baidu_oauth_list_files(access_token, cb, user_data)` — 异步 pthread
- 调用 `pan.baidu.com/rest/2.0/xpan/file?method=list`
- JSON 解析 `list[]` 数组，提取 `path`, `server_filename`, `isdir`, `size`
- 回调传递文件数组

### Task 3: 文件下载 API (baidu_oauth.c)

- `baidu_oauth_download_file(access_token, remote_path, cb)` — 异步 pthread
- 先 GET filemetas 获取 `dlink`
- 再 GET `dlink` + `access_token` 下载文件内容
- 保存到 `BAIDU_DOWNLOAD_DIR`（默认 `/data/baidu-xkphot/`）
- 备份到 `/mnt/sdcard/baidu-xkphoto/`

### Task 4: 目录下载

- `baidu_oauth_download_dir(access_token, remote_dir, cb)` — 递归下载
- 先 list_files(remote_dir)
- 对每个文件：download_file
- 对每个子目录：递归 download_dir

### Task 5: LVGL UI 更新

- **QR 尺寸**: 180x180（当前太大占满屏幕）
- 新增 `UI_STATE_FILE_LIST`: 启动时检查 token → 有效则显示文件列表
- 文件列表用 `lv_list` 展示，每个条目显示文件名 + 大小
- 列表入口: 点击进入子目录，返回按钮
- 文件操作: [Download] 按钮下载单文件，[Download All] 下载整个目录
- Logout 按钮 → 清除 token → 回到登录页
- 下载时 spinner + "Downloading..." 提示

### Task 5: 构建验证

- 编译通过
- 设备测试：登录→文件列表→下载文件

## Risks

| Risk | Mitigation |
|------|-----------|
| PAN API 列表解析大 JSON 可能 OOM | 限制 `limit=50`，流式解析 |
| Baidu API 频率限制 | 列表页加手动刷新按钮，不自定轮询 |
| 大文件下载耗时长 | 异步线程 + 下载进度友好提示 |
| `/data` 目录不存在 | 自动 mkdir |

## Acceptance

- [ ] 重启 APP 后登录状态保持
- [ ] 文件列表正确显示
- [ ] 文件可下载到本地
- [ ] 注销后清除 token
