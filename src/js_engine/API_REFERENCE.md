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

---

## 2. 控件创建

所有创建函数返回一个 **整数 ID**，后续用此 ID 操作控件。

### 2.1 label — 文本标签
```js
var id = lvgljs.label(text, x, y);
```
| 参数 | 类型 | 说明 |
|------|------|------|
| text | string | 显示文字 |
| x, y | int | 位置（左上角） |

### 2.2 btn — 按钮
```js
var id = lvgljs.btn(text, x, y, w, h, [callback]);
```
| 参数 | 类型 | 说明 |
|------|------|------|
| text | string | 按钮文字 |
| x, y, w, h | int | 位置和大小 |
| callback | function | 可选，点击回调 |

### 2.3 textbox — 单行文本框
```js
var id = lvgljs.textbox(x, y, w, h);
```

### 2.4 image — 图片
```js
var id = lvgljs.image(path, x, y, w, h);
```
| 参数 | 类型 | 说明 |
|------|------|------|
| path | string | 图片文件路径（PNG/JPEG/BMP） |
| x, y, w, h | int | 位置和显示大小 |

**注意**：图片路径必须是 LVGL 文件系统可访问的绝对路径（如 `/mnt/sdcard/sphoto/skin/weather_sunny.png`）。需要 LVGL 配置启用相应图片解码器（`LV_USE_LODEPNG`、`LV_USE_LIBJPEG_TURBO` 等）。

### 2.5 panel — 容器面板
```js
var id = lvgljs.panel(x, y, w, h);
```
默认样式：白色背景 80% 透明度、18px 圆角、无边框。

### 2.6 sw — 开关
```js
var id = lvgljs.sw(x, y, [callback]);
```

### 2.7 slider — 滑动条
```js
var id = lvgljs.slider(x, y, w, [min, max, val, callback]);
```
默认 min=0, max=100, val=50。

### 2.8 checkbox — 复选框
```js
var id = lvgljs.checkbox(text, x, y);
```

### 2.9 arc — 圆弧
```js
var id = lvgljs.arc(x, y, size, [min, max, val]);
```

---

## 3. 样式设置

以下函数第一个参数均为控件 ID。

| 函数 | 参数 | 说明 |
|------|------|------|
| `setText(id, text)` | text: string | 设置文字内容（label/textarea/checkbox） |
| `setImage(id, path)` | path: string | 更换图片源 |
| `setTextColor(id, hex)` | hex: `0xRRGGBB` | 文字颜色 |
| `setBgColor(id, hex)` | hex: `0xRRGGBB` | 背景颜色 |
| `setFont(id, size)` | size: 12/14/16/18/20/22/24/28/30/36/48 | 字体大小 |
| `setRadius(id, r)` | r: int | 圆角半径 |
| `setOpacity(id, opa)` | opa: 0-255 | 背景透明度（255=不透明） |
| `setPos(id, x, y)` | x, y: int | 移动控件位置 |
| `setSize(id, w, h)` | w, h: int | 调整控件大小 |
| `setWidth(id, w)` | w: int | 设置宽度 |
| `setHeight(id, h)` | h: int | 设置高度 |
| `setBorder(id, width, color)` | width: int, color: hex | 边框宽度和颜色 |
| `setVisible(id, visible)` | visible: 0/1 | 显示/隐藏 |
| `setAlign(id, type)` | type: 0=左 1=中 2=右 | 文字对齐 |

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

**注意**：`setInterval` 基于 LVGL timer 实现，精度约为 30ms。适合 UI 刷新，不适合高精度计时。

---

## 7. 可用字体大小

12, 14, 16, 18, 20, 22, 24, 28, 30, 36, 48

---

## 8. 注意事项

### 8.1 控件 ID
控件 ID 从 0 开始递增，每次运行 JS 脚本都会重置。不要在 `setInterval` 回调中依赖外部捕获的 ID——确保 ID 在回调执行时仍然有效。

### 8.2 图片路径
图片必须使用设备上的绝对路径。ARM 设备上天气图标示例：
```
/mnt/sdcard/sphoto/skin/weather_sunny.png
/mnt/sdcard/sphoto/skin/weather_cloudy.png
/mnt/sdcard/sphoto/skin/weather_rainy.png
/mnt/sdcard/sphoto/skin/weather_snowy.png
/mnt/sdcard/sphoto/skin/weather_overcast.png
/mnt/sdcard/sphoto/skin/weather_sunny_small.png
/mnt/sdcard/sphoto/skin/weather_cloudy_small.png
/mnt/sdcard/sphoto/skin/weather_rainy_small.png
/mnt/sdcard/sphoto/skin/weather_snowy_small.png
/mnt/sdcard/sphoto/skin/weather_overcast_small.png
```

### 8.3 颜色格式
全部颜色使用 `0xRRGGBB` 十六进制格式，无 Alpha 通道。透明度通过 `setOpacity()` 单独设置。

### 8.4 定时器清理
`setInterval` 返回的 timerId 在退出 JS 应用时**自动清理**（`js_engine_cleanup()` 会删除所有 LVGL timer）。如果需要在运行中取消定时器，调用 `clearInterval(id)`。

### 8.5 JS 内置对象
QuickJS 支持以下标准内置对象：`Date`, `Math`, `JSON`, `Array`, `String`, `Object`, `Number`, `Boolean`, `parseInt`, `parseFloat`, `isNaN`。

txiki.js 额外提供：`setTimeout`, `setInterval`, `clearTimeout`, `clearInterval`。

**注意**：txiki.js 的 `setTimeout`/`setInterval` 基于 libuv 事件循环，而 `lvgljs.setInterval` 基于 LVGL timer。两者均可使用，但 LVGL timer 更可靠（与 UI 刷新同步）。

### 8.6 内存
每个控件和回调都会分配内存。控件总数限制 512 个，回调总数限制 256 个。超过限制会静默失败（返回 -1）。

### 8.7 错误处理
JS 异常会打印到 stderr 和 LVGL log。如果点击 JS app 后没有任何反应：
1. 检查终端输出的 `[js_engine] JS exception` 错误消息
2. 确认文件路径、函数名拼写正确
3. 确认使用的 API 函数存在（参考本文档）

### 8.8 返回和清理
点击右上角 ✕ 或调用 `lvgljs.exit()` 会触发完整的清理流程：
1. 停止所有 LVGL timer
2. 释放所有 JS 回调
3. 删除 JS screen 及其上的所有控件
4. 释放 QuickJS 运行时
5. 恢复上一个 screen（widgets demo）

控件和回调不需要手动释放。

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

### 9.2 带定时器的计数器
```js
lvgljs.screenColor(0x0d1b3e);
var count = 0;
var label = lvgljs.label("Count: 0", 30, 60);
lvgljs.setFont(label, 30);

lvgljs.setInterval(1000, function() {
    count++;
    lvgljs.setText(label, "Count: " + count);
});
```

### 9.3 Switch + Slider 联动
```js
lvgljs.screenColor(0x202020);
var sw = lvgljs.sw(30, 60);
var sl = lvgljs.slider(30, 120, 250, 0, 100, 50, function() {
    var val = lvgljs.getValue(sl);
    lvgljs.setText(valLabel, "Value: " + val);
});
var valLabel = lvgljs.label("Value: 50", 30, 170);
lvgljs.setTextColor(valLabel, 0xFFFFFF);
```

### 9.4 图片显示
```js
var img = lvgljs.image("/mnt/sdcard/img/photo.png", 50, 50, 300, 200);

// 点击更换图片
lvgljs.btn("Next", 50, 280, 100, 44, function() {
    lvgljs.setImage(img, "/mnt/sdcard/img/photo2.png");
});
```
