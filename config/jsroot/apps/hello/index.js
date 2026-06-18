/**
 * hello — minimal JS app example for lv_jsloader
 *
 * Demonstrates:
 *   - lvgljs.heartbeat()   safety watchdog
 *   - lvgljs.label()       text widget
 *   - lvgljs.btn()         button with onClick
 *   - lvgljs.setFont/setPos/setStyle shortcuts
 *   - lvgljs.exit()        return to launcher
 *   - lvgljs.setFPS()      CPU-saving frame rate
 */

/* ---- init ---- */
lvgljs.screenColor(0x1A1A2E);
lvgljs.setFPS(10);          /* low FPS for static screens */
lvgljs.heartbeat(5000);     /* 5 s watchdog */

/* ---- heading ---- */
var h = lvgljs.label("Hello, JS!", 20, 40);
lvgljs.setFont(h, 36);
lvgljs.setTextColor(h, 0xFFFFFF);

/* ---- subtitle ---- */
var sub = lvgljs.label("Powered by lv_jsloader", 20, 90);
lvgljs.setFont(sub, 16);
lvgljs.setTextColor(sub, 0x888888);

/* ---- screen info ---- */
var scr = lvgljs.getScreenSize();
var info = lvgljs.label("Screen: " + scr.w + "x" + scr.h, 20, 120);
lvgljs.setFont(info, 14);
lvgljs.setTextColor(info, 0x666666);

/* ---- exit button ---- */
var btn = lvgljs.btn("←  Back", 20, scr.h - 80, scr.w - 40, 52);
lvgljs.setFont(btn, 22);
lvgljs.setTextColor(btn, 0xFFFFFF);
lvgljs.setBgColor(btn, 0x3D3D5C);
lvgljs.setRadius(btn, 10);
lvgljs.setBorder(btn, 0);

lvgljs.onClick(btn, function() {
    lvgljs.print("[hello] exiting");
    lvgljs.exit();
});

/* ---- periodic heartbeat refresh ---- */
lvgljs.setInterval(3000, function() {
    lvgljs.heartbeat();  /* keep watchdog alive */
});

lvgljs.print("[hello] ready");
