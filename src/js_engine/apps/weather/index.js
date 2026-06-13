// Weather + Calendar — unified layout matching original weather.c
// No Refresh button, close uses skin image, panels form cohesive whole

var SKIN = __dirname + "/skin/";
var W = lvgljs.getScreenSize();
var isLand = W.w > W.h;

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
// Screen
// ============================================================
lvgljs.screenColor(0xD2E0EB);

// Close button — use skin image like original
var closeImg = lvgljs.image(SKIN + "weather_close.png", W.w - 68, 8, 60, 60);
lvgljs.onClick(closeImg, function() { lvgljs.exit(); });

// ============================================================
// Unified panel dimensions
// ============================================================
var margin = 20, gap = 24;
var panW = isLand ? 598 : W.w - margin*2;
var wxH  = isLand ? 700 : 550;
var calY = isLand ? margin + gap : margin + wxH + gap;
var calH = isLand ? 700 : W.h - calY - margin;

// ============================================================
// Weather panel
// ============================================================
var wxPan = lvgljs.panel(margin, margin, panW, wxH);
lvgljs.setBgColor(wxPan, 0xFFFFFF);
lvgljs.setOpacity(wxPan, 200);
lvgljs.setRadius(wxPan, 28);

var dateLbl  = lvgljs.label("", 30, 30, wxPan);
lvgljs.setTextColor(dateLbl, 0x172335); lvgljs.setFont(dateLbl, 22); lvgljs.setOpacity(dateLbl, 150);

var lunarLbl = lvgljs.label("", 220, 30, wxPan);
lvgljs.setTextColor(lunarLbl, 0x172335); lvgljs.setFont(lunarLbl, 22); lvgljs.setOpacity(lunarLbl, 150);

var timeLbl  = lvgljs.label("", 30, 86, wxPan);
lvgljs.setTextColor(timeLbl, 0x000000); lvgljs.setFont(timeLbl, 36);

var iconImg  = lvgljs.image(icon(wx.icon), isLand ? 219 : 290, isLand ? 235 : 140, 160, 160, wxPan);

var cityLbl  = lvgljs.label("* " + wx.city, isLand ? 420 : 540, 40, wxPan);
lvgljs.setTextColor(cityLbl, 0x172335); lvgljs.setFont(cityLbl, 22);

var locationImg = lvgljs.image(SKIN + "weather_location.png",
    isLand ? 538 : 686, isLand ? 43 : 24, 30, 30, wxPan);

var tempLbl  = lvgljs.label(wx.temp + " C", isLand ? 372 : 500, isLand ? 87 : 80, wxPan);
lvgljs.setTextColor(tempLbl, 0x172335); lvgljs.setFont(tempLbl, 48);

var condLbl  = lvgljs.label(wx.cond, isLand ? 235 : 310, isLand ? 410 : 300, wxPan);
lvgljs.setTextColor(condLbl, 0x172335); lvgljs.setFont(condLbl, 22);

// Forecast row
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
// Calendar panel — same style, positioned right below weather
// ============================================================
var calPan = lvgljs.panel(isLand ? 652 : margin, calY, panW, calH);
lvgljs.setBgColor(calPan, 0xFFFFFF);
lvgljs.setOpacity(calPan, 200);
lvgljs.setRadius(calPan, 28);

var calTitle = lvgljs.label("", isLand ? 199 : panW/2 - 60, 40, calPan);
lvgljs.setTextColor(calTitle, 0x172335); lvgljs.setFont(calTitle, 30);

var weekHdr  = lvgljs.label("Su Mo Tu We Th Fr Sa", isLand ? 40 : 25, 90, calPan);
lvgljs.setTextColor(weekHdr, 0x999999); lvgljs.setFont(weekHdr, 14);

var dayLabels = [];
var cw = isLand ? 73 : Math.floor((panW - 50) / 7), rh = isLand ? 60 : Math.floor((calH - 160) / 6);
var gx = isLand ? 26 : 25, gy = isLand ? 117 : 120;
for (var row = 0; row < 6; row++)
    for (var col = 0; col < 7; col++)
        dayLabels.push(lvgljs.label("", gx + col*cw, gy + row*rh, calPan));

var hlY = gy + 6*rh + 15;
var hlLabel = lvgljs.label("", isLand ? 50 : 25, hlY, calPan);
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

updateAll();
lvgljs.setInterval(1000, updateAll);
