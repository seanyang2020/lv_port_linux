# JS-Apps 部署指南

## 概述

`jsroot/` 是 lv_jsloader 的运行时部署包，包含 JavaScript 应用程序、QuickJS 引擎和 LVGL 桥接层。
xkphoto 的 settingpad 通过 dlopen 在运行时加载，无需重新编译固件即可更新 JS 应用。

## 目录结构

```
jsroot/
├── config.json              # 运行时配置（日志级别、自动启动等）
├── logs/                    # 崩溃日志（运行时自动生成）
├── lib/                     # .so 文件（引擎 + 桥接）
│   ├── libjsloader.so       # JS 加载器主入口
│   ├── liblvgljs_bridge.so  # JS <-> LVGL API 桥接
│   └── libjs_tjs.so         # QuickJS 引擎
└── apps/                    # JS 应用程序
    ├── common/
    │   ├── js-launcher/     # 应用列表（卡片式 UI）
    │   └── hello/           # Hello World 示例
    ├── calculator/          # 计算器
    ├── weather/             # 天气应用
    ├── snake/               # 贪吃蛇
    ├── jump/                # 跳一跳
    └── sudoku/              # 数独
```

## 部署步骤

### 1. 推送 jsroot 到设备

```bash
adb push jsroot/ /mnt/sdcard/jsroot/
```

### 2. 验证部署

```bash
adb shell ls /mnt/sdcard/jsroot/lib/
adb shell ls /mnt/sdcard/jsroot/apps/
```

### 3. 配置（可选）

编辑 `/mnt/sdcard/jsroot/config.json`：

```json
{
  "auto_start_app": "none",
  "log": {
    "level": 1
  }
}
```

日志级别：0=关, 1=错误(默认), 2=警告, 3=信息, 4=调试

### 4. 启动

xkphoto 固件需开启 `CONFIG_XOS_USE_APP_JSLOADER=y`。settingpad 启动后自动检测 `/mnt/sdcard/jsroot/` 目录，存在则在左侧菜单显示 "JS-Apps" 入口（位于 WIFI 和关于之间）。

## 添加新应用

在 `apps/` 下创建目录，放入 `index.js`：

```
jsroot/apps/<app_name>/index.js
```

js-launcher 会自动扫描并显示在应用列表中。

## 更新应用

直接替换 `apps/<app_name>/index.js`，重启 settingpad 即可生效。无需重新编译固件。

## 编译 jsloader

参见 lv_jsloader 工程的 `scripts/build-xkphoto.sh`：

```bash
cd lv_jsloader
./scripts/build-xkphoto.sh
# 产物: build-xos/lib*.so  -> 拷贝到 jsroot/lib/
#       jsroot-xos/          -> 完整部署包
```

## 构建依赖

| 依赖 | 路径 |
|------|------|
| ARM 工具链 | qmenv qm10xd |
| xkphoto LVGL 头文件 | `~/xos/core/package/guiengine/lvgl-v9` |
| LVGL 配置 | `~/xos/core/board/generic/qm10xd/conf/lv_conf.h` |
| FS 驱动字母 | `LVGLJS_FS_LETTER=H` |

## 故障排查

| 问题 | 检查项 |
|------|--------|
| 菜单不显示 | `CONFIG_XOS_USE_APP_JSLOADER=y` 是否开启 |
| 菜单显示但无应用 | `/mnt/sdcard/jsroot/lib/libjsloader.so` 是否存在 |
| 点击无响应 | 日志级别设为 4 查看调试输出 |
| 图片不显示 | 图片路径是否正确（不带 `/` 前缀，如 `mnt/sdcard/...`） |
