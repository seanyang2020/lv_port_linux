// Weather + Calendar — self-contained single-package app
// Widgets nested inside panels for correct layout

var SKIN = __dirname + "/skin/";
var W = lvgljs.getScreenSize();
var isLand = W.w > W.h;

// Data
var wx = { city: "Shanghai", temp: 22, cond: "Sunny", icon: "sunny" };
var fc = [
    { d: "Today",  i: "sunny",    hi: 22, lo: 15 },
    { d: "Tom.",   i: "cloudy",   hi: 20, lo: 14 },
    { d: "Day+2",  i: "sunny",    hi: 23, lo: 16 },
    { d: "Day+3",  i: "overcast", hi: 19, lo: 13 }
];
var lunarDays = ["Chu1","Chu2","Chu3","Chu4","Chu5","Chu6","Chu7","Chu8","Chu9","Chu10",
    "11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30"];
var yiJi = [
    ["Wedding","Planting"],["Travel","Opening"],["Trade","MoveIn"],["BedSetup","Move"],
    ["Bridge","Burial"],["Ground","Earth"],["Canal","Water"],["Build","Logging"]
];
function lunarDay(y,m,d) { return ((y+m*13+d*7)%30)+1; }
function icon(name, small) { return SKIN + "weather_" + name + (small?"_small":"") + ".png"; }

// ============================================================
// Screen init
// ============================================================
lvgljs.screenColor(0xD2E0EB);
lvgljs.btn("X", W.w - 52, 8, 44, 44, function() { lvgljs.exit(); });

// ============================================================
// Weather panel
// ============================================================
var pw = isLand ? 598 : W.w - 60, ph = isLand ? 700 : 550;
var wxPan = lvgljs.panel(20, 60, pw, ph);
lvgljs.setBgColor(wxPan, 0xFFFFFF);
lvgljs.setOpacity(wxPan, 200);
lvgljs.setRadius(wxPan, 28);

// All children of wxPan: positions relative to panel top-left
var dateLbl  = lvgljs.label("", 30, 30, wxPan);
lvgljs.setTextColor(dateLbl, 0x172335); lvgljs.setFont(dateLbl, 22); lvgljs.setOpacity(dateLbl, 150);

var lunarLbl = lvgljs.label("", 220, 30, wxPan);
lvgljs.setTextColor(lunarLbl, 0x172335); lvgljs.setFont(lunarLbl, 22); lvgljs.setOpacity(lunarLbl, 150);

var timeLbl  = lvgljs.label("", 30, 86, wxPan);
lvgljs.setTextColor(timeLbl, 0x000000); lvgljs.setFont(timeLbl, 36);

var iconImg  = lvgljs.image(icon(wx.icon), isLand ? 219 : 290, isLand ? 235 : 140, 160, 160, wxPan);

var cityLbl  = lvgljs.label("* " + wx.city, isLand ? 420 : 540, 40, wxPan);
lvgljs.setTextColor(cityLbl, 0x172335); lvgljs.setFont(cityLbl, 22);

var tempLbl  = lvgljs.label(wx.temp + " C", isLand ? 372 : 500, isLand ? 87 : 80, wxPan);
lvgljs.setTextColor(tempLbl, 0x172335); lvgljs.setFont(tempLbl, 48);

var condLbl  = lvgljs.label(wx.cond, isLand ? 235 : 310, isLand ? 410 : 300, wxPan);
lvgljs.setTextColor(condLbl, 0x172335); lvgljs.setFont(condLbl, 22);

// Forecast
var fcIcons = [], fcLabels = [];
var fcX = 30, fcY = isLand ? 550 : 400, fcW = isLand ? 120 : 150, fcGap = isLand ? 20 : 30;
for (var i = 0; i < 4; i++) {
    var x = fcX + i * (fcW + fcGap);
    fcLabels.push(lvgljs.label(fc[i].d, x + 30, fcY + 12, wxPan));
    fcIcons.push(lvgljs.image(icon(fc[i].i, true), x + 35, fcY + 58, 50, 50, wxPan));
    var tl = lvgljs.label(fc[i].hi + "/" + fc[i].lo, x + 30, fcY + 115, wxPan);
    lvgljs.setTextColor(tl, 0x666666); lvgljs.setFont(tl, 14);
}
for (var j = 0; j < fcLabels.length; j++) {
    lvgljs.setTextColor(fcLabels[j], 0x172335); lvgljs.setFont(fcLabels[j], 22);
}

// ============================================================
// Calendar panel
// ============================================================
var cy = isLand ? 70 : 640, ch = isLand ? 700 : W.h - cy - 10;
var calPan = lvgljs.panel(isLand ? 652 : 20, cy, pw, ch);
lvgljs.setBgColor(calPan, 0xFFFFFF);
lvgljs.setOpacity(calPan, 200);
lvgljs.setRadius(calPan, 28);

var calTitle = lvgljs.label("", isLand ? 199 : pw/2 - 60, 40, calPan);
lvgljs.setTextColor(calTitle, 0x172335); lvgljs.setFont(calTitle, 30);

var weekHdr  = lvgljs.label("Su Mo Tu We Th Fr Sa", isLand ? 40 : 30, 90, calPan);
lvgljs.setTextColor(weekHdr, 0x999999); lvgljs.setFont(weekHdr, 14);

var dayLabels = [];
var cw = isLand ? 73 : Math.floor(pw/7.5), rh = isLand ? 60 : 60;
var gx = isLand ? 26 : 25, gy = isLand ? 117 : 120;
for (var row = 0; row < 6; row++)
    for (var col = 0; col < 7; col++)
        dayLabels.push(lvgljs.label("", gx + col*cw, gy + row*rh, calPan));

var hlLabel = lvgljs.label("", isLand ? 50 : 25, gy + 6*rh + 15, calPan);
lvgljs.setTextColor(hlLabel, 0x666666); lvgljs.setFont(hlLabel, 14);

// ============================================================
// Update
// ============================================================
function updateAll() {
    var now = new Date();
    var y = now.getFullYear(), m = now.getMonth()+1, d = now.getDate();
    var wd = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"][now.getDay()];
    lvgljs.setText(dateLbl, y+"-"+(m<10?"0":"")+m+"-"+(d<10?"0":"")+d+" "+wd);
    var h=now.getHours(), mi=now.getMinutes(), s=now.getSeconds();
    lvgljs.setText(timeLbl, (h<10?"0":"")+h+":"+(mi<10?"0":"")+mi+":"+(s<10?"0":"")+s);
    lvgljs.setText(lunarLbl, "Lunar "+m+"/"+d+" "+lunarDays[lunarDay(y,m,d)-1]);

    lvgljs.setText(calTitle, y+"-"+(m<10?"0":"")+m);
    var swd = new Date(y, m-1, 1).getDay();
    var dim = new Date(y, m, 0).getDate();
    for (var i = 0; i < 42; i++) {
        var dn = i - swd + 1;
        if (dn >= 1 && dn <= dim) {
            lvgljs.setText(dayLabels[i], dn+"\n"+lunarDays[lunarDay(y,m,dn)-1]);
            if (dn === d) {
                lvgljs.setBgColor(dayLabels[i], 0x0088FF); lvgljs.setOpacity(dayLabels[i], 25);
                lvgljs.setTextColor(dayLabels[i], 0x0088FF); lvgljs.setRadius(dayLabels[i], 8);
            } else {
                lvgljs.setBgColor(dayLabels[i], 0x000000); lvgljs.setOpacity(dayLabels[i], 0);
                lvgljs.setTextColor(dayLabels[i], 0x000000);
            }
        } else {
            lvgljs.setText(dayLabels[i], "");
            lvgljs.setBgColor(dayLabels[i], 0x000000); lvgljs.setOpacity(dayLabels[i], 0);
        }
    }
    var idx = (y+m+d)%8;
    lvgljs.setText(hlLabel, "Yi: "+yiJi[idx][0]+", "+yiJi[(idx+1)%8][0]+
        "  Ji: "+yiJi[(idx+2)%8][0]+", "+yiJi[(idx+3)%8][0]);
}

// Refresh
lvgljs.btn("Refresh", isLand ? 260 : 320, isLand ? 440 : 560, 100, 40, function() {
    wx.temp = [20,21,22,23,24,25,26][Math.floor(Math.random()*7)];
    lvgljs.setText(tempLbl, wx.temp+" C");
    var f0 = fc.shift(); fc.push(f0);
    for (var i = 0; i < 4; i++) {
        lvgljs.setText(fcLabels[i], fc[i].d);
        lvgljs.setImage(fcIcons[i], icon(fc[i].i, true));
    }
});

updateAll();
lvgljs.setInterval(1000, updateAll);
