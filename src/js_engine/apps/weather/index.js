// Weather + Calendar — bilingual (zh/en) via LANG env var
//   export LANG=zh_CN.UTF-8  → Chinese
//   export LANG=en_US.UTF-8  → English
//   default                   → English

var L = lvgljs.getEnv("LANG", "en").indexOf("zh") >= 0 ? "zh" : "en";
var CJK = "cjk", M14=14, M16=16, M22=22, M30=30, M36=36, M48=48;
var SKIN = __dirname + "/skin/";
var W = lvgljs.getScreenSize();
var isLand = W.w > W.h;

// ---- i18n data ----
var T = {
    en: {
        city: "Shanghai", cond: "Sunny", icon: "sunny",
        week: ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"],
        fcDays: ["Today","Tom.","Day+2","Day+3"],
        yi: [["Wedding","Planting"],["Travel","Opening"],["Trade","MoveIn"],["BedSetup","Move"],
             ["Bridge","Burial"],["Ground","Earth"],["Canal","Water"],["Build","Logging"]],
        lunarDays: ["Chu1","Chu2","Chu3","Chu4","Chu5","Chu6","Chu7","Chu8","Chu9","Chu10",
            "11","12","13","14","15","16","17","18","19","20",
            "21","22","23","24","25","26","27","28","29","30"],
        fmtDate: function(y,m,d,wd){return y+"-"+(m<10?"0":"")+m+"-"+(d<10?"0":"")+d+" "+T.en.week[wd];},
        fmtLunar: function(m,d,ld){return "Lunar "+m+"/"+d+" "+T.en.lunarDays[ld-1];},
        fmtCal: function(y,m){return y+"-"+(m<10?"0":"")+m;},
        fmtHuangli: function(yi,ji){return "Yi: "+yi[0]+" "+yi[1]+"  Ji: "+ji[0]+" "+ji[1];},
        weekHdr: "Su Mo Tu We Th Fr Sa"
    },
    zh: {
        city: "上海", cond: "晴天", icon: "sunny",
        week: ["周日","周一","周二","周三","周四","周五","周六"],
        fcDays: ["今天","明天","后天","周五"],
        yi: [["嫁娶","栽种"],["出行","开市"],["交易","入宅"],["安床","移徙"],
             ["造桥","安葬"],["动土","破土"],["开渠","放水"],["修造","伐木"]],
        lunarDays: ["初一","初二","初三","初四","初五","初六","初七","初八","初九","初十",
            "十一","十二","十三","十四","十五","十六","十七","十八","十九","二十",
            "廿一","廿二","廿三","廿四","廿五","廿六","廿七","廿八","廿九","三十"],
        fmtDate: function(y,m,d,wd){return y+"年"+(m<10?"0":"")+m+"月"+(d<10?"0":"")+d+"日 "+T.zh.week[wd];},
        fmtLunar: function(m,d,ld){return "农历"+T.zh.lunarDays[ld-1];},
        fmtCal: function(y,m){return y+"年"+(m<10?"0":"")+m+"月";},
        fmtHuangli: function(yi,ji){return "宜: "+yi[0]+" "+yi[1]+"  忌: "+ji[0]+" "+ji[1];},
        weekHdr: "日 一 二 三 四 五 六"
    }
};
var D = T[L];
var font = (L==="zh") ? CJK : M16;

var wx = { city: D.city, temp: 22, cond: D.cond, icon: D.icon };
var fc = [
    { d: D.fcDays[0], i: "sunny",    hi: 22, lo: 15 },
    { d: D.fcDays[1], i: "cloudy",   hi: 20, lo: 14 },
    { d: D.fcDays[2], i: "sunny",    hi: 23, lo: 16 },
    { d: D.fcDays[3], i: "overcast", hi: 19, lo: 13 }
];
function lunarDay(y,m,d) { return ((y+m*13+d*7)%30)+1; }
function icon(name, small) { return SKIN+"weather_"+name+(small?"_small":"")+".png"; }

// Screen
lvgljs.screenColor(0xD2E0EB);
var closeImg = lvgljs.image(SKIN+"weather_close.png", W.w-68, 8, 60, 60);
lvgljs.onClick(closeImg, function(){ lvgljs.exit(); });

var m=20, gap=24, panW=isLand?598:W.w-m*2, wxH=isLand?700:550;
var calY=isLand?m+gap:m+wxH+gap, calH=isLand?700:W.h-calY-m;

// Weather panel
var wxPan = lvgljs.panel(m, m, panW, wxH);
lvgljs.setBgColor(wxPan, 0xFFFFFF); lvgljs.setOpacity(wxPan, 200); lvgljs.setRadius(wxPan, 28);
var dateLbl  = lvgljs.label("", 30, 30, wxPan);
lvgljs.setTextColor(dateLbl, 0x172335); lvgljs.setFont(dateLbl, M22); lvgljs.setOpacity(dateLbl, 150);
var lunarLbl = lvgljs.label("", 220, 30, wxPan);
lvgljs.setTextColor(lunarLbl, 0x172335); lvgljs.setFont(lunarLbl, font); lvgljs.setOpacity(lunarLbl, 150);
var timeLbl  = lvgljs.label("", 30, 86, wxPan);
lvgljs.setTextColor(timeLbl, 0x000000); lvgljs.setFont(timeLbl, M36);
var iconImg = lvgljs.image(icon(wx.icon), isLand?219:290, isLand?235:140, 160, 160, wxPan);
lvgljs.image(SKIN+"weather_location.png", isLand?538:686, isLand?43:24, 30, 30, wxPan);
var cityLbl  = lvgljs.label(D.city, isLand?420:540, 40, wxPan);
lvgljs.setTextColor(cityLbl, 0x172335); lvgljs.setFont(cityLbl, font);
var tempLbl  = lvgljs.label(wx.temp+"°C", isLand?372:500, isLand?87:80, wxPan);
lvgljs.setTextColor(tempLbl, 0x172335); lvgljs.setFont(tempLbl, M48);
var condLbl  = lvgljs.label(D.cond, isLand?235:310, isLand?410:300, wxPan);
lvgljs.setTextColor(condLbl, 0x172335); lvgljs.setFont(condLbl, font);

var fcIcons=[], fcLabels=[], fcX=30, fcY=isLand?550:400, fcW=isLand?120:150, fcGap=isLand?20:30;
for (var i=0; i<4; i++) {
    var x=fcX+i*(fcW+fcGap);
    fcLabels.push(lvgljs.label(fc[i].d, x+30, fcY+12, wxPan));
    fcIcons.push(lvgljs.image(icon(fc[i].i,true), x+35, fcY+58, 50, 50, wxPan));
    var tl=lvgljs.label(fc[i].hi+"/"+fc[i].lo, x+30, fcY+115, wxPan);
    lvgljs.setTextColor(tl, 0x666666); lvgljs.setFont(tl, M14);
}
for (var j=0; j<fcLabels.length; j++) {
    lvgljs.setTextColor(fcLabels[j], 0x172335); lvgljs.setFont(fcLabels[j], font);
}

// Calendar panel
var calPan = lvgljs.panel(isLand?652:m, calY, panW, calH);
lvgljs.setBgColor(calPan, 0xFFFFFF); lvgljs.setOpacity(calPan, 200); lvgljs.setRadius(calPan, 28);
var calTitle = lvgljs.label("", isLand?199:panW/2-60, 40, calPan);
lvgljs.setTextColor(calTitle, 0x172335); lvgljs.setFont(calTitle, font);
var weekHdr  = lvgljs.label(D.weekHdr, isLand?40:25, 90, calPan);
lvgljs.setTextColor(weekHdr, 0x999999); lvgljs.setFont(weekHdr, font);
var dayLabels=[], cw=isLand?73:Math.floor((panW-50)/7), rh=isLand?60:Math.floor((calH-160)/6);
var gx=isLand?26:25, gy=isLand?117:120;
for (var row=0; row<6; row++)
    for (var col=0; col<7; col++)
        dayLabels.push(lvgljs.label("", gx+col*cw, gy+row*rh, calPan));
var hlLabel = lvgljs.label("", isLand?50:25, gy+6*rh+15, calPan);
lvgljs.setTextColor(hlLabel, 0x666666); lvgljs.setFont(hlLabel, font);

function updateAll() {
    var now=new Date();
    var y=now.getFullYear(), m=now.getMonth()+1, d=now.getDate();
    lvgljs.setText(dateLbl, D.fmtDate(y,m,d,now.getDay()));
    var h=now.getHours(), mi=now.getMinutes(), s=now.getSeconds();
    lvgljs.setText(timeLbl, (h<10?"0":"")+h+":"+(mi<10?"0":"")+mi+":"+(s<10?"0":"")+s);
    lvgljs.setText(lunarLbl, D.fmtLunar(m,d,lunarDay(y,m,d)));

    lvgljs.setText(calTitle, D.fmtCal(y,m));
    var swd=new Date(y,m-1,1).getDay(), dim=new Date(y,m,0).getDate();
    for (var i=0; i<42; i++) {
        var dn=i-swd+1;
        if (dn>=1 && dn<=dim) {
            lvgljs.setText(dayLabels[i], dn+"\n"+D.lunarDays[lunarDay(y,m,dn)-1]);
            lvgljs.setFont(dayLabels[i], font);
            if (dn===d) {
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
    var idx=(y+m+d)%8;
    var yi=D.yi[idx], ji=D.yi[(idx+2)%8];
    lvgljs.setText(hlLabel, D.fmtHuangli(yi,ji));
    lvgljs.setText(wxUpdateLbl, "Updated: " + agoStr(wxUpdateTs));  // live "X ago"
}

// ---- Weather cache & real API fetch ----
var WX_CACHE = __dirname + "/weather_cache.json";
var wxUpdateTs = 0;
var wxUpdateLbl = lvgljs.label("", isLand?235:310, isLand?440:330, wxPan);
lvgljs.setTextColor(wxUpdateLbl, 0x888888); lvgljs.setFont(wxUpdateLbl, 14);
lvgljs.setOpacity(wxUpdateLbl, 180);

function loadWxCache() {
    var d = lvgljs.readFile(WX_CACHE);
    if (!d || d.length < 10) return null;
    try { return JSON.parse(d); } catch(e) { return null; }
}
function saveWxCache() {
    var c = { temp: wx.temp, cond: wx.cond, icon: wx.icon,
              fc: fc, ts: wxUpdateTs, city: wx.city };
    lvgljs.writeFile(WX_CACHE, JSON.stringify(c));
}

function agoStr(ts) {
    if (!ts) return "";
    var s = Math.floor((Date.now() - ts) / 1000);
    if (s < 60) return "just now";
    var m = Math.floor(s / 60);
    if (m < 60) return m + "min ago";
    var h = Math.floor(m / 60);
    if (h < 24) return h + "h " + (m % 60) + "min ago";
    var d = Math.floor(h / 24);
    return d + "d ago";
}

function refreshWeatherUI() {
    lvgljs.setText(tempLbl, wx.temp + "°C");
    lvgljs.setText(condLbl, wx.cond);
    lvgljs.setImage(iconImg, icon(wx.icon));
    for (var i = 0; i < 4; i++) lvgljs.setImage(fcIcons[i], icon(fc[i].i, true));
    lvgljs.setText(wxUpdateLbl, "Updated: " + agoStr(wxUpdateTs));
}

function fetchWeather() {
    lvgljs.httpGet("https://api.open-meteo.com/v1/forecast?latitude=31.23&longitude=121.47&current_weather=true", function(json) {
        try {
            var data = JSON.parse(json.trim());
            var cw = data.current_weather;
            wx.temp = cw.temperature;
            var code = cw.weathercode;
            if (code === 0)          { wx.cond = "Clear";   wx.icon = "sunny"; }
            else if (code <= 3)      { wx.cond = "Cloudy";  wx.icon = "cloudy"; }
            else if (code <= 48)     { wx.cond = "Fog";     wx.icon = "cloudy"; }
            else if (code <= 67)     { wx.cond = "Rain";    wx.icon = "rainy"; }
            else if (code <= 77)     { wx.cond = "Snow";    wx.icon = "rainy"; }
            else if (code <= 82)     { wx.cond = "Shower";  wx.icon = "rainy"; }
            else                     { wx.cond = "Storm";   wx.icon = "rainy"; }
            wxUpdateTs = Date.now();
            saveWxCache();
            refreshWeatherUI();
            lvgljs.print("Weather: " + wx.temp + "C " + wx.cond);
        } catch(e) {
            lvgljs.print("Weather parse error: " + e);
            var c = loadWxCache();
            if (c) { wx.temp = c.temp; wx.cond = c.cond; wx.icon = c.icon; wxUpdateTs = c.ts; refreshWeatherUI(); }
        }
    });
}

// Init
var saved = loadWxCache();
if (saved && saved.ts && (Date.now() - saved.ts) < 1800000) {
    wx.temp = saved.temp; wx.cond = saved.cond; wx.icon = saved.icon; wxUpdateTs = saved.ts;
    if (saved.fc) for (var i = 0; i < Math.min(4, saved.fc.length); i++) {
        fc[i].i = saved.fc[i].i; fc[i].hi = saved.fc[i].hi; fc[i].lo = saved.fc[i].lo;
    }
    refreshWeatherUI();
} else {
    fetchWeather();
}
lvgljs.setInterval(1800000, fetchWeather);
updateAll();
lvgljs.setInterval(1000, updateAll);
// "Updated: X ago" refreshes every second via updateAll → refreshWeatherUI → agoStr
lvgljs.hideBackButton();
lvgljs.print("Weather ready [" + L + "]");
