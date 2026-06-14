# lvgljs — JavaScript 编程指南

> **基于**: lv_port_linux + txiki/QuickJS + lvgljs bridge
> **LVGL 版本**: v9.6

---

## 1. 快速开始

### 1.1 App 目录结构

```
src/js_engine/apps/
├── my_app/
│   ├── index.js          # 入口（必须命名为 index.js）
│   └── icon.png          # 资源文件（可选）
└── LVGLJS_GUIDE.md       # 本文档
```

部署到设备后在 JS-Apps tab 自动发现。

### 1.2 最小 App

```javascript
var W = lvgljs.getScreenSize();
lvgljs.screenColor(0x1A1A2E);              // 屏幕背景色
lvgljs.btn("X", W.w - 52, 8, 44, 44,       // 退出按钮
    function() { lvgljs.exit(); });
lvgljs.hideBackButton();                    // 隐藏系统返回按钮
lvgljs.setFPS(20);                          // 设置刷新率
lvgljs.print("Hello from my app!");         // 打印到 log
```

### 1.3 资源文件路径

**使用相对路径**，资源文件放在 app 目录下：

```javascript
// 推荐：__dirname 指向当前 app 目录
var img = lvgljs.image(__dirname + "/icon.png", 100, 200, 48, 48);

// 读取/写入数据文件
var data = lvgljs.readFile(__dirname + "/data.txt");
lvgljs.writeFile(__dirname + "/save.txt", "content");
```

---

## 2. Widgets 参考

### 2.1 通用样式 — `lvgljs.style(id, prop, val)`

**一个函数替代所有样式设置**，无需每次新增 bridge 函数：

| prop | val | 说明 |
|------|-----|------|
| `"bg_color"` | hex (如 `0xFF6B6B`) | 背景色 |
| `"text_color"` | hex | 文字颜色 |
| `"radius"` | px | 圆角半径 |
| `"opacity"` | 0-255 | 透明度 |
| `"border_w"` | px | 边框宽度 |
| `"border_color"` | hex | 边框颜色 |
| `"pad_all"` | px | 内边距 |
| `"shadow_w"` | px | 阴影宽度 |
| `"shadow_ofs"` | px | 阴影偏移 |
| `"text_align"` | 0/1/2 | 左/中/右对齐 |
| `"font"` | px | 字号(14/16/18/22) |
| `"w"` / `"h"` | px | 宽/高 |
| `"x"` / `"y"` | px | 坐标 |
| `"visible"` | 0/1 | 显示/隐藏 |

```javascript
lvgljs.style(btn, "bg_color", 0xE74C3C);
lvgljs.style(btn, "radius", 8);
lvgljs.style(btn, "text_color", 0xFFFFFF);
```

### 2.2 专用方法（兼容保留）

| 方法 | 说明 |
|------|------|
| `setPos(id, x, y)` | 设置位置 |
| `setSize(id, w, h)` | 设置尺寸 |
| `setVisible(id, bool)` | 显示/隐藏 |
| `setBgColor(id, hex)` | 背景色 |
| `setOpacity(id, 0-255)` | 透明度 |
| `setRadius(id, r)` | 圆角半径 |
| `setBorder(id, width, hex)` | 边框宽度+颜色 |
| `setFont(id, ...)` | 设置字体 |
| `setTextColor(id, hex)` | 文字颜色 |
| `style(id, prop, val)` | **通用样式(推荐)** |

### 2.2 label — 文本标签

```javascript
var lbl = lvgljs.label("Hello World", x, y);
lvgljs.setText(lbl, "Updated text");
lvgljs.setTextColor(lbl, 0x4ECDC4);
lvgljs.setFont(lbl, 22);      // 字体大小
```

### 2.3 btn — 按钮

```javascript
var btn = lvgljs.btn("Click Me", x, y, w, h);
lvgljs.onClick(btn, function() {
    lvgljs.print("Button clicked!");
});
```

### 2.4 panel — 面板容器

```javascript
var p = lvgljs.panel(x, y, w, h);
lvgljs.setBgColor(p, 0x34495E);
lvgljs.setRadius(p, 8);
```

### 2.5 image — 图片

```javascript
// 支持 PNG/JPG（需启用 LV_USE_LODEPNG / LV_USE_LIBJPEG_TURBO）
var img = lvgljs.image(__dirname + "/photo.png", x, y, w, h);
lvgljs.setPos(img, newX, newY);
```

### 2.6 textbox — 文本输入框

```javascript
var tb = lvgljs.textbox(x, y, w, h);
lvgljs.setText(tb, "Default text");
var val = lvgljs.getValue(tb);
```

### 2.7 switch — 开关

```javascript
var sw = lvgljs.sw(x, y);
lvgljs.onChange(sw, function() {
    lvgljs.print("Switch toggled");
});
```

### 2.8 slider — 滑动条

```javascript
var sl = lvgljs.slider(x, y, min, max, value, w);
lvgljs.onChange(sl, function() {
    lvgljs.print("Slider value: " + lvgljs.getValue(sl));
});
```

### 2.9 checkbox — 复选框

```javascript
var cb = lvgljs.checkbox("Option", x, y);
lvgljs.onChange(cb, function() {
    lvgljs.print("Checkbox changed");
});
```

---

## 3. 事件系统

### 3.1 onClick — 点击

```javascript
lvgljs.onClick(widgetId, function() {
    // 点击逻辑
});
```

### 3.2 onChange — 值变化

```javascript
lvgljs.onChange(widgetId, function() {
    // 用于 slider/switch/checkbox
});
```

### 3.3 onPress / onRelease — 全局按压

```javascript
lvgljs.onPress(function() {
    // 屏幕任意位置按下
    pressTime = Date.now();
});

lvgljs.onRelease(function() {
    // 屏幕任意位置松开
    var elapsed = Date.now() - pressTime;
});
```

---

## 4. 定时器

```javascript
// 周期性定时器（毫秒）
var timerId = lvgljs.setInterval(100, function() {
    // 每 100ms 执行
});

// 清除定时器
lvgljs.clearInterval(timerId);
```

**重要**: `lvgljs.clearInterval(id)` 必须传入 `setInterval` 返回的 ID。

---

## 5. 文件操作

```javascript
// 读取（路径相对于 app 目录）
var content = lvgljs.readFile(__dirname + "/config.txt");

// 写入
lvgljs.writeFile(__dirname + "/save.txt", "data to save");

// 删除
lvgljs.deleteFile(__dirname + "/temp.txt");
```

---

## 6. 屏幕控制

```javascript
// 获取屏幕尺寸
var size = lvgljs.getScreenSize();  // {w: 800, h: 1280}

// 设置背景色
lvgljs.screenColor(0x1A1A2E);

// 设置刷新率（>30 无实际效果）
lvgljs.setFPS(20);

// 隐藏系统返回按钮
lvgljs.hideBackButton();

// 退出当前 App（返回 JS-Apps 列表）
lvgljs.exit();

// 打印日志
lvgljs.print("Debug message");
```

---

## 7. 最佳实践

### 7.1 坐标与布局

- 屏幕尺寸: `var W = lvgljs.getScreenSize()`, `W.w=800`, `W.h=1280`
- 使用**绝对坐标**定位（无 flex/grid 布局）
- 游戏区域避开底部 180px（D-pad 占用）

### 7.2 颜色使用

```javascript
// 推荐的游戏配色方案
var DARK_BG   = 0x0F0F23;  // 深色背景
var ACCENT    = 0x4ECDC4;  // 青绿强调色
var WARN      = 0xFF6B6B;  // 红色警告
var GOLD      = 0xF39C12;  // 金色分数
var MUTED     = 0x7F8C8D;  // 灰色次要文字
var BTN_BG    = 0x2C3E50;  // 按钮背景
var BTN_HOVER = 0xE74C3C;  // 按钮高亮
```

### 7.3 游戏循环

**推荐**: 使用单一持久定时器 + 时间戳节流

```javascript
var lastMove = 0;
lvgljs.setInterval(30, function() {
    var now = Date.now();
    if (now - lastMove < speed) return;  // 节流
    lastMove = now;
    // 游戏逻辑
});
```

**避免**: 反复 clearInterval/setInterval（容易堆积）

### 7.4 状态机

```javascript
var state = "IDLE";  // IDLE → CHARGING → JUMPING → IDLE

lvgljs.onPress(function() {
    if (state === "IDLE") { state = "CHARGING"; /* ... */ }
});
```

### 7.5 对象池

大量创建/销毁 LVGL widget 开销大，使用对象池预分配：

```javascript
var pool = [];
for (var i = 0; i < 200; i++) {
    var obj = lvgljs.panel(0, 0, 10, 10);
    lvgljs.setVisible(obj, 0);
    pool.push(obj);
}
// 使用时从池中取
```

### 7.6 内存管理

- JS 引擎自动 GC，不需要手动 free
- LVGL widget 通过 `lvgljs.exit()` 清理
- 尽量减少 `setVisible` 操作（每帧调用开销大）

---

## 8. 常见问题

### Q: 定时器速度不稳定？
**A**: 使用时间戳节流（见 7.3），不要依赖 setInterval 的精确性。

### Q: 中文显示为方框？
**A**: 需要启用 CJK 字体（已默认启用 `LV_FONT_SOURCE_HAN_SANS_SC_16_CJK`），在 label/btn 上设置：
```javascript
lvgljs.setFont(widget, 16);  // 使用内置字体
```

### Q: 为什么看不到我的 app？
**A**: 
1. 确认 `index.js` 在 `src/js_engine/apps/<app_name>/` 下
2. 部署后切换 JS-Apps tab（自动刷新）
3. 检查 `lvgljs.print` 输出确认加载

### Q: 如何调试？
**A**: 使用 `lvgljs.print()` 打印到串口/终端 log 查看。

---

## 9. Widget 样式对齐说明

本 bridge 的 widget 默认样式已调整为接近 lv_binding_js 视觉效果：

| Widget | 默认样式 |
|--------|---------|
| label | 白色文字, Montserrat 16, 黑色背景上可见 |
| btn | 8px 圆角, 阴影, 白色文字, 主题色背景 |
| panel | 18px 圆角, 白色半透明背景, 无边框 |
| switch | 默认 LVGL 主题样式 |
| slider | 默认 LVGL 主题样式 |
| image | 直接解码显示 PNG/JPG |

如需完全一致的 lv_binding_js 渲染，需移植其 React 组件系统（非本 bridge 范围）。

---

> **维护**: lv_port_linux 项目
> **更新**: 2026-06
