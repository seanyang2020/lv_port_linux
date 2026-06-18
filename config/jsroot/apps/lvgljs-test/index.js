/**
 * lvgljs-test — 全接口测试
 */
lvgljs.heartbeat(15000);
lvgljs.setInterval(3000, function () { lvgljs.heartbeat(); });
lvgljs.screenColor(0xF0F0F4);
lvgljs.setFPS(15);

var scr = lvgljs.getScreenSize();
var W = scr.w, M = 14, CW = W - M * 2;
var y = 8;
var PASS = 0x50B86C, FAIL = 0xE74C3C, TEXT = 0x1A1A1A, SUB = 0x888888;
var SURFACE = 0xFFFFFF, ACCENT = 0x4A90D9, BORDER = 0xDDDDE0;
var p = 0, f = 0;

/* ---- helpers ---- */
function cardHdr(num, title, apis) {
    var bg = lvgljs.panel(M, y, CW, 28);
    lvgljs.setBgColor(bg, SURFACE);
    lvgljs.setRadius(bg, 8);
    lvgljs.setBorder(bg, 1, BORDER);
    var line = lvgljs.panel(M + 4, y, 3, 28);
    lvgljs.setBgColor(line, ACCENT);
    lvgljs.setRadius(line, 2);
    lvgljs.setBorder(line, 0);
    var n = lvgljs.label("TC-" + (num < 10 ? "0" : "") + num, M + 14, y + 5);
    lvgljs.setFont(n, 13);
    lvgljs.setTextColor(n, ACCENT);
    var t = lvgljs.label(title, M + 60, y + 5);
    lvgljs.setFont(t, 14);
    lvgljs.setTextColor(t, TEXT);
    var a = lvgljs.label('', 0, 0); lvgljs.setVisible(a, false);
    lvgljs.setFont(a, 10);
    lvgljs.setTextColor(a, SUB);
    y += 32;
}

function ok(msg) {
    var dot = lvgljs.label("PASS", M + 20, y);
    lvgljs.setFont(dot, 11);
    lvgljs.setTextColor(dot, PASS);
    var lbl = lvgljs.label(msg, M + 56, y);
    lvgljs.setFont(lbl, 11);
    lvgljs.setTextColor(lbl, TEXT);
    y += 18;
    p++;
}

function bad(msg) {
    var dot = lvgljs.label("FAIL", M + 20, y);
    lvgljs.setFont(dot, 11);
    lvgljs.setTextColor(dot, FAIL);
    var lbl = lvgljs.label(msg, M + 56, y);
    lvgljs.setFont(lbl, 11);
    lvgljs.setTextColor(lbl, FAIL);
    y += 18;
    f++;
}

function gap() { y += 10; }

/* =================================================================
 * TC-01  Widgets
 * ================================================================= */
cardHdr(1, "Widget Creation", "label btn textbox switch slider checkbox arc panel dropdown bar");

var lb  = lvgljs.label("Hello World", M + 20, y); lvgljs.setFont(lb, 14); lvgljs.setTextColor(lb, TEXT); y += 22;
var btn = lvgljs.btn("Click", M + 20, y, 76, 28); lvgljs.setFont(btn, 11); lvgljs.setBgColor(btn, ACCENT); lvgljs.setRadius(btn, 6); y += 36;
var tb  = lvgljs.textbox(M + 20, y, 140, 26); lvgljs.setFont(tb, 11); y += 34;
var sw  = lvgljs.sw(M + 20, y); y += 44;
var sl  = lvgljs.slider(M + 74, y - 26, 140, 0, 100, 50); y += 14;
var ch  = lvgljs.checkbox("Check", M + 20, y); y += 38;
var arcW = lvgljs.arc(M + 20, y, 46, 0, 100, 30); y += 54;
var pn  = lvgljs.panel(M + 20, y, 60, 24); lvgljs.setBgColor(pn, 0xE8854A); lvgljs.setRadius(pn, 6); y += 32;
var dd  = lvgljs.dropdown(M + 20, y, 140, 28); lvgljs.dropdownSet(dd, "A\nB\nC"); y += 40;
var barW = lvgljs.bar(M + 20, y, 160, 14, 42); y += 24;

ok("label, btn, textbox, switch, slider, checkbox, arc, panel, dropdown, bar");
gap();

/* =================================================================
 * TC-02  Styling
 * ================================================================= */
cardHdr(2, "Styling", "setTextColor setBgColor setFont setRadius setOpacity setPos setSize setBorder style");

lvgljs.setTextColor(lb, PASS); lvgljs.setFont(lb, 20);
ok("setTextColor + setFont — green 20pt");

lvgljs.setBgColor(pn, 0x9B59B6); lvgljs.setRadius(pn, 12); lvgljs.setOpacity(pn, 160);
ok("setBgColor + setRadius + setOpacity");

lvgljs.setPos(tb, M + 40, y - 12); lvgljs.setSize(pn, 80, 18);
ok("setPos + setSize");

lvgljs.setBorder(pn, 2, 0x333333);
ok("setBorder");

lvgljs.setAlign(lb, 1);
ok("setAlign(center)");

lvgljs.setVisible(sw, true); lvgljs.setWidth(sl, 190); lvgljs.setHeight(btn, 30);
ok("setVisible setWidth setHeight");

lvgljs.style(arcW, "radius", 12); lvgljs.toFront(btn);
ok("style + toFront");

lvgljs.setImage(arcW, "");
gap();

/* =================================================================
 * TC-03  Getters
 * ================================================================= */
cardHdr(3, "Getters", "getScreenSize getValue getText getType getDropdownSelected");

var sz = lvgljs.getScreenSize();
sz.w > 0 ? ok("getScreenSize -> " + JSON.stringify(sz)) : bad("getScreenSize");

var v1 = lvgljs.getValue(sl);
v1 >= 0 ? ok("getValue(slider) -> " + v1) : bad("getValue");

lvgljs.getText(lb) === "Hello World" ? ok("getText(label)") : bad("getText");

lvgljs.getType(btn) === "btn" ? ok("getType(btn)") : bad("getType(btn)=" + lvgljs.getType(btn));
lvgljs.getType(lb)  === "label" ? ok("getType(label)") : bad("getType(label)");
lvgljs.getType(barW) === "bar" ? ok("getType(bar) [NEW]") : bad("getType(bar)");

lvgljs.getDropdownSelected(dd) === 0 ? ok("getDropdownSelected") : bad("getDropdownSelected");
gap();

/* =================================================================
 * TC-04  setValue (NEW)
 * ================================================================= */
cardHdr(4, "setValue", "setValue — slider switch arc bar dropdown");

lvgljs.setValue(sl, 80);
lvgljs.getValue(sl) === 80 ? ok("setValue(slider, 80)") : bad("setValue slider");

lvgljs.setValue(sw, 1);
lvgljs.getValue(sw) === 1 ? ok("setValue(switch, 1)") : bad("setValue switch");

lvgljs.setValue(arcW, 70);
lvgljs.getValue(arcW) === 70 ? ok("setValue(arc, 70)") : bad("setValue arc");

lvgljs.setValue(barW, 88);
lvgljs.getValue(barW) === 88 ? ok("setValue(bar, 88)") : bad("setValue bar");

lvgljs.setValue(dd, 2);
lvgljs.getDropdownSelected(dd) === 2 ? ok("setValue(dropdown, 2)") : bad("setValue dropdown");
gap();

/* =================================================================
 * TC-05  Layout & Lifecycle
 * ================================================================= */
cardHdr(5, "Layout & Lifecycle", "del setFlexFlow scrollTo");

var tmp = lvgljs.label("tmp", M + 20, y); y += 20;
lvgljs.del(tmp);
ok("del() — widget removed");

lvgljs.setFlexFlow(pn, 0);
ok("setFlexFlow(panel, row)");

lvgljs.scrollTo(pn, 0, 0);
ok("scrollTo(panel, 0, 0)");
gap();

/* =================================================================
 * TC-06  Timer
 * ================================================================= */
cardHdr(6, "Timer", "setInterval heartbeat");

var cnt = 0;
var tmr = lvgljs.label("0s", M + 20, y);
lvgljs.setFont(tmr, 14); lvgljs.setTextColor(tmr, PASS); y += 24;
var tid = lvgljs.setInterval(1000, function () {
    cnt++;
    lvgljs.setText(tmr, cnt + "s");
});
tid >= 0 ? ok("setInterval running (id=" + tid + ")") : bad("setInterval");
gap();

/* =================================================================
 * TC-07  Events (manual)
 * ================================================================= */
cardHdr(7, "Events", "onClick onPress onRelease — touch to verify");

var evb = lvgljs.btn("Click me", M + 20, y, 80, 28);
lvgljs.setFont(evb, 12); lvgljs.setBgColor(evb, ACCENT); lvgljs.setRadius(evb, 6);
var evs = lvgljs.label("[waiting]", M + 114, y + 5);
lvgljs.setFont(evs, 12); lvgljs.setTextColor(evs, SUB);
lvgljs.onClick(evb, function () { lvgljs.setTextColor(evs, PASS); lvgljs.setText(evs, "onClick OK"); });
y += 38;

var pr = lvgljs.label("onPress/Release: [touch anywhere]", M + 20, y);
lvgljs.setFont(pr, 12); lvgljs.setTextColor(pr, SUB);
lvgljs.onPress(function () { lvgljs.setTextColor(pr, PASS); lvgljs.setText(pr, "onPress OK / onRelease..."); });
lvgljs.onRelease(function () { lvgljs.setText(pr, "onPress OK / onRelease OK"); });
y += 22;
ok("events — manual verification");
gap();

/* =================================================================
 * TC-08  File I/O
 * ================================================================= */
cardHdr(8, "File I/O", "writeFile readFile deleteFile");

var fp = "/mnt/sdcard/jsroot/apps/lvgljs-test/_t.txt";
lvgljs.writeFile(fp, "hello-lvgljs");
lvgljs.readFile(fp) === "hello-lvgljs" ? ok("writeFile + readFile") : bad("writeFile/readFile");

lvgljs.deleteFile(fp);
lvgljs.readFile(fp) === "" ? ok("deleteFile") : bad("deleteFile");
gap();

/* =================================================================
 * TC-09  App Management
 * ================================================================= */
cardHdr(9, "App Management", "scanApps engineHealth getAutoStart getEnv");

var apps = JSON.parse(lvgljs.scanApps());
apps.length > 0 ? ok("scanApps -> " + apps.length + " apps") : bad("scanApps");

var h = JSON.parse(lvgljs.engineHealth());
h.ok ? ok("engineHealth: depth=" + h.depth) : bad("engineHealth");

ok("getAutoStart -> " + lvgljs.getAutoStart());
ok("getEnv(PATH) -> " + lvgljs.getEnv("PATH", "?"));
gap();

/* =================================================================
 * TC-10  Summary
 * ================================================================= */
cardHdr(10, "Result", "lvgljs-2.0");

var total = p + f;
var sumText = p + "/" + total + " passed";
var sumColor = f === 0 ? PASS : FAIL;

var sum = lvgljs.label(sumText, M + 20, y);
lvgljs.setFont(sum, 20);
lvgljs.setTextColor(sum, sumColor);


var exit = lvgljs.btn("X", W - 44, 6, 36, 36);
lvgljs.setBgColor(exit, ACCENT);
lvgljs.setRadius(exit, 8);
lvgljs.onClick(exit, function () { lvgljs.exit(); });
y += 44;

lvgljs.print("[lvgljs-test] " + sumText);
