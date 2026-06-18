/**
 * js-settings — jsroot config manager (2-pane layout)
 */
lvgljs.heartbeat(15000);
lvgljs.setInterval(3000, function () { lvgljs.heartbeat(); });
lvgljs.screenColor(0xF0F0F4);
lvgljs.setFPS(15);

var scr = lvgljs.getScreenSize();
var W = scr.w, M = 10;
var PASS = 0x50B86C, FAIL = 0xE74C3C, TEXT = 0x1A1A1A, SUB = 0x888888;
var SURFACE = 0xFFFFFF, ACCENT = 0x4A90D9, BORDER = 0xDDDDE0, BG2 = 0xF8F8FA;
var JSROOT = "/mnt/sdcard/jsroot";

/* header bar */
var titleBar = lvgljs.panel(0, 0, W, 42);
lvgljs.setBgColor(titleBar, SURFACE); lvgljs.setRadius(titleBar, 0); lvgljs.setBorder(titleBar, 0);
var titleT = lvgljs.label("JS Settings", M + 6, 10);
lvgljs.setFont(titleT, 18); lvgljs.setTextColor(titleT, TEXT);

/* exit button — on top of header, right-aligned */
var xBtn = lvgljs.btn("X", W - 44, 4, 36, 36);
lvgljs.setBgColor(xBtn, FAIL); lvgljs.setRadius(xBtn, 18); lvgljs.setBorder(xBtn, 0);
lvgljs.setTextColor(xBtn, 0xFFFFFF); lvgljs.setFont(xBtn, 16);
lvgljs.toFront(xBtn);
lvgljs.onClick(xBtn, function () { lvgljs.exit(); });

/* left panel — app list */
var LW = Math.floor(W * 0.38);
var leftBg = lvgljs.panel(M, 46, LW, scr.h - 56);
lvgljs.setBgColor(leftBg, SURFACE); lvgljs.setRadius(leftBg, 10); lvgljs.setBorder(leftBg, 1, BORDER);
var leftTitle = lvgljs.label("Apps", M + 10, 52);
lvgljs.setFont(leftTitle, 13); lvgljs.setTextColor(leftTitle, SUB);

/* right panel — config items */
var RX = M + LW + M;
var RW = W - RX - M;
var rightBg = lvgljs.panel(RX, 46, RW, scr.h - 56);
lvgljs.setBgColor(rightBg, SURFACE); lvgljs.setRadius(rightBg, 10); lvgljs.setBorder(rightBg, 1, BORDER);
var rightTitle = lvgljs.label("Config", RX + 10, 52);
lvgljs.setFont(rightTitle, 13); lvgljs.setTextColor(rightTitle, SUB);

/* right panel dynamic area */
var cfgY = 70;    /* y offset inside right panel */
var cfgWidgets = []; /* track widgets for cleanup */

function clearRight() {
    for (var i = 0; i < cfgWidgets.length; i++) {
        lvgljs.del(cfgWidgets[i]);
    }
    cfgWidgets = [];
    cfgY = 70;
}

function addWidget(wid) {
    cfgWidgets.push(wid);
}

function rightLabel(text, x, yy, color, size) {
    var c = color || TEXT;
    var s = size || 14;
    var l = lvgljs.label(text, RX + (x || 10), yy);
    lvgljs.setFont(l, s); lvgljs.setTextColor(l, c);
    addWidget(l);
    return l;
}

/* ================================================================
 * Data
 * ================================================================ */
var apps = JSON.parse(lvgljs.scanApps());
var configs = []; /* {name: "Global", path: "...", cfg: {...}} */

/* global config */
var gpath = JSROOT + "/config.json";
var gRaw = lvgljs.readFile(gpath);
var gCfg = gRaw ? JSON.parse(gRaw) : { auto_start_app: "none", log: { level: 1 } };
configs.push({ name: "Global", path: gpath, cfg: gCfg });

/* per-app configs — try config.json first, then <appname>.json */
for (var i = 0; i < apps.length; i++) {
    var app = apps[i];
    var p = JSROOT + "/apps/" + app.name + "/config.json";
    var r = lvgljs.readFile(p);
    if (!r || r === "") {
        p = JSROOT + "/apps/" + app.name + "/" + app.name + ".json";
        r = lvgljs.readFile(p);
    }
    if (r && r !== "") {
        configs.push({ name: app.name, path: p, cfg: JSON.parse(r) });
    }
}

/* ================================================================
 * Left panel — app list (panel + label, full color control)
 * ================================================================ */
var ly = 70;
var selected = -1;
var leftItems = []; /* {bg, label} */

function selectApp(idx) {
    for (var i = 0; i < leftItems.length; i++) {
        lvgljs.setBgColor(leftItems[i].bg, SURFACE);
        lvgljs.setBorder(leftItems[i].bg, 1, BORDER);
        lvgljs.setTextColor(leftItems[i].label, TEXT);
    }
    lvgljs.setBgColor(leftItems[idx].bg, ACCENT);
    lvgljs.setBorder(leftItems[idx].bg, 0);
    lvgljs.setTextColor(leftItems[idx].label, 0xFFFFFF);
    selected = idx;
    showConfig(idx);
}

for (var j = 0; j < configs.length; j++) {
    (function (idx) {
        var itemH = 38;
        var bg = lvgljs.panel(M + 6, ly, LW - 16, itemH);
        lvgljs.setBgColor(bg, SURFACE);
        lvgljs.setRadius(bg, 8);
        lvgljs.setBorder(bg, 1, BORDER);
        var lbl = lvgljs.label(configs[idx].name, M + 16, ly + 8);
        lvgljs.setFont(lbl, 14);
        lvgljs.setTextColor(lbl, TEXT);
        lvgljs.onClick(bg, function () { selectApp(idx); });
        lvgljs.onClick(lbl, function () { selectApp(idx); });
        leftItems.push({ bg: bg, label: lbl });
        ly += itemH + 6;
    })(j);
}

/* ================================================================
 * Right panel — show config
 * ================================================================ */
function showConfig(idx) {
    clearRight();
    var entry = configs[idx];
    var cfg = entry.cfg;
    var entryPath = entry.path;

    var nameLbl = lvgljs.label(entry.name, RX + 10, cfgY);
    lvgljs.setFont(nameLbl, 16); lvgljs.setTextColor(nameLbl, TEXT);
    addWidget(nameLbl);
    cfgY += 26;

    var keys = Object.keys(cfg);
    for (var k = 0; k < keys.length; k++) {
        (function (key) {
            var val = cfg[key];

            /* nested object? skip for now */
            if (typeof val === "object" && val !== null) {
                /* flatten: log.level -> show as sub-items */
                var subKeys = Object.keys(val);
                for (var sk = 0; sk < subKeys.length; sk++) {
                    (function (subKey) {
                        var subVal = val[subKey];
                        if (typeof subVal === "number") {
                            showNumberSlider(key + "." + subKey, subVal, function (v) {
                                val[subKey] = v;
                                lvgljs.writeFile(entryPath, JSON.stringify(cfg));
                            });
                        }
                    })(subKeys[sk]);
                }
                return;
            }

            if (typeof val === "boolean") {
                showBoolean(key, val, function (v) {
                    cfg[key] = v;
                    lvgljs.writeFile(entryPath, JSON.stringify(cfg));
                });
            } else if (typeof val === "number") {
                showNumberSlider(key, val, function (v) {
                    cfg[key] = v;
                    lvgljs.writeFile(entryPath, JSON.stringify(cfg));
                });
            } else {
                /* string */
                if (key === "auto_start_app") {
                    showAppDropdown(key, val, function (v) {
                        cfg[key] = v;
                        lvgljs.writeFile(entryPath, JSON.stringify(cfg));
                        lvgljs.setAutoStart(v);
                    });
                } else {
                    showTextBox(key, "" + val, function (v) {
                        cfg[key] = v;
                        lvgljs.writeFile(entryPath, JSON.stringify(cfg));
                    });
                }
            }
        })(keys[k]);
    }
}

/* ---- boolean -> switch ---- */
function showBoolean(key, val, onChange) {
    var y0 = cfgY;
    rightLabel(key, 10, cfgY + 4);
    var sw = lvgljs.sw(RX + RW - 70, cfgY);
    addWidget(sw);
    var swLbl = rightLabel(val ? "ON" : "OFF", RW - 50, cfgY + 4, val ? PASS : SUB);
    if (val) { lvgljs.setValue(sw, 1); }
    lvgljs.onChange(sw, function () {
        var v = lvgljs.getValue(sw) === 1;
        lvgljs.setText(swLbl, v ? "ON" : "OFF");
        lvgljs.setTextColor(swLbl, v ? PASS : SUB);
        onChange(v);
    });
    cfgY += 38;
}

/* ---- number -> slider ---- */
function showNumberSlider(key, val, onChange) {
    var y0 = cfgY;
    var min = 0, max = val * 3;
    if (max < 10) { max = 10; }
    if (key.indexOf("level") >= 0 || key.indexOf("Level") >= 0) { min = 0; max = 4; }
    if (key.indexOf("minute") >= 0 || key.indexOf("Minute") >= 0) { min = 1; max = 120; }
    if (key.indexOf("second") >= 0 || key.indexOf("Second") >= 0) { min = 1; max = 60; }

    rightLabel(key, 10, cfgY);
    cfgY += 18;
    var slW = RW - 60;
    var sl = lvgljs.slider(RX + 10, cfgY, slW, min, max, val);
    addWidget(sl);
    var slVal = rightLabel("" + val, RW - 50, cfgY - 2, ACCENT, 14);
    lvgljs.onChange(sl, function () {
        var v = lvgljs.getValue(sl);
        lvgljs.setText(slVal, "" + v);
        onChange(v);
    });
    cfgY += 36;
}

/* ---- string -> textbox ---- */
function showTextBox(key, val, onChange) {
    rightLabel(key, 10, cfgY);
    cfgY += 18;
    var tbW = RW - 20;
    var tb = lvgljs.textbox(RX + 10, cfgY, tbW, 30);
    lvgljs.setFont(tb, 14);
    lvgljs.setText(tb, val);
    addWidget(tb);
    lvgljs.onChange(tb, function () {
        onChange(lvgljs.getText(tb));
    });
    cfgY += 42;
}

/* ---- app dropdown ---- */
function showAppDropdown(key, val, onChange) {
    var y0 = cfgY;
    rightLabel(key, 10, cfgY);
    cfgY += 18;
    var optStr = "none";
    for (var a = 0; a < apps.length; a++) {
        optStr = optStr + "\n" + apps[a].name;
    }
    var dd = lvgljs.dropdown(RX + 10, cfgY, RW - 20, 32);
    lvgljs.dropdownSet(dd, optStr);
    addWidget(dd);
    /* select current */
    for (var s = 0; s < apps.length + 1; s++) {
        var on = s === 0 ? "none" : apps[s - 1].name;
        if (on === val) { lvgljs.setValue(dd, s); }
    }
    lvgljs.onChange(dd, function () {
        var sel = lvgljs.getDropdownSelected(dd);
        var v = sel === 0 ? "none" : apps[sel - 1].name;
        onChange(v);
    });
    cfgY += 44;
}

/* ================================================================
 * Initial: auto-select first config
 * ================================================================ */
if (configs.length > 0) {
    selectApp(0);
}

lvgljs.print("[js-settings] " + configs.length + " configs loaded");
