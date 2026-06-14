// Widgets Showcase — tabs + theme switcher
// Replicates the lv_binding_js widgets demo using pure lvgljs API

var W = lvgljs.getScreenSize();
lvgljs.screenColor(0x1a1a2e);

// ---- Color themes ----
var colors = [
    { hex: 0x2196F3, name: "Blue"    },
    { hex: 0x4CAF50, name: "Green"   },
    { hex: 0x9E9E9E, name: "Grey"    },
    { hex: 0x607D8B, name: "BlueGrey"},
    { hex: 0xFF9800, name: "Orange"  },
    { hex: 0xF44336, name: "Red"     },
    { hex: 0x9C27B0, name: "Purple"  },
    { hex: 0x009688, name: "Teal"    }
];
var themeColor = 0x2196F3;
var paletteOpen = false;
var swatches = [];

// ---- Tab state ----
var tabs = ["Profile", "Analytics", "Shop"];
var activeTab = 0;
var tabBtns = [];
var tabPanels = [];

// ---- Close button ----
var closeBtn = lvgljs.btn("X", W.w-56, 8, 48, 48, function(){ lvgljs.exit(); });
lvgljs.setRadius(closeBtn, 24);
lvgljs.setBgColor(closeBtn, 0x333333);
lvgljs.setOpacity(closeBtn, 120);
lvgljs.setTextColor(closeBtn, 0xFFFFFF);
lvgljs.toFront(closeBtn);
lvgljs.hideBackButton();

// ---- Tab bar ----
var tabW = (W.w - 20) / 3;
for (var i = 0; i < 3; i++) {
    var btn = lvgljs.btn(tabs[i], 5 + i * tabW, 65, tabW - 5, 36, function(idx) {
        return function() { switchTab(idx); };
    }(i));
    tabBtns.push(btn);
    lvgljs.setRadius(btn, 4);
    lvgljs.setTextColor(btn, 0xFFFFFF);
}

// ---- Tab content panels ----
for (var j = 0; j < 3; j++) {
    var p = lvgljs.panel(10, 110, W.w - 20, 500);
    lvgljs.setBgColor(p, 0x2a2a3e);
    lvgljs.setOpacity(p, 255);
    lvgljs.setRadius(p, 8);
    tabPanels.push(p);
}

// Profile tab content
var profileName = lvgljs.label("User Profile", 30, 20, tabPanels[0]);
lvgljs.setTextColor(profileName, 0xFFFFFF); lvgljs.setFont(profileName, 24);

var profileInfo = lvgljs.label("Name: Sean\nEmail: sean@example.com\nVersion: 1.0.0", 30, 70, tabPanels[0]);
lvgljs.setTextColor(profileInfo, 0xCCCCCC); lvgljs.setFont(profileInfo, 16);

// Analytics tab content
var analyticsTitle = lvgljs.label("Analytics", 30, 20, tabPanels[1]);
lvgljs.setTextColor(analyticsTitle, 0xFFFFFF); lvgljs.setFont(analyticsTitle, 24);

var metrics = ["Users: 1,234", "Sessions: 5,678", "Bounce: 42%", "Revenue: $9,999"];
var yPos = 70;
for (var k = 0; k < metrics.length; k++) {
    var lbl = lvgljs.label(metrics[k], 30, yPos, tabPanels[1]);
    lvgljs.setTextColor(lbl, 0xCCCCCC); lvgljs.setFont(lbl, 16);
    yPos += 40;
}

// Shop tab content
var shopTitle = lvgljs.label("Shop", 30, 20, tabPanels[2]);
lvgljs.setTextColor(shopTitle, 0xFFFFFF); lvgljs.setFont(shopTitle, 24);

var products = ["Item A — $10", "Item B — $25", "Item C — $50"];
yPos = 70;
for (var p = 0; p < products.length; p++) {
    var lbl = lvgljs.label(products[p], 30, yPos, tabPanels[2]);
    lvgljs.setTextColor(lbl, 0xCCCCCC); lvgljs.setFont(lbl, 16);
    yPos += 40;
}

// ---- Floating color palette (bottom-right) ----
var paletteX = W.w - 65, paletteY = W.h - 160;

// Color swatches (hidden initially)
for (var c = 0; c < colors.length; c++) {
    var sw = lvgljs.btn("", paletteX + 10 + c * 28, paletteY + 10, 24, 24, function(hex) {
        return function() {
            themeColor = hex;
            applyTheme();
        };
    }(colors[c].hex));
    lvgljs.setBgColor(sw, colors[c].hex);
    lvgljs.setRadius(sw, 12);
    lvgljs.setVisible(sw, 0);
    lvgljs.toFront(sw);
    swatches.push(sw);
}

// Main color button (always visible)
var colorBtn = lvgljs.btn("", paletteX, paletteY, 50, 50, function() {
    paletteOpen = !paletteOpen;
    for (var i = 0; i < swatches.length; i++) {
        lvgljs.setVisible(swatches[i], paletteOpen ? 1 : 0);
    }
});
lvgljs.setBgColor(colorBtn, themeColor);
lvgljs.setRadius(colorBtn, 25);
lvgljs.toFront(colorBtn);

// ---- Theme application ----
function applyTheme() {
    lvgljs.setBgColor(colorBtn, themeColor);
    for (var i = 0; i < tabBtns.length; i++) {
        lvgljs.setBgColor(tabBtns[i], i === activeTab ? themeColor : 0x444444);
    }
    for (var j = 0; j < tabPanels.length; j++) {
        lvgljs.setBorder(tabPanels[j], j === activeTab ? 2 : 0, themeColor);
    }
}

// ---- Tab switching ----
function switchTab(idx) {
    activeTab = idx;
    for (var i = 0; i < 3; i++) {
        lvgljs.setBgColor(tabBtns[i], i === idx ? themeColor : 0x444444);
        lvgljs.setVisible(tabPanels[i], i === idx ? 1 : 0);
    }
    applyTheme();
}

// ---- Init ----
switchTab(0);
applyTheme();
lvgljs.print("Widgets ready");

// For live clock on profile tab
lvgljs.setInterval(1000, function() {
    var now = new Date();
    var h = now.getHours(), m = now.getMinutes(), s = now.getSeconds();
    lvgljs.setText(profileInfo, "Name: Sean\nEmail: sean@example.com\nTime: " +
        (h<10?"0":"")+h+":"+(m<10?"0":"")+m+":"+(s<10?"0":"")+s);
});
