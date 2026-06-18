// Weather + Calendar — bilingual (zh/en)
// Language from apps/weather/config.json:
//   { "lang": "en" }  → English (default)
//   { "lang": "zh" }  → Chinese (built-in CJK font ~1338 radicals; some glyphs missing)
// LANG env is only a fallback when config.json is absent/invalid.

function loadLang() {
    var raw = lvgljs.readFile(__dirname + "/config.json");
    if (raw) {
        try {
            var cfg = JSON.parse(raw);
            if (cfg && cfg.lang) {
                var s = String(cfg.lang).toLowerCase();
                if (s.indexOf("zh") === 0) return "zh";
                if (s.indexOf("en") === 0) return "en";
            }
        } catch (e) {
            lvgljs.print("weather config.json parse error: " + e);
        }
    }
    var env = lvgljs.getEnv("LANG", "");
    if (env && env.indexOf("zh") >= 0) return "zh";
    return "en";
}
var L = loadLang();
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
        // Uniform lunar day-of-month labels (1..30) — no ChuN / digit mix
        lunarDays: ["1","2","3","4","5","6","7","8","9","10",
            "11","12","13","14","15","16","17","18","19","20",
            "21","22","23","24","25","26","27","28","29","30"],
        fmtDate: function(y,m,d,wd){return y+"-"+(m<10?"0":"")+m+"-"+(d<10?"0":"")+d+" "+T.en.week[wd];},
        fmtLunar: function(y,m,d){
            var L=solarToLunar(y,m,d);
            return "Lunar "+L.month+"/"+L.day;
        },
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
        fmtLunar: function(y,m,d){
            var L=solarToLunar(y,m,d);
            return "农历"+L.month+"月"+T.zh.lunarDays[L.day-1];
        },
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

/* Real Chinese lunar calendar 1900–2100 (bit-packed month lengths).
 * Replaces the old fake hash which jumped +7 per solar day. */
var LUNAR_INFO = [
    0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09d50,0x06d25,
    0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,0x0b540,0x0d6a0,0x0ada2,0x095b0,0x14977,
    0x04970,0x0a4b0,0x0b4b5,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,
    0x06566,0x0d4a0,0x0ea50,0x06e95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,
    0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,
    0x06ca0,0x0b550,0x15355,0x04da0,0x0a5d0,0x14573,0x052d0,0x0a9a8,0x0e950,0x06aa0,
    0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,
    0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,0x0d250,0x0d558,0x0b540,0x0b5a0,0x195a6,
    0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x0af46,0x0ab60,0x09570,
    0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x05ac0,0x0ab60,0x096d5,0x092e0,
    0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,
    0x0a950,0x0b4a0,0x0baa4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x052b0,0x0a930,
    0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4e0,0x0d260,0x0ea65,0x0d530,
    0x05aa0,0x076a3,0x096d0,0x04bd7,0x04ad0,0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,
    0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0,
    0x14b63,0x09370,0x049f8,0x04970,0x064b0,0x168a6,0x0ea50,0x06b20,0x1a6c4,0x0aae0,
    0x0a2e0,0x0d2e3,0x0c960,0x0d557,0x0d4a0,0x0da50,0x05d55,0x056a0,0x0a6d0,0x055d4,
    0x052d0,0x0a9b8,0x0a950,0x0b4a0,0x0b6a6,0x0ad50,0x055a0,0x0aba4,0x0a5b0,0x052b0,
    0x0b273,0x06930,0x07337,0x06aa0,0x0ad50,0x14b55,0x04b60,0x0a570,0x054e4,0x0d160,
    0x0e968,0x0d520,0x0daa0,0x16aa6,0x056d0,0x04ae0,0x0a9d4,0x0a2d0,0x0d150,0x0f252,
    0x0d520
];
function _leapMonth(y){ return LUNAR_INFO[y-1900] & 0xf; }
function _leapDays(y){
    if (_leapMonth(y)) return (LUNAR_INFO[y-1900] & 0x10000) ? 30 : 29;
    return 0;
}
function _monthDays(y,m){ return (LUNAR_INFO[y-1900] & (0x10000>>m)) ? 30 : 29; }
function _lYearDays(y){
    var i, sum=348;
    for (i=0x8000; i>0x8; i>>=1) sum += (LUNAR_INFO[y-1900] & i) ? 1 : 0;
    return sum + _leapDays(y);
}
function solarToLunar(y, m, d) {
    if (y < 1900 || y > 2100) return { month: m, day: ((d-1)%30)+1, isLeap: false };
    /* Base 1900-01-31; -1 corrects common off-by-one vs modern CNY dates. */
    var offset = Math.floor((Date.UTC(y, m-1, d) - Date.UTC(1900, 0, 31)) / 86400000) - 1;
    var i, temp=0;
    for (i=1900; i<2101 && offset>0; i++) { temp=_lYearDays(i); offset-=temp; }
    if (offset<0) { offset+=temp; i--; }
    var year=i, leap=_leapMonth(i), isLeap=false;
    for (i=1; i<13 && offset>0; i++) {
        if (leap>0 && i===(leap+1) && !isLeap) { --i; isLeap=true; temp=_leapDays(year); }
        else { temp=_monthDays(year, i); }
        if (isLeap && i===(leap+1)) isLeap=false;
        offset-=temp;
    }
    if (offset===0 && leap>0 && i===leap+1) {
        if (isLeap) isLeap=false;
        else { isLeap=true; --i; }
    }
    if (offset<0) { offset+=temp; --i; }
    return { year: year, month: i, day: offset+1, isLeap: isLeap };
}
function lunarDay(y,m,d){ return solarToLunar(y,m,d).day; }
function icon(name, small) { return SKIN+"weather_"+name+(small?"_small":"")+".png"; }

// Screen
lvgljs.screenColor(0xD2E0EB);
var closeImg = lvgljs.image(SKIN+"weather_close.png", W.w-68, 8, 60, 60);
lvgljs.onClick(closeImg, function(){ lvgljs.exit(); });

var m=20, gap=24, panW=isLand?598:W.w-m*2, wxH=isLand?700:550;
var calY=isLand?m+gap:m+wxH+gap, calH=isLand?700:W.h-calY-m;

// Weather panel
var wxPan = lvgljs.panel(m, m, panW, wxH);
lvgljs.setBgColor(wxPan, 0xFFFFFF); lvgljs.setOpacity(wxPan, 255); lvgljs.setRadius(wxPan, 28);
var dateLbl  = lvgljs.label("", 30, 30, wxPan);
lvgljs.setTextColor(dateLbl, 0x172335); lvgljs.setFont(dateLbl, font);
var lunarLbl = lvgljs.label("", 220, 30, wxPan);
lvgljs.setTextColor(lunarLbl, 0x172335); lvgljs.setFont(lunarLbl, font);
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
lvgljs.setBgColor(calPan, 0xFFFFFF); lvgljs.setOpacity(calPan, 255); lvgljs.setRadius(calPan, 28);
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

// Track last-rendered calendar state to avoid redundant LVGL invalidations.
// Each setText/setStyle call triggers dirty area → expensive VGS2 rotate per area.
var gLastCalY = 0, gLastCalM = 0, gLastCalD = 0;
var gLastHuangliIdx = -1;

function updateCalendar(y, m, d) {
    // Only redraw calendar when date actually changes (once per day or on first load)
    if (y === gLastCalY && m === gLastCalM && d === gLastCalD) return;
    gLastCalY = y; gLastCalM = m; gLastCalD = d;

    lvgljs.setText(dateLbl, D.fmtDate(y,m,d,new Date(y,m-1,d).getDay()));
    lvgljs.setText(lunarLbl, D.fmtLunar(y,m,d));
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
    if (idx !== gLastHuangliIdx) {
        gLastHuangliIdx = idx;
        var yi=D.yi[idx], ji=D.yi[(idx+2)%8];
        lvgljs.setText(hlLabel, D.fmtHuangli(yi,ji));
    }
}

// Per-second update: only the clock label (1 dirty area instead of 48+)
function updateClock() {
    var now=new Date();
    var y=now.getFullYear(), m=now.getMonth()+1, d=now.getDate();
    var h=now.getHours(), mi=now.getMinutes(), s=now.getSeconds();
    lvgljs.setText(timeLbl, (h<10?"0":"")+h+":"+(mi<10?"0":"")+mi+":"+(s<10?"0":"")+s);

    // Calendar redraw only when date changes (checked once per second, redraws ~once/day)
    updateCalendar(y, m, d);

    // "Updated: X ago" — update every 30s to further reduce dirty areas
    if (s % 30 === 0)
        lvgljs.setText(wxUpdateLbl, "Updated: " + agoStr(wxUpdateTs));
}

// ---- Weather cache & real API fetch ----
var WX_CACHE = __dirname + "/weather.json";
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
              fc: fc, ts: wxUpdateTs, city: wx.city,
              screen: CFG.screen };
    lvgljs.writeFile(WX_CACHE, JSON.stringify(c, null, 2));
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
lvgljs.toFront(closeImg);  // bring close button above both panels
updateClock();
lvgljs.setFPS(20);  // smooth UI, no need for 30fps on weather display
lvgljs.setInterval(1000, updateClock);
lvgljs.hideBackButton();
// ---- Screen power management (from weather.json, all disabled by default) ----
var CFG = { screen: { auto_off_enabled: false, double_tap_wake: false, schedule_enabled: false } };
var hasConfig = false;
if (saved && saved.screen) { CFG.screen = saved.screen; hasConfig = true; }

var dimOverlay = null, screenOn = true, lastActivity = Date.now(), dimTimer = null;
var lastTap = 0;

// Only create overlay & start timers if config exists with features enabled
if (hasConfig && (CFG.screen.auto_off_enabled || CFG.screen.double_tap_wake || CFG.screen.schedule_enabled)) {
    lvgljs.print("Power mgmt: " + JSON.stringify(CFG.screen));

    dimOverlay = lvgljs.panel(0, 0, W.w, W.h);
    lvgljs.setBgColor(dimOverlay, 0x000000);
    lvgljs.setOpacity(dimOverlay, 0);
    lvgljs.toFront(dimOverlay);
    lvgljs.style(dimOverlay, "visible", 0);

    function dimScreen() {
        if (!screenOn) return;
        lvgljs.backlight(0);
        lvgljs.style(dimOverlay, "visible", 1);
        lvgljs.setOpacity(dimOverlay, 255);
        screenOn = false; lvgljs.print("Screen dimmed");
    }
    function wakeScreen() {
        if (screenOn) return;
        lvgljs.backlight(1);
        lvgljs.setOpacity(dimOverlay, 0);
        lvgljs.style(dimOverlay, "visible", 0);
        screenOn = true; lastActivity = Date.now();
        lvgljs.print("Screen woke");
    }
    function scheduleDim() {
        if (dimTimer) { lvgljs.clearInterval(dimTimer); dimTimer = null; }
        if (!CFG.screen.auto_off_enabled) return;
        // Check every 10s when on, every 1s when off (for prompt wake)
        var interval = screenOn ? 10000 : 1000;
        dimTimer = lvgljs.setInterval(interval, function() {
            if (screenOn) {
                // Auto-dim: never during schedule ON window
                if (CFG.screen.schedule_enabled && inScheduleWindow()) return;
                if (Date.now() - lastActivity >= CFG.screen.auto_off_minutes * 60000)
                    dimScreen();
            } else {
                // Auto-wake: ONLY during schedule ON window
                if (!CFG.screen.schedule_enabled || !inScheduleWindow()) return;
                if (Date.now() - lastActivity >= (CFG.screen.wake_timeout_seconds || 15) * 1000)
                    wakeScreen();
            }
        });
    }

    // Check if current time is within the schedule ON window
    function inScheduleWindow() {
        var onP = CFG.screen.schedule_on.split(":");
        var offP = CFG.screen.schedule_off.split(":");
        var onHM = parseInt(onP[0])*60 + parseInt(onP[1]);
        var offHM = parseInt(offP[0])*60 + parseInt(offP[1]);
        var now = new Date(), hm = now.getHours()*60 + now.getMinutes();
        return hm >= onHM && hm < offHM;
    }

    // Double-tap toggles screen on/off
    // Uses onRelease — C-side stateful debounce guarantees exactly one
    // callback per physical tap (no time-based minimum needed).
    var tapCnt = 0, toggleCooldown = 0;
    lvgljs.onRelease(function() {
        lvgljs.print("tap#" + (++tapCnt) + " dt=" + (Date.now() - lastTap) + "ms cd=" + (Date.now() - toggleCooldown) + "ms");
        lastActivity = Date.now();
        if (!CFG.screen.double_tap_wake) return;
        var now = Date.now();
        if (now - toggleCooldown < 1000) return;
        if (now - lastTap < 500 && lastTap > 0) {
            if (screenOn) { dimScreen(); lvgljs.print("Dbl-tap: dim"); }
            else          { wakeScreen(); lvgljs.print("Dbl-tap: wake"); }
            lastTap = 0;
            toggleCooldown = now;
        } else {
            lastTap = now;
        }
    });

    // Schedule timer
    if (CFG.screen.schedule_enabled) {
        function checkSchedule() {
            if (inScheduleWindow()) {
                if (!screenOn) { wakeScreen(); lvgljs.print("Schedule: wake "+CFG.screen.schedule_on); }
            } else {
                if (screenOn && lastActivity > 0) { dimScreen(); lvgljs.print("Schedule: dim "+CFG.screen.schedule_off); }
            }
        }
        lvgljs.setInterval(60000, checkSchedule);
        // Don't call immediately — let the first interval fire naturally
    }

    scheduleDim();
} else {
    lvgljs.print("Power mgmt: no config, disabled");
}
lvgljs.print("Weather ready [" + L + "]");
