# 配置系统需求文档

## 1. 概述

为 lvglsim/lv_port_linux 设计统一的配置系统，支持：
- JS-Apps 默认启动配置
- Tab 顺序与自动轮播配置
- SDL（开发）与 xOS（部署）无差异化运行

## 2. 配置文件

### 2.1 目录结构

```
仓库 (git):
  src/js_engine/apps/                  ← JS 源码（git 跟踪，单一事实来源）
  config/
  ├── config-entry.json               ← 入口配置模板（git 跟踪，含默认值）
  └── .gitkeep

运行时 (.gitignore):
  config/js-apps/                      ← SDL 运行时（build step 从 src/ 复制）
  ├── config.json                      ← JS-Apps 全局配置（运行时生成）
  ├── weather/
  │   ├── index.js                     ← 从 src/ 复制
  │   ├── skin/*                       ← 从 src/ 复制
  │   └── weather_cache.json           ← 运行时写入
  └── ...

xOS:
  /data/config/
  ├── config-entry.json               ← 可选，不存在时使用 ./config/ 默认值
  └── ...
  /mnt/sdcard/js-apps/                 ← 部署目录
  ├── config.json                      ← JS-Apps 全局配置
  ├── weather/
  │   ├── index.js
  │   ├── skin/*
  │   └── weather_cache.json
  └── ...
```

### 2.2 查找链

```
config-entry.json:
  1. ./config/config-entry.json
  2. /data/config/config-entry.json
  都没找到 → 全部使用编译宏默认值

js-apps 根目录:
  1. config-entry.json → js_apps_path
  2. 编译宏 JS_APPS_DIR_DEFAULT
```

> **注意**：SDL 和 xOS 使用完全相同的查找链，无平台差异。

---

## 3. `config-entry.json` 格式

```json
{
  "js_apps_path":       "./config/js-apps",
  "tab_cycle":          true,
  "tab_cycle_interval": 5000,
  "tab_order":          ["JS-Apps", "Profile", "Analytics", "Shop", "BaiduPan"]
}
```

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `js_apps_path` | string | 平台默认 | JS-Apps 根目录路径 |
| `tab_cycle` | bool | `true` | Tab 是否自动轮播 |
| `tab_cycle_interval` | int (ms) | `5000` | 轮播间隔，毫秒 |
| `tab_order` | array | 见上 | Tab 创建顺序，数组元素为 tab 名称 |

### 编译宏默认值

| 宏 | SDL 默认 | xOS 默认 |
|----|---------|---------|
| `JS_APPS_DIR_DEFAULT` | `"./config/js-apps"` | `"/mnt/sdcard/js-apps"` |

---

## 4. `js-apps/config.json` 格式

```json
{
  "auto_start_app": "none"
}
```

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `auto_start_app` | string | `"none"` | 启动时自动运行的 JS app 名称，`"none"` 表示不自动启动 |

---

## 5. 功能需求

### 5.1 JS-Apps 作为第一个 Tab

- 默认 `tab_order` 中 `"JS-Apps"` 排第一位
- Tab 按 `tab_order` 数组顺序创建
- 数组中不存在的 tab 名称 → 跳过
- 数组为空 → 使用 hardcoded 默认顺序

### 5.2 Tab 自动轮播

| 条件 | 行为 |
|------|------|
| `tab_cycle = true` | 按 `tab_cycle_interval` ms 间隔自动切换下一个 tab |
| `tab_cycle = false` | 仅手动点击切换 |
| `tab_cycle_interval = 0` | 视为 5000ms 默认值 |

### 5.3 JS-Apps 默认启动

**UI（js_tab.c）**：
- JS-Apps 列表上方显示 `"Auto-start"` label + dropdown
- 选项：`"none\napp1\napp2\n..."`（`"none"` + 所有扫描到的 app 名）
- 默认选中 `auto_start_app` 配置值
- `LV_EVENT_VALUE_CHANGED` → 写 `js-apps/config.json`

**自动启动逻辑**：
- `lv_js_tab_create()` 末尾检查 `auto_start_app`
- 若 ≠ `"none"` 且在扫描列表中 → `lv_async_call()` 延迟启动对应 app
- 延迟确保 UI 初始化完成，避免竞态

### 5.4 构建同步（SDL）

- Build step：`cp -r src/js_engine/apps/* config/js-apps/`
- `config/js-apps/` 加入 `.gitignore`
- 首次 build 自动执行，后续 build 增量覆盖

---

## 6. 异常处理

| 场景 | 处理 |
|------|------|
| `config-entry.json` 不存在 | 全部使用编译宏默认值，正常运行 |
| `config-entry.json` 内容损坏 | 日志警告，全部使用默认值 |
| `config-entry.json` 缺少某字段 | 该字段使用默认值 |
| `js-apps/config.json` 不存在 | 默认 `auto_start_app="none"`，首次选择后创建 |
| `js-apps/config.json` 内容损坏 | 日志警告，默认 `"none"`，不覆盖文件 |
| `auto_start_app` 指向不存在的 app | 下拉显示 `"none"`（实际值保留），日志提示，不启动 |
| `tab_order` 缺失/跳过某 tab | 跳过该项，其余按序创建 |
| `tab_order` 为空数组 | 使用 hardcoded 默认顺序 |
| `tab_order` 有重复 | 去重，保留首次出现 |
| 扫描到 0 个 JS app | 下拉仅 `"none"`，显示 `"No apps found"` |
| 写入配置失败（磁盘满等） | 日志警告，UI 状态不回退 |

---

## 7. 涉及文件

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/lib/config_util.h` | **新增** | `config_find()`, `config_get_str()`, `config_get_bool()`, `config_get_int()` |
| `src/lib/config_util.c` | **新增** | 实现：文件查找、简单 JSON 解析、类型转换 |
| `src/js_engine/js_tab.c` | **修改** | 配置读写 + dropdown UI + 自动启动 |
| `src/js_engine/js_tab.h` | **修改** | 暴露 `lv_js_tab_get_auto_start()` |
| `lvgl/demos/widgets/lv_demo_widgets.c` | **修改** | 读取 tab_order / tab_cycle / tab_cycle_interval |
| `.gitignore` | **修改** | 添加 `config/js-apps/` |
| `lvglsim-build.sh` / `lvglxos-build.sh` | **修改** | 设置 `JS_APPS_DIR_DEFAULT` 编译宏 |
| `CMakeLists.txt` | **修改** | 添加 `config_util.c` 编译、SDL copy step |

---

## 8. 实现阶段

| Phase | 内容 | 文件 |
|-------|------|------|
| 1 | `config_util` 基础设施（文件查找 + 简单 JSON 解析） | `config_util.h/c` |
| 2 | `js_tab.c`：`js-apps/config.json` 读写 + dropdown UI | `js_tab.c` |
| 3 | `js_tab.c`：自动启动逻辑 | `js_tab.c` |
| 4 | `lv_demo_widgets.c`：tab 顺序 + 轮播控制 | `lv_demo_widgets.c` |
| 5 | 构建脚本适配 + .gitignore | CMakeLists.txt, build scripts |

---

> **文档版本**: v1.0 | **日期**: 2026-06-17
