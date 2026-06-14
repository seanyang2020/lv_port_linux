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

// Profile tab content (parent=tabPanels[0])
var photo = lvgljs.image(__dirname + "/avatar.png", 30, 20, 64, 64, tabPanels[0]); lvgljs.setRadius(photo, 32);
var profileName = lvgljs.label("Sean", 110, 30, tabPanels[0]); lvgljs.setTextColor(profileName, 0xFFFFFF); lvgljs.setFont(profileName, 22);
var profileInfo = lvgljs.label("sean@example.com\nVersion 1.0.0", 110, 60, tabPanels[0]); lvgljs.setTextColor(profileInfo, 0xCCCCCC); lvgljs.setFont(profileInfo, 14);

// Textbox (input) — tap to open keyboard
var tb = lvgljs.textbox(30, 180, 400, 36, tabPanels[0]); lvgljs.setText(tb, "Type here...");
lvgljs.onClick(tb, function(){ lvgljs.keyboard(tb); });

// Dropdown
var dd = lvgljs.dropdown(30, 370, 200, 36, tabPanels[0]);
lvgljs.dropdownSet(dd, "Option A\nOption B\nOption C");

// Slider
var sl = lvgljs.slider(30, 240, 300, 0, 100, 50, tabPanels[0]);
lvgljs.onChange(sl, function(){ lvgljs.setText(profileInfo, "Value: "+lvgljs.getValue(sl)); });

// Switch + Checkbox
var sw0 = lvgljs.sw(30, 290, tabPanels[0]); var swLbl = lvgljs.label("Notifications", 80, 295, tabPanels[0]); lvgljs.setTextColor(swLbl, 0xCCCCCC); lvgljs.setFont(swLbl, 14);
var cb0 = lvgljs.checkbox("Dark Mode", 30, 330, tabPanels[0]); lvgljs.setTextColor(cb0, 0xCCCCCC);

// Analytics tab content (parent=tabPanels[1])
var chartIcon = lvgljs.panel(30, 10, 40, 40, tabPanels[1]); lvgljs.setRadius(chartIcon, 8); lvgljs.setBgColor(chartIcon, 0x4ECDC4);
var chartLabel = lvgljs.label("▲", 10, 6, chartIcon); lvgljs.setTextColor(chartLabel, 0xFFFFFF); lvgljs.setFont(chartLabel, 20);
var analyticsTitle = lvgljs.label("Analytics", 80, 20, tabPanels[1]); lvgljs.setTextColor(analyticsTitle, 0xFFFFFF); lvgljs.setFont(analyticsTitle, 22);

var metrics = ["Users: 1,234", "Sessions: 5,678", "Bounce: 42%", "Revenue: $9,999"];
var yPos = 60;
for (var k = 0; k < metrics.length; k++) {
    var icon = lvgljs.panel(30, yPos+2, 12, 12, tabPanels[1]); lvgljs.setRadius(icon, 6); lvgljs.setBgColor(icon, k%2?0xFFE66D:0xFF6B6B);
    var lbl = lvgljs.label(metrics[k], 52, yPos, tabPanels[1]); lvgljs.setTextColor(lbl, 0xCCCCCC); lvgljs.setFont(lbl, 16);
    yPos += 40;
}

// Shop tab content (parent=tabPanels[2])
var cartIcon = lvgljs.panel(30, 10, 40, 40, tabPanels[2]); lvgljs.setRadius(cartIcon, 20); lvgljs.setBgColor(cartIcon, 0xF39C12);
var cartLabel = lvgljs.label("$", 14, 6, cartIcon); lvgljs.setTextColor(cartLabel, 0xFFFFFF); lvgljs.setFont(cartLabel, 22);
var shopTitle = lvgljs.label("Shop", 80, 20, tabPanels[2]); lvgljs.setTextColor(shopTitle, 0xFFFFFF); lvgljs.setFont(shopTitle, 22);

var products = ["Item A — $10", "Item B — $25", "Item C — $50"];
yPos = 60;
for (var p = 0; p < products.length; p++) {
    var icon = lvgljs.panel(30, yPos+4, 10, 10, tabPanels[2]); lvgljs.setRadius(icon, 5); lvgljs.setBgColor(icon, 0xF39C12);
    var lbl = lvgljs.label(products[p], 52, yPos, tabPanels[2]); lvgljs.setTextColor(lbl, 0xCCCCCC); lvgljs.setFont(lbl, 16);
    yPos += 40;
}

// ---- Floating color palette (bottom-right, vertical column) ----
var paletteX = W.w - 50, paletteY = W.h - 230;

// Color swatches (hidden initially)
for (var c = 0; c < colors.length; c++) {
    var sw = lvgljs.btn(colors[c].name.charAt(0), paletteX+2, paletteY-8-c*26, 22, 22, function(hex) {
        return function() { themeColor = hex; applyTheme(); };
    }(colors[c].hex));
    lvgljs.setBgColor(sw, colors[c].hex);
    lvgljs.setRadius(sw, 11);
    lvgljs.setVisible(sw, 0);
    lvgljs.toFront(sw);
    swatches.push(sw);
}

// Main color button (always visible)
var colorBtn = lvgljs.btn("", paletteX, paletteY, 44, 44, function() {
    paletteOpen = !paletteOpen;
    for (var i = 0; i < swatches.length; i++) {
        lvgljs.setVisible(swatches[i], paletteOpen ? 1 : 0);
    }
});
lvgljs.setBgColor(colorBtn, themeColor);
lvgljs.setRadius(colorBtn, 22);
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
