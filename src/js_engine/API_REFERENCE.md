# lvgljs JavaScript API Reference

`lvgljs` 是嵌入 LVGL 中的 JavaScript 运行时接口，所有函数通过全局 `lvgljs` 对象访问。

---

## 1. 基础工具

| 函数 | 参数 | 返回 | 说明 |
|------|------|------|------|
| `print(...msg)` | 任意数量参数 | — | 输出日志到 LVGL log |
| `screenColor(hex)` | hex: `0xRRGGBB` | — | 设置屏幕背景色 |
| `getScreenSize()` | — | `{w, h}` | 获取屏幕宽高 |
| `exit()` | — | — | 退出当前 JS 应用，返回列表 |
| `hideBackButton()` | — | — | 隐藏系统返回按钮（JS 自有退出UI时调用） |
| `getEnv(name, def)` | name: 变量名, def: 默认值 | string | 读取进程环境变量 |

### 1.1 全局变量

| 变量 | 类型 | 说明 |
|------|------|------|
| `__dirname` | string | 当前 JS 文件所在目录的绝对路径 |

---

## 2. 控件创建

所有创建函数返回一个 **整数 ID**，后续用此 ID 操作控件。

**最后可选参数 `parentId`**：所有控件创建函数末尾可追加一个整数参数，指定父容器 ID。子控件坐标相对于父容器左上角。

### 2.1 label — 文本标签
```js
var id = lvgljs.label(text, x, y, [parentId]);
```

### 2.2 btn — 按钮
```js
var id = lvgljs.btn(text, x, y, w, h, [callback], [parentId]);
```
| 参数 | 类型 | 说明 |
|------|------|------|
| text | string | 按钮文字 |
| x, y, w, h | int | 位置和大小 |
| callback | function | 可选，点击回调 |

### 2.3 textbox — 单行文本框
```js
var id = lvgljs.textbox(x, y, w, h, [parentId]);
```

### 2.4 image — 图片
```js
var id = lvgljs.image(path, x, y, w, h, [parentId]);
```
图片路径使用 `__dirname + "/skin/xxx.png"` 实现自包含单包部署。

### 2.5 panel — 容器面板
```js
var id = lvgljs.panel(x, y, w, h, [parentId]);
```
默认样式：白色背景 80% 透明度、18px 圆角、无边框。

### 2.6 sw — 开关
```js
var id = lvgljs.sw(x, y, [callback], [parentId]);
```

### 2.7 slider — 滑动条
```js
var id = lvgljs.slider(x, y, w, [min, max, val, callback], [parentId]);
```
默认 min=0, max=100, val=50。

### 2.8 checkbox — 复选框
```js
var id = lvgljs.checkbox(text, x, y, [parentId]);
```

### 2.9 arc — 圆弧
```js
var id = lvgljs.arc(x, y, size, [min, max, val], [parentId]);
```

---

## 3. 样式设置

以下函数第一个参数均为控件 ID。

| 函数 | 参数 | 说明 |
|------|------|------|
| `setText(id, text)` | text: string | 设置文字内容 |
| `setImage(id, path)` | path: string | 更换图片源 |
| `setTextColor(id, hex)` | hex: `0xRRGGBB` | 文字颜色 |
| `setBgColor(id, hex)` | hex: `0xRRGGBB` | 背景颜色 |
| `setFont(id, size\|name)` | int 或 "cjk" | 字体大小(12~48) 或 CJK 中文字体 |
| `setRadius(id, r)` | r: int | 圆角半径 |
| `setOpacity(id, opa)` | opa: 0-255 | 背景透明度（255=不透明） |
| `setPos(id, x, y)` | x, y: int | 移动控件位置 |
| `setSize(id, w, h)` | w, h: int | 调整控件大小 |
| `setWidth(id, w)` | w: int | 设置宽度 |
| `setHeight(id, h)` | h: int | 设置高度 |
| `setBorder(id, width, color)` | width: int, color: hex | 边框宽度和颜色 |
| `setVisible(id, visible)` | visible: 0/1 | 显示/隐藏 |
| `setAlign(id, type)` | type: 0=左 1=中 2=右 | 文字对齐 |
| `toFront(id)` | — | 将控件提到最上层（解决面板遮挡） |

---

## 4. 取值

| 函数 | 返回 | 说明 |
|------|------|------|
| `getValue(id)` | int | 获取值（slider/switch/arc/checkbox） |
| `getText(id)` | string | 获取文字内容 |

---

## 5. 事件

| 函数 | 参数 | 说明 |
|------|------|------|
| `onClick(id, callback)` | callback: function | 为已有控件绑定点击事件 |
| `onChange(id, callback)` | callback: function | 为已有控件绑定值变化事件 |

---

## 6. 定时器

| 函数 | 参数 | 返回 | 说明 |
|------|------|------|------|
| `setInterval(ms, callback)` | ms: 毫秒, callback: function | timerId | 周期性定时器 |
| `clearInterval(id)` | id: timerId | — | 取消定时器 |

精度约 30ms，适合 UI 刷新。退出时自动清理，无需手动释放。

---

## 7. 可用字体

| 值 | 字体 |
|------|------|
| 12, 14, 16, 18, 20, 22, 24, 28, 30, 36, 48 | Montserrat（西文） |
| `"cjk"` | 思源黑体 CJK（中文） |

---

## 8. 应用开发指南

### 8.1 自包含单包部署

每个应用是 `JS_APPS_DIR` 下的一个子目录：

```
weather/
├── index.js        ← 入口文件
└── skin/           ← 图片等资源（通过 __dirname 相对引用）
    ├── weather_sunny.png
    └── ...
```

JS 中通过 `__dirname` 获取自身目录，拼接资源路径：
```js
var SKIN = __dirname + "/skin/";
lvgljs.image(SKIN + "weather_sunny.png", x, y, w, h);
```

部署只需复制整个目录到设备：
```bash
cp -r src/js_engine/apps/weather /mnt/sdcard/js-app/weather
```

### 8.2 系统返回按钮与退出设计

| 原则 | 说明 |
|------|------|
| **默认安全网** | 每个 JS 屏幕右上角自动显示半透明 ✕ 系统返回按钮 |
| **JS 声明接管** | 应用提供自己的退出 UI 后，调用 `lvgljs.hideBackButton()` 隐藏系统按钮 |
| **崩溃兜底** | 如果 JS 未调用 `hideBackButton()`，系统按钮始终可用 |

**推荐模式**：
```js
// 1. 创建自己的关闭按钮（放在最后，toFront 确保可点击）
var closeBtn = lvgljs.btn("X", W.w-56, 8, 48, 48, function() {
    lvgljs.exit();
});
lvgljs.toFront(closeBtn);

// 2. 隐藏系统按钮
lvgljs.hideBackButton();
```

**注意**：关闭按钮必须 `toFront()` 或在所有面板之后创建，否则会被大面板遮挡导致点击无效。

### 8.3 中英文双语

通过环境变量 `LANG` 切换语言，JS 无需修改：

```bash
export LANG=zh_CN.UTF-8   # 中文
export LANG=en_US.UTF-8   # 英文
```

```js
var L = lvgljs.getEnv("LANG", "en").indexOf("zh") >= 0 ? "zh" : "en";
var font = (L === "zh") ? "cjk" : 16;
```

### 8.4 颜色格式

全部颜色使用 `0xRRGGBB` 十六进制格式，无 Alpha 通道。透明度通过 `setOpacity()` 单独设置。

### 8.5 内存限制

控件总数 512，回调总数 256。超限静默失败（返回 -1）。退出时全部自动释放。

### 8.6 错误排查

JS 异常打印到终端 `[js_engine] JS exception`。开启调试日志：

```c
// src/js_engine/js_tab.h 第 17 行
#define JS_DEBUG 1   // 设为 0 关闭
```

调试日志前缀 `[js-debug]`，覆盖：启动、退出、定时器触发、回调执行。

---

## 9. 示例

### 9.1 Hello World
```js
lvgljs.screenColor(0x1a1a2e);
lvgljs.label("Hello World!", 30, 60);
lvgljs.btn("Click Me", 30, 150, 140, 48, function() {
    lvgljs.print("clicked!");
});
```

### 9.2 带关闭按钮的完整应用
```js
var W = lvgljs.getScreenSize();
lvgljs.screenColor(0x0d1b3e);

// 自建关闭按钮（最后创建，toFront 确保不被遮挡）
var close = lvgljs.btn("X", W.w-56, 8, 48, 48, function() { lvgljs.exit(); });
lvgljs.setRadius(close, 24);
lvgljs.setBgColor(close, 0x333333);
lvgljs.setOpacity(close, 120);
lvgljs.toFront(close);
lvgljs.hideBackButton();

// 应用内容...
lvgljs.label("My App", 30, 60);
```

### 9.3 带定时器的计数器
```js
var count = 0;
var label = lvgljs.label("Count: 0", 30, 60);
lvgljs.setInterval(1000, function() {
    count++;
    lvgljs.setText(label, "Count: " + count);
});
```

### 9.4 嵌套布局
```js
var panel = lvgljs.panel(20, 60, 300, 200);
lvgljs.label("Title", 10, 10, panel);    // 子控件，坐标相对 panel
lvgljs.btn("OK", 10, 50, 80, 40, function() {
    lvgljs.print("clicked");
}, panel);                               // parent 放在最后
```

### 9.5 中英文双语
```js
var L = lvgljs.getEnv("LANG", "en").indexOf("zh") >= 0 ? "zh" : "en";
var T = {
    zh: { hello: "你好" },
    en: { hello: "Hello" }
};
lvgljs.label(T[L].hello, 30, 60);
lvgljs.setFont(id, L === "zh" ? "cjk" : 16);
```
