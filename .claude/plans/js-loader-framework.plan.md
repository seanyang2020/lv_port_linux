# Plan: JS 动态加载框架 (lv_jsloader)

**Source PRD**: /plan conversation
**Selected Milestone**: 完整架构方案
**Complexity**: Large

## Summary

独立项目 `lv_jsloader`，将 lv_port_linux 中已验证的 JS 引擎集成（QuickJS + libuv + lvgljs bridge + JS-Apps tab）抽离为可复用的 .so 框架。第三方通过 `deps.cfg` 配置 LVGL 路径后即可集成，产物部署在 `jsroot/lib/` + `jsroot/apps/` 下，支持多分辨率应用。

## Key Architecture Decisions

| 决策 | 选择 | 理由 |
|------|------|------|
| 项目位置 | 独立 repo `lv_jsloader` | 第三方无需 clone 150MB lv_port_linux |
| 依赖管理 | `deps.cfg` JSON + pkg-config fallback | 第三方不碰 CMakeLists.txt |
| LVGL 版本 | `bridge/lvgl_v9/` 目录隔离，编译期选择 | v10 到来时加目录即可 |
| 链接方式 | bridge 编译 `--allow-shlib-undefined`，LVGL 符号由 App 提供 | bridge 不与特定 lv_conf.h 绑定 |
| .so 部署 | 构建时自动拷贝到 `jsroot/lib/` | 部署目录自包含 |
| 平台兼容 | 环境变量 `${VAR}` + toolchain file + build scripts | deps.cfg 不改，CMakeLists.txt 不改 |

## Patterns to Mirror

| Category | Source | Pattern |
|----------|--------|---------|
| Naming | `src/js_engine/js_bridge.cpp` | `js_engine_init/run_script/tick/cleanup` lifecycle API |
| Error handling | `src/lib/config_util.c` | 文件不存在返回 NULL，日志到 stderr |
| Logging | `src/js_engine/js_tab.h:16-22` | `JS_LOG(fmt, ...)` → `LV_LOG_USER("[jsloader] " fmt, ...)` |
| Config format | `config/config-entry.json` | JSON + cJSON 解析 + 查找链 |
| Build | `CMakeLists.txt:431` | `add_library(... SHARED ...)` |

## Directory Layout

```
lv_jsloader/                           (独立 git repo)
├── CMakeLists.txt                     (~150 行)
├── deps.cfg                           JSON 依赖声明 (git 跟踪)
├── deps.cfg.local                     (.gitignore, 第三方本地覆盖)
├── deps/
│   └── txiki/                         git submodule (QuickJS + libuv)
├── src/
│   ├── bridge/                        LVGL 版本耦合代码
│   │   └── lvgl_v9/
│   │       ├── js_bridge.cpp          从 lv_port_linux 移植
│   │       └── js_bridge.h
│   ├── bridge_common/                 版本无关的 JS 引擎代码
│   │   ├── engine.cpp/h               QuickJS init / cleanup
│   │   ├── timer.cpp/h                setInterval / clearInterval
│   │   ├── http_client.cpp/h          lvgljs.httpGet (libcurl)
│   │   ├── file_io.cpp/h              lvgljs.readFile / writeFile
│   │   ├── screen_control.cpp/h       backlight, getEnv, FPS
│   │   ├── event_glue.cpp/h           onClick / onPress / onRelease / onChange
│   │   └── widget_factory.cpp/h       label/btn/panel/... 创建入口
│   ├── loader/                        核心框架
│   │   ├── js_loader.c/h              对外 API (init/run_app/tick/cleanup)
│   │   ├── app_discovery.c/h          扫描 jsroot/apps/ 发现 JS 应用
│   │   └── config_util.c/h            JSON 配置读写 (从 lv_port_linux 移植)
│   └── tab/                           JS app 列表 UI
│       └── js_tab.c/h                 从 lv_port_linux 移植
├── jsroot/                            部署骨架
│   ├── lib/                           构建后: .so 文件 + 依赖 .so 自动拷贝
│   ├── apps/
│   │   ├── 800x1280/                  分辨率特定应用
│   │   └── common/                    分辨率无关应用
│   └── config.json                    运行时全局配置
├── examples/
│   └── minimal_sdl/                   SDL + jsloader 最小集成示例
└── docs/
    └── INTEGRATION.md                 第三方集成指南
```

## deps.cfg Specification

### 格式: 支持 `${ENV_VAR}` 展开

```json
{
  "lvgl": {
    "version": 9,
    "include": "${LVGL_INCLUDE}",
    "library": "${LVGL_LIBRARY}"
  },
  "cjson": {
    "include": "${SDKTARGETSYSROOT}/usr/include/cjson",
    "library": "${SDKTARGETSYSROOT}/usr/lib/libcjson.so"
  },
  "libcurl": {
    "pc": "libcurl"
  },
  "freetype": {
    "required": false,
    "pc": "freetype2"
  }
}
```

- `${VAR}` 在 CMake 解析时展开为 `$ENV{VAR}` 环境变量
- `deps.cfg` 一份文件覆盖所有平台，平台差异通过 build script 设置环境变量体现

### 解析优先级

```
1. CMake 命令行     -DLVGL_INCLUDE=/path       (最高优先级)
2. deps.cfg.local   {"lvgl": {"library": "..."}} (本地覆盖, .gitignore)
3. deps.cfg         ${LVGL_INCLUDE} 显式路径     (git 跟踪模板)
4. pkg-config       pc: "lvgl"                   (自动 fallback)
5. FATAL_ERROR      没找到, 提示配置 deps.cfg.local
```

### CMake 解析伪代码

```cmake
function(resolve_dep name)
  # 1. CMake 命令行覆盖
  if(DEFINED ${name}_INCLUDE AND DEFINED ${name}_LIBRARY)
    set(${name}_FOUND TRUE PARENT_SCOPE)
    return()
  endif()

  # 2. deps.cfg.local → deps.cfg → parse JSON, expand ${VAR} → $ENV{VAR}
  set(cfg_val "${DEP_CFG_${name}_LIBRARY}")
  string(REGEX MATCHALL "\\$\\{([^}]+)\\}" vars "${cfg_val}")
  foreach(var ${vars})
    string(REGEX REPLACE "\\$\\{${var}\\}" "$ENV{${var}}" cfg_val "${cfg_val}")
  endforeach()

  # 3. 如果路径有效 → found; 否则尝试 pkg-config
  if(EXISTS "${cfg_val}")
    set(${name}_FOUND TRUE PARENT_SCOPE)
  elseif(DEFINED DEP_CFG_${name}_PC)
    pkg_check_modules(${name} ${DEP_CFG_${name}_PC})
  else()
    message(FATAL_ERROR "${name} not found. Edit deps.cfg.local")
  endif()
endfunction()
```

## Platform Compatibility

### 分层模型

```
┌──────────────────────────────────────────────┐
│  scripts/build-{platform}.sh                 │  ← 环境变量 + toolchain
│  (参考 lv_port_linux: lvglwsl-build.sh,      │
│   lvglxos-build.sh)                          │
├──────────────────────────────────────────────┤
│  CMakeLists.txt + CMAKE_TOOLCHAIN_FILE       │  ← 标准 CMake 交叉编译
│  cmake -DCMAKE_TOOLCHAIN_FILE=arm.cmake      │
├──────────────────────────────────────────────┤
│  deps.cfg + ${VAR} 环境变量展开              │  ← 依赖路径 (不改文件)
│  {"lvgl": {"library": "${LVGL_LIBRARY}"}}    │
└──────────────────────────────────────────────┘
```

### 平台构建脚本 (参考 lv_port_linux 模式)

**x86_64 SDL (开发)** — `scripts/build-sdl.sh`:
```bash
#!/bin/bash
export LVGL_INCLUDE="/usr/include/lvgl"
export LVGL_LIBRARY="/usr/lib/x86_64-linux-gnu/liblvgl.a"
export SDKTARGETSYSROOT="/usr"

cmake -B build-sdl -DCMAKE_BUILD_TYPE=Debug
make -C build-sdl -j$(nproc)
```

**ARM xOS (部署)** — `scripts/build-xos.sh`:
```bash
#!/bin/bash
source qmenv qm10xd                           # 设置 CC/CXX/SYSROOT/...
export LVGL_INCLUDE="${SYSROOT}/usr/include/lvgl"
export LVGL_LIBRARY="${SYSROOT}/usr/lib/liblvgl.a"
export SDKTARGETSYSROOT="${SYSROOT}"

cmake -B build-xos \
  -DCMAKE_TOOLCHAIN_FILE="${SYSROOT}/toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release
make -C build-xos -j$(nproc)

# 部署 .so 到 jsroot/lib/
cmake --install build-xos --prefix jsroot/
```

### 第三方自定义平台

第三方只需创建自己的 build script 或直接导出环境变量 — **不改 deps.cfg，不改 CMakeLists.txt**:

```bash
# 第三方嵌入式 ARM 平台
export CC=arm-linux-gnueabihf-gcc
export SYSROOT=/path/to/sysroot
export LVGL_INCLUDE="${SYSROOT}/usr/include/lvgl"
export LVGL_LIBRARY="${SYSROOT}/usr/lib/liblvgl.a"
cmake -B build -DCMAKE_TOOLCHAIN_FILE=my_arm.cmake
make -C build
```

### 平台矩阵

| 平台 | CC | LVGL 来源 | Toolchain | Build Script |
|------|-----|-----------|-----------|-------------|
| x86_64 SDL | gcc | apt liblvgl-dev | 无 | `scripts/build-sdl.sh` |
| ARM xOS | arm-molv2-...-gcc | 预编译 liblvgl.a | qmenv 环境 | `scripts/build-xos.sh` |
| 第三方 ARM | 自定 | 自编 liblvgl.a | 自定 .cmake | 自建 (参考模板) |

## SO Deployment Rule

构建完成后，CMake `install` 步骤执行:

```
for each .so in (libjsloader, liblvgljs_bridge, libtjs, libquickjs, libuv):
    install(TARGETS ${tgt} LIBRARY DESTINATION ${jsroot}/lib)

for each dep in deps.cfg where library ends with ".so":
    file(COPY ${dep.library} DESTINATION ${jsroot}/lib/)
```

`.a` 依赖不拷贝（静态链接进 .so），`.so` 依赖自动拷贝到 jsroot/lib/。

## Link Model

```
最终 App (第三方 main)
  ├── libjsloader.so           ← dlopen / -ljsloader
  ├── liblvgljs_bridge.so      ← LVGL 符号悬空 (--allow-shlib-undefined)
  ├── libtjs.so                ← 完整自包含 (静态链接 mbedtls/sqlite3)
  ├── libquickjs.so            ← 无外部依赖
  ├── libuv.so                 ← 无外部依赖
  └── liblvgl.a                ← App 自己的 LVGL, 解析 bridge 的 LVGL 符号

部署到 jsroot/lib/:
  libjsloader.so
  liblvgljs_bridge.so
  libtjs.so
  libquickjs.so
  libuv.so
  libcjson.so                  ← .so 依赖自动拷贝
  libcurl.so                   ← .so 依赖自动拷贝
```

## Public API (libjsloader.so)

```c
/* 一次初始化，全局调用 */
int  jsloader_init(const char * jsroot_path);
int  jsloader_run_app(const char * app_name);
int  jsloader_run_app_async(const char * app_name);  /* 延迟到下一帧 */
void jsloader_tick(void);         /* 每帧调用，驱动 libuv */
void jsloader_cleanup(void);

/* 应用发现 */
typedef struct { char name[64]; char path[512]; char resolution[32]; } js_app_t;
int  jsloader_scan_apps(js_app_t * apps, int max_count);
int  jsloader_get_app_count(void);
const char * jsloader_get_app_name(int idx);

/* 配置 */
const char * jsloader_get_auto_start(void);
void jsloader_set_auto_start(const char * app_name);

/* 桥接层: 注册平台特定回调 */
typedef lv_obj_t * (*widget_create_fn)(void * parent, int type, ...);
void jsloader_register_lvgl_vtable(const void * vtable);
```

## Resolution Routing

```
jsroot/apps/
├── 800x1280/          ← lvgljs.getScreenSize() → {w:800, h:1280}
├── 720x1280/
└── common/            ← fallback

加载优先级:
  1. {w}x{h}/app_name/index.js   ← 精确匹配
  2. common/app_name/index.js    ← 通用回退
  3. ERROR: app not found
```

## Implementation Phases

### Phase 1: Build System + SO Export
- **Action**: 创建 `lv_jsloader` repo，移植 `deps/txiki` submodule，编写 CMakeLists.txt 编译 quickjs/uv/tjs/bridge 为 .so
- **Validate**: `cmake -B build && make -C build` 产出 5 个 .so
- **Files**: CMakeLists.txt, deps.cfg

### Phase 2: Bridge Extraction (lvgl_v9)
- **Action**: 从 lv_port_linux 移植 `js_bridge.cpp` → `src/bridge/lvgl_v9/`，拆分为 bridge_common (引擎/定时器/HTTP/文件/事件) + bridge/lvgl_v9 (LVGL widget 调用)
- **Validate**: SDL minimal example 启动 weather app
- **Files**: src/bridge/*, src/bridge_common/*

### Phase 3: Loader Core
- **Action**: 实现 `js_loader.c/h` 对外 API + `app_discovery.c` 分辨率路由
- **Validate**: `jsloader_init/jsroot` → `jsloader_scan_apps` → `jsloader_run_app("weather")` 全链路
- **Files**: src/loader/*

### Phase 4: Tab UI + Config
- **Action**: 移植 `js_tab.c/h` + `config_util.c/h`，适配 jsroot/config.json
- **Validate**: JS-Apps tab dropdown + auto-start 功能
- **Files**: src/tab/*

### Phase 5: Integration Example + Docs
- **Action**: `examples/minimal_sdl/` 最小集成示例 + `docs/INTEGRATION.md`
- **Validate**: 第三方按文档操作可跑通
- **Files**: examples/, docs/

## Risks

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| LVGL API 表面积大，bridge 移植遗漏 | 中 | 从 lv_port_linux 已验证的 js_bridge.cpp 出发，按 widget 逐个移植 |
| quickjs/libuv 符号与宿主冲突 | 中 | `-fvisibility=hidden` + 仅导出 jsloader_* 前缀符号 |
| 跨 .so 边界 C++ 异常 | 中 | 所有公共 API 使用 `extern "C"`，内部 catch |
| txiki.js 升级时 submodule 同步 | 低 | 两个 repo 使用同一 tag，定期对齐 |
| ARM 工具链 shared lib 兼容性 | 低 | 已验证 xOS 工具链支持 -fPIC |

## Acceptance

- [ ] 5 个 .so 产出: jsloader, lvgljs_bridge, tjs, quickjs, uv
- [ ] `deps.cfg.local` 配置后 `cmake && make` 一次通过
- [ ] .so 依赖自动拷贝到 jsroot/lib/
- [ ] 分辨率路由: 800x1280/ → common/ fallback
- [ ] SDL minimal example 可启动 weather app
- [ ] 现有 lv_port_linux 功能不受影响
