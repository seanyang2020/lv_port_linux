/**
 * js-launcher — JavaScript application launcher (Phase 4)
 *
 * Card-style UI: light background, white cards with coloured
 * accents.  Each app gets a unique colour from the palette.
 *
 * New APIs:
 *   lvgljs.scanApps()      → JSON array
 *   lvgljs.runApp(name)    → 0 on success
 *   lvgljs.getAutoStart()  → "none" or app name
 *   lvgljs.setAutoStart(n) → persist config
 *   lvgljs.engineHealth()  → JSON status object
 */

/* ================================================================
 *  Colour palette — cycled across app cards
 * ================================================================ */
var PALETTE = [
    0x4A90D9,  // blue
    0x50B86C,  // green
    0xE8854A,  // orange
    0x9B59B6,  // purple
    0x3498DB,  // sky
    0x1ABC9C,  // teal
    0xE74C3C,  // red
    0xF39C12,  // amber
];

/* ================================================================
 *  Layout constants
 * ================================================================ */
var scr   = lvgljs.getScreenSize();
var M     = 16;            // margin / gutter
var CARD_W = scr.w - M * 2;
var CARD_H = 62;
var GAP   = 10;
var RADIUS = 12;
var TITLE_Y = 16;

/* Light theme */
var BG      = 0xF0F0F4;
var SURFACE = 0xFFFFFF;
var TEXT    = 0x1A1A1A;
var SUBTLE  = 0x888888;
var BORDER  = 0xDDDDE0;

/* ================================================================
 *  Init
 * ================================================================ */
lvgljs.screenColor(BG);
lvgljs.setFPS(20);
lvgljs.heartbeat(10000);

/* Refresh heartbeat every 3 s */
lvgljs.setInterval(3000, function () {
    lvgljs.heartbeat();
});

var health = JSON.parse(lvgljs.engineHealth());
lvgljs.print("[launcher] depth=" + health.depth +
             " apps=" + health.app_count);

var apps = JSON.parse(lvgljs.scanApps());
lvgljs.print("[launcher] " + apps.length + " app(s)");

/* ================================================================
 *  Header
 * ================================================================ */
var title = lvgljs.label("Applications", M, TITLE_Y);
lvgljs.setFont(title, 24);
lvgljs.setTextColor(title, TEXT);

var sub = lvgljs.label(apps.length + " available", M, TITLE_Y + 28);
lvgljs.setFont(sub, 14);
lvgljs.setTextColor(sub, SUBTLE);

/* Divider line (thin panel) */
var div = lvgljs.panel(M, TITLE_Y + 50, CARD_W, 1);
lvgljs.setBgColor(div, 0xD8D8DC);
lvgljs.setRadius(div, 0);

/* ================================================================
 *  Exit button — subtle top-right
 * ================================================================ */
var exitBtn = lvgljs.btn("X", scr.w - 44, 10, 34, 34);
lvgljs.setFont(exitBtn, 16);
lvgljs.setTextColor(exitBtn, SUBTLE);
lvgljs.setBgColor(exitBtn, SURFACE);
lvgljs.setRadius(exitBtn, 17);
lvgljs.setBorder(exitBtn, 1, BORDER);
lvgljs.onClick(exitBtn, function () {
    lvgljs.print("[launcher] exit");
    lvgljs.exit();
});

/* ================================================================
 *  App list — cards
 * ================================================================ */
if (apps.length === 0) {
    var empty = lvgljs.label(
        "No applications found.\nPlace bundles in jsroot/apps/<name>/index.js",
        M, 110);
    lvgljs.setFont(empty, 14);
    lvgljs.setTextColor(empty, SUBTLE);
} else {
    buildCards(apps);
}

/* Auto-start */
var auto = lvgljs.getAutoStart();
if (auto !== "none") {
    for (var i = 0; i < apps.length; i++) {
        if (apps[i].name === auto) {
            lvgljs.print("[launcher] auto-start: " + auto);
            lvgljs.runApp(auto);
            break;
        }
    }
}

/* ================================================================
 *  Card builder
 * ================================================================ */
function buildCards(apps) {
    var y = TITLE_Y + 65;

    for (var i = 0; i < apps.length; i++) {
        var colour = PALETTE[i % PALETTE.length];

        /* Card surface (white panel) */
        var card = lvgljs.panel(M, y, CARD_W, CARD_H);

        /* ---- Children relative to card (0,0 = card top-left) ---- */

        /* Colour accent dot */
        var dot = lvgljs.panel(12, 16, 30, 30, card);
        lvgljs.setBgColor(dot, colour);
        lvgljs.setRadius(dot, 15);
        lvgljs.setBorder(dot, 0);

        /* First letter of app name (white on dot) */
        var letter = apps[i].name.charAt(0).toUpperCase();
        var dotLabel = lvgljs.label(letter, 20, 22, card);
        lvgljs.setFont(dotLabel, 15);
        lvgljs.setTextColor(dotLabel, 0xFFFFFF);

        /* App name */
        var name = lvgljs.label(apps[i].name, 52, 14, card);
        lvgljs.setFont(name, 17);
        lvgljs.setTextColor(name, TEXT);

        /* Resolution / subtitle */
        var hint = lvgljs.label(apps[i].resolution || "common", 52, 36, card);
        lvgljs.setFont(hint, 12);
        lvgljs.setTextColor(hint, SUBTLE);

        /* Arrow indicator */
        var arrow = lvgljs.label(">", CARD_W - 30, 18, card);
        lvgljs.setFont(arrow, 20);
        lvgljs.setTextColor(arrow, SUBTLE);

        /* Card styling */
        lvgljs.setBgColor(card, SURFACE);
        lvgljs.setRadius(card, RADIUS);
        lvgljs.setBorder(card, 1, BORDER);

        /* Click anywhere on card → launch app */
        (function (app) {
            lvgljs.onClick(card, function () {
                lvgljs.print("[launcher] launch: " + app.name);
                var ret = lvgljs.runApp(app.name);
                if (ret !== 0) {
                    lvgljs.print("[launcher] FAILED: " + app.name);
                }
            });
        })(apps[i]);

        y += CARD_H + GAP;
    }
}
