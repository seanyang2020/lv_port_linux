# Plan: JS 应用集成到 LVGL Tab

**Source**: 用户需求 — 将 `lv_binding_js` 的 JS 引擎集成到 `lv_port_linux`，作为 tab 项加载和运行 JS 应用
**Complexity**: Large

## 设计约束

### 约束 1: 代码独立管理

JS 引擎所有代码集中在 `src/js_engine/` 目录下，自包含、不侵入现有模块：

```
src/js_engine/          ← 唯一的 JS 相关代码目录
├── js_bridge.h         ← C 兼容头文件（extern "C"）
├── js_bridge.cpp       ← 引擎初始化/执行/清理/tick
├── js_tab.h            ← Tab UI 头文件
├── js_tab.c            ← 应用列表、执行、返回逻辑
└── apps/               ← 预编译 JS 应用 bundle
    ├── hello_world/index.js
    ├── calculator/index.js
    └── widgets/index.js
```

- 对现有文件的修改仅限于 **集成点**：`main.c`（2-3 行条件调用）、`CMakeLists.txt`（条件编译块）、`lv_conf.defaults`（1 个宏开关）
- `src/lib/`、`src/baidu_pan/` 等现有模块**不受任何影响**

### 约束 2: Feature 宏控制

所有 JS 引擎代码通过 `LV_USE_JS_ENGINE` 宏进行编译期开关控制：

- `lv_conf.defaults` 中定义 `LV_USE_JS_ENGINE 0`（默认关闭）
- CMakeLists.txt 中所有 JS 相关源文件、子目录、链接库均条件编译
- C/C++ 源码中所有 JS 路径均用 `#if LV_USE_JS_ENGINE` 守卫
- 当 `LV_USE_JS_ENGINE=0` 时，编译产物与集成前**完全一致**，零开销

## Summary

将 lv_binding_js 中的 QuickJS (txiki.js) JS 引擎以独立模块形式嵌入到 lv_port_linux 项目中，通过 `LV_USE_JS_ENGINE` feature 宏控制。创建一个新的 "JS Apps" tab 页面，支持扫描指定目录下的 JS 应用并枚举为列表，点击执行 JS 脚本，执行完支持返回到应用列表。

## 架构分析

### 两个项目的结构对比

| 方面 | lv_port_linux (目标) | lv_binding_js (来源) |
|------|---------------------|---------------------|
| 语言 | C | C++ (C++14) |
| 入口 | `main()` → `lv_demo_widgets()` | `main()` → `TJS_Run()` (libuv 事件循环) |
| 渲染 | `lv_timer_handler()` 在 backend loop 中 | `uv_timer` 回调 `lv_timer_handler()` |
| 构建 | CMake, 生成 `lvglsim` | CMake, 生成 `lvgljs` |
| 依赖 | lvgl, display backends | lvgl, txiki.js (QuickJS+libuv), lv_drivers |

### 关键挑战

1. **双事件循环冲突**：lv_binding_js 使用 libuv 事件循环（`TJS_Run()` 是阻塞调用），而 lv_port_linux 使用 backend 的事件循环。直接调用 `TJS_Run()` 会阻塞整个程序。

2. **main() 冲突**：两个项目各有自己的 `main()` 函数，`engine.cpp` 中的 `main()` 会初始化 QuickJS、LVGL 并进入 libuv 循环。

3. **JS 构件系统**：JS 应用使用 React/JSX 语法，需要 esbuild 打包为 `index.js`。lvgljs 在启动时加载指定的 JS bundle 文件。

4. **依赖引入**：需要将 txiki.js (QuickJS + libuv) 作为子模块引入 lv_port_linux。

## 方案选择

**嵌入式引擎模式**

不运行完整的 txiki.js 事件循环，而是：
- 初始化 QuickJS runtime（不启动 libuv loop）
- 手动调用 `JS_EvalFunction` / `JS_Call` 执行 JS
- 将 libuv loop 的一步集成到 LVGL 的 `lv_timer` 中
- 每次 tick 调用 `uv_run(loop, UV_RUN_NOWAIT)` 处理 pending 事件

优点：完全嵌入 LVGL 架构，无需修改现有事件循环；通过 feature 宏可完全禁用
要点：需要修改 engine.cpp 去除 `main()`，支持嵌入模式

## Patterns to Mirror

| Category | Source | Pattern |
|----------|--------|---------|
| Feature 宏 | `lv_conf.defaults:28` | `LV_USE_LINUX_FBDEV 1` — 布尔开关风格 |
| CMake 条件 | `CMakeLists.txt:79` | `if (CONFIG_LV_USE_EVDEV)` — 条件编译块 |
| 独立模块 | `src/baidu_pan/` | 功能代码集中在独立子目录 |
| C/C++ 边界 | `src/baidu_pan/baidu_oauth.h:20` | `extern "C"` 包装 C++ 实现，暴露 C 接口 |
| LVGL Screen | lvgl demos | `lv_obj_create(NULL)` 创建独立屏幕 |
| LVGL List | LVGL list widget | `lv_list` 创建可点击列表 |

## Files to Change

### 新建文件（全部在 `src/js_engine/` 下）

| File | Action | Why |
|------|--------|-----|
| `src/js_engine/js_bridge.h` | CREATE | JS 引擎 C 兼容接口头文件 |
| `src/js_engine/js_bridge.cpp` | CREATE | 引擎初始化、JS 执行、tick 驱动、清理 |
| `src/js_engine/js_tab.h` | CREATE | Tab UI 接口声明 |
| `src/js_engine/js_tab.c` | CREATE | 应用列表枚举、点击执行、返回逻辑 |
| `src/js_engine/apps/*/index.js` | CREATE | 预编译 JS bundle 资源文件 |

### 修改文件（仅集成点，受 feature 宏守卫）

| File | Action | Why |
|------|--------|-----|
| `lv_conf.defaults` | APPEND 1 行 | `LV_USE_JS_ENGINE 0` feature 宏 |
| `CMakeLists.txt` | APPEND ~20 行 | `if(LV_USE_JS_ENGINE)` 条件编译块 |
| `src/main.c` | APPEND ~5 行 | `#if LV_USE_JS_ENGINE` 添加 js_tab 调用 |

### 需要引入的子模块

| Submodule | Source | Path in project |
|-----------|--------|-----------------|
| txiki.js | `lv_binding_js/deps/txiki` | `deps/txiki` |
| lv_drivers | `lv_binding_js/deps/lv_drivers` | `deps/lv_drivers` |

## Implementation Phases

### Phase 1: 构建系统集成（Feature 宏 + CMake）

**任务 1.1**: 添加 feature 宏
- 在 `lv_conf.defaults` 末尾追加：
  ```
  # JS Engine — run JavaScript LVGL apps
  LV_USE_JS_ENGINE 0
  ```
- 在 `lv_conf.defaults` 顶部注释中说明此宏的作用
- 默认值为 0（关闭），需要时手动改为 1

**任务 1.2**: 添加子模块
- `git submodule add <txiki-url> deps/txiki`
- `git submodule add <lv_drivers-url> deps/lv_drivers`
- 或从本地 `lv_binding_js/deps/` 复制 commit hash，保持版本一致

**任务 1.3**: 更新 CMakeLists.txt（条件编译块）
```cmake
# ---- JS Engine (feature-gated) ----
if(LV_USE_JS_ENGINE)
    message("Including JS Engine support")
    set(CMAKE_CXX_STANDARD 14)
    enable_language(CXX)

    add_subdirectory(deps/txiki EXCLUDE_FROM_ALL)
    add_subdirectory(deps/lv_drivers EXCLUDE_FROM_ALL)

    # JS Engine sources (from src/js_engine/)
    file(GLOB JS_ENGINE_SRC src/js_engine/*.cpp src/js_engine/*.c)
    
    target_sources(lvglsim PRIVATE ${JS_ENGINE_SRC})
    target_include_directories(lvglsim PRIVATE
        deps/txiki/src
        deps/lv_drivers
        src/js_engine)
    target_link_libraries(lvglsim PUBLIC tjs lv_drivers)
endif()
```
- 关键：整个 block 在 `LV_USE_JS_ENGINE=0` 时不执行，零影响

### Phase 2: JS 引擎桥接层

**任务 2.1**: 创建 `js_bridge.h` — C 兼容接口
```c
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"
#include "lv_conf.h"  // LV_USE_JS_ENGINE

#if LV_USE_JS_ENGINE

int  js_engine_init(void);
int  js_engine_run_script(const char *script_path);
void js_engine_tick(void);
void js_engine_cleanup(void);
int  js_engine_is_running(void);

#endif /* LV_USE_JS_ENGINE */

#ifdef __cplusplus
}
#endif
```

**任务 2.2**: 创建 `js_bridge.cpp` — 引擎实现
- 从 `lv_binding_js/src/engine/engine.cpp` 移植核心代码
- 移除原有的 `main()` 函数
- `js_engine_init()`: 初始化 QuickJS runtime、创建 JS context、注册 lvgljs 全局对象、初始化 NativeRender
- `js_engine_tick()`: 调用 `uv_run(UV_RUN_NOWAIT)` + `lv_timer_handler()`
- `js_engine_run_script()`: 读取 JS 文件，调用 `TJS_Eval` 执行
- `js_engine_cleanup()`: 释放 JS 对象和 runtime，清理 lv_drivers 资源
- HAL 层处理：跳过 lv_binding_js 的 `hal_init()`，复用 lv_port_linux 已有的 display backend

**任务 2.3**: 创建 `js_tab.h` / `js_tab.c` — Tab UI
```c
#if LV_USE_JS_ENGINE

lv_obj_t * lv_js_tab_create(lv_obj_t * parent);

#endif
```
- 扫描 `JS_APPS_DIR`（编译宏或运行时参数指定）下的子目录
- 若子目录包含 `index.js`，视为一个应用
- 用 `lv_list` 列出应用名
- 每项绑定回调：点击 → 执行该应用的 JS

**任务 2.4**: JS 执行与返回流程
```
[应用列表 screen]
    │ 点击某项
    ▼
[创建新 screen] → js_engine_run_script("path/to/index.js")
    │ JS 代码渲染 LVGL UI 到新 screen
    │ 注册返回按钮（物理按键/UI 按钮）
    │ 每 30ms: js_engine_tick() 驱动 libuv
    │
    ▼ 点击返回
[js_engine_cleanup()] → 删除 JS screen → 回到列表 screen
```

### Phase 3: 主程序集成（最小侵入）

**任务 3.1**: 修改 `main.c`
```c
#if LV_USE_JS_ENGINE
#include "src/js_engine/js_tab.h"
#endif

// 在 main() 中，替换原本的单一 demo 为 tab 结构：
#if LV_USE_JS_ENGINE
    lv_obj_t * tabview = lv_tabview_create(lv_screen_active());
    lv_obj_t * tab_demo = lv_tabview_add_tab(tabview, "Demos");
    lv_obj_t * tab_js   = lv_tabview_add_tab(tabview, "JS Apps");
    
    // Widgets demo 嵌入 tab_demo
    // JS 应用列表嵌入 tab_js
    lv_js_tab_create(tab_js);
#else
    lv_demo_widgets();
    lv_demo_widgets_start_slideshow();
#endif
```

**任务 3.2**: 注册 JS tick timer
- 创建 `lv_timer` 每 30ms 调用 `js_engine_tick()`
- 仅在 `js_engine_is_running()` 为 true 时活跃
- JS 返回后自动删除 timer

### Phase 4: JS 应用资源

**任务 4.1**: 复制预编译 JS bundle
- 从 `lv_binding_js/demo/*/index.js` 复制到 `src/js_engine/apps/*/index.js`
- 不需要在项目构建时重新打包（使用上游预编译结果）
- 资源文件路径通过 CMake `JS_APPS_DIR` 宏定义

**任务 4.2**: 运行时路径配置
```cmake
# CMakeLists.txt 中定义默认搜索路径
target_compile_definitions(lvglsim PRIVATE 
    JS_APPS_DIR="${CMAKE_SOURCE_DIR}/src/js_engine/apps")
```

## 宏守卫策略汇总

| 位置 | 守卫方式 | 影响范围 |
|------|---------|---------|
| `lv_conf.defaults` | `LV_USE_JS_ENGINE 0` | 宏定义 |
| `CMakeLists.txt` | `if(LV_USE_JS_ENGINE)` | 整个 JS 构建块 |
| `js_bridge.h` | `#if LV_USE_JS_ENGINE` | 所有函数声明 |
| `js_bridge.cpp` | 文件顶层 `#if` 或 CMake 条件编译 | 整个 .cpp |
| `js_tab.h` | `#if LV_USE_JS_ENGINE` | 所有函数声明 |
| `js_tab.c` | 文件顶层 `#if` 或 CMake 条件编译 | 整个 .c |
| `main.c` | `#if LV_USE_JS_ENGINE` | 仅集成调用点 |

## Validation

```bash
# 验证 1: 默认关闭构建（应与集成前完全一致）
cmake -B build -DCONFIG=default
cmake --build build
./build/bin/lvglsim   # 确认行为不变，正常运行 widgets demo

# 验证 2: 开启 JS 引擎构建
# 将 lv_conf.defaults 中 LV_USE_JS_ENGINE 改为 1
cmake -B build -DCONFIG=default
cmake --build build
# 确认无编译/链接错误

# 验证 3: 功能验证（模拟器环境）
./build/bin/lvglsim
# 1. 启动后看到 tab 页面，包含 "JS Apps" tab
# 2. JS Apps tab 列出 hello_world / calculator / widgets
# 3. 点击 hello_world → 显示蓝色背景 + 红色按钮
# 4. 点击 calculator → 显示可交互计算器
# 5. 点击 widgets → 显示带 tabs + 主题切换的 widgets demo
# 6. 返回按钮 → 回到应用列表

# 验证 4: 关闭宏后再构建
# LV_USE_JS_ENGINE 改为 0
cmake -B build -DCONFIG=default
cmake --build build
# 产物应与验证 1 完全一致（bit-identical 或等价）
```

## Risks

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| libuv 事件循环与 LVGL backend 循环冲突 | HIGH | `UV_RUN_NOWAIT` 非阻塞模式，单步驱动 |
| JS 执行期间阻塞 LVGL 渲染 | HIGH | tick 函数 <10ms，不调用 `UV_RUN_DEFAULT` |
| lv_drivers SDL 依赖与主项目 SDL backend 冲突 | MEDIUM | 条件编译，优先复用主项目 backend |
| JS bundle 文件过大（600KB+），加载慢 | MEDIUM | 首版接受；后续考虑 QuickJS 字节码编译 |
| C/C++ ABI 兼容性问题 | LOW | `extern "C"` 包装所有跨语言接口 |
| 内存不足 | MEDIUM | 限制 JS heap size；退出时完整释放 |
| 子模块版本不兼容 LVGL 版本 | MEDIUM | 锁定与 lv_binding_js 相同 commit |

## Acceptance

- [ ] `LV_USE_JS_ENGINE=0` 时构建产物与集成前一致
- [ ] `LV_USE_JS_ENGINE=1` 时 CMake 构建成功，无编译/链接错误
- [ ] 程序启动后显示包含 "JS Apps" tab 的界面
- [ ] JS 应用列表正确枚举 `apps/` 下所有应用
- [ ] 点击 hello_world 执行 JS 并显示 UI
- [ ] 点击 calculator 执行并显示可交互计算器
- [ ] 点击 widgets 执行并显示 widgets demo
- [ ] 返回功能正常，回到应用列表，内存正确释放
- [ ] 所有 JS 代码严格隔离在 `src/js_engine/` 目录下
- [ ] 现有文件修改仅限于 `main.c`、`CMakeLists.txt`、`lv_conf.defaults`
