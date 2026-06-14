// Weather + Calendar — Chinese text with CJK font

var SKIN = __dirname + "/skin/";
var W = lvgljs.getScreenSize();
var isLand = W.w > W.h;
var CJK = "cjk";  // font name for lvgljs.setFont
var M14 = 14, M16 = 16, M22 = 22, M30 = 30, M36 = 36, M48 = 48;

var wx = { city: "上海", temp: 22, cond: "晴天", icon: "sunny" };
var fc = [
    { d: "今天", i: "sunny",    hi: 22, lo: 15 },
    { d: "明天", i: "cloudy",   hi: 20, lo: 14 },
    { d: "后天", i: "sunny",    hi: 23, lo: 16 },
    { d: "周五", i: "overcast", hi: 19, lo: 13 }
];
var lunarDays = ["初一","初二","初三","初四","初五","初六","初七","初八","初九","初十",
    "十一","十二","十三","十四","十五","十六","十七","十八","十九","二十",
    "廿一","廿二","廿三","廿四","廿五","廿六","廿七","廿八","廿九","三十"];
var weekDays = ["周日","周一","周二","周三","周四","周五","周六"];
var yiJi = [
    ["嫁娶","栽种"],["出行","开市"],["交易","入宅"],["安床","移徙"],
    ["造桥","安葬"],["动土","破土"],["开渠","放水"],["修造","伐木"]
];
function lunarDay(y,m,d) { return ((y+m*13+d*7)%30)+1; }
function icon(name, small) { return SKIN+"weather_"+name+(small?"_small":"")+".png"; }

// Screen
lvgljs.screenColor(0xD2E0EB);
var closeImg = lvgljs.image(SKIN+"weather_close.png", W.w-68, 8, 60, 60);
lvgljs.onClick(closeImg, function(){ lvgljs.exit(); });

// Panel dimensions
var m = 20, gap = 24;
var panW = isLand ? 598 : W.w - m*2;
var wxH  = isLand ? 700 : 550;
var calY = isLand ? m+gap : m+wxH+gap;
var calH = isLand ? 700 : W.h - calY - m;

// ==== Weather Panel ====
var wxPan = lvgljs.panel(m, m, panW, wxH);
lvgljs.setBgColor(wxPan, 0xFFFFFF); lvgljs.setOpacity(wxPan, 200); lvgljs.setRadius(wxPan, 28);

var dateLbl  = lvgljs.label("", 30, 30, wxPan);
lvgljs.setTextColor(dateLbl, 0x172335); lvgljs.setFont(dateLbl, M22); lvgljs.setOpacity(dateLbl, 150);

var lunarLbl = lvgljs.label("", 220, 30, wxPan);
lvgljs.setTextColor(lunarLbl, 0x172335); lvgljs.setFont(lunarLbl, CJK); lvgljs.setOpacity(lunarLbl, 150);

var timeLbl  = lvgljs.label("", 30, 86, wxPan);
lvgljs.setTextColor(timeLbl, 0x000000); lvgljs.setFont(timeLbl, M36);

lvgljs.image(icon(wx.icon), isLand?219:290, isLand?235:140, 160, 160, wxPan);

var cityLbl  = lvgljs.label(wx.city, isLand?420:540, 40, wxPan);
lvgljs.setTextColor(cityLbl, 0x172335); lvgljs.setFont(cityLbl, CJK);

// Location icon
lvgljs.image(SKIN+"weather_location.png", isLand?538:686, isLand?43:24, 30, 30, wxPan);

var tempLbl  = lvgljs.label(wx.temp+"°C", isLand?372:500, isLand?87:80, wxPan);
lvgljs.setTextColor(tempLbl, 0x172335); lvgljs.setFont(tempLbl, M48);

var condLbl  = lvgljs.label(wx.cond, isLand?235:310, isLand?410:300, wxPan);
lvgljs.setTextColor(condLbl, 0x172335); lvgljs.setFont(condLbl, CJK);

// Forecast
var fcIcons=[], fcLabels=[];
var fcX=30, fcY=isLand?550:400, fcW=isLand?120:150, fcGap=isLand?20:30;
for (var i=0; i<4; i++) {
    var x=fcX+i*(fcW+fcGap);
    fcLabels.push(lvgljs.label(fc[i].d, x+30, fcY+12, wxPan));
    fcIcons.push(lvgljs.image(icon(fc[i].i,true), x+35, fcY+58, 50, 50, wxPan));
    var tl=lvgljs.label(fc[i].hi+"/"+fc[i].lo, x+30, fcY+115, wxPan);
    lvgljs.setTextColor(tl, 0x666666); lvgljs.setFont(tl, M14);
}
for (var j=0; j<fcLabels.length; j++) {
    lvgljs.setTextColor(fcLabels[j], 0x172335); lvgljs.setFont(fcLabels[j], CJK);
}

// ==== Calendar Panel ====
var calPan = lvgljs.panel(isLand?652:m, calY, panW, calH);
lvgljs.setBgColor(calPan, 0xFFFFFF); lvgljs.setOpacity(calPan, 200); lvgljs.setRadius(calPan, 28);

var calTitle = lvgljs.label("", isLand?199:panW/2-60, 40, calPan);
lvgljs.setTextColor(calTitle, 0x172335); lvgljs.setFont(calTitle, CJK);

var weekHdr  = lvgljs.label("日 一 二 三 四 五 六", isLand?40:25, 90, calPan);
lvgljs.setTextColor(weekHdr, 0x999999); lvgljs.setFont(weekHdr, CJK);

var dayLabels = [];
var cw=isLand?73:Math.floor((panW-50)/7), rh=isLand?60:Math.floor((calH-160)/6);
var gx=isLand?26:25, gy=isLand?117:120;
for (var row=0; row<6; row++)
    for (var col=0; col<7; col++)
        dayLabels.push(lvgljs.label("", gx+col*cw, gy+row*rh, calPan));

var hlLabel = lvgljs.label("", isLand?50:25, gy+6*rh+15, calPan);
lvgljs.setTextColor(hlLabel, 0x666666); lvgljs.setFont(hlLabel, CJK);

// ==== Update ====
function updateAll() {
    var now = new Date();
    var y=now.getFullYear(), m=now.getMonth()+1, d=now.getDate();
    lvgljs.setText(dateLbl, y+"年"+(m<10?"0":"")+m+"月"+(d<10?"0":"")+d+"日 "+weekDays[now.getDay()]);
    var h=now.getHours(), mi=now.getMinutes(), s=now.getSeconds();
    lvgljs.setText(timeLbl, (h<10?"0":"")+h+":"+(mi<10?"0":"")+mi+":"+(s<10?"0":"")+s);
    lvgljs.setText(lunarLbl, "农历"+lunarDays[lunarDay(y,m,d)-1]);

    lvgljs.setText(calTitle, y+"年"+(m<10?"0":"")+m+"月");
    var swd=new Date(y,m-1,1).getDay(), dim=new Date(y,m,0).getDate();
    for (var i=0; i<42; i++) {
        var dn=i-swd+1;
        if (dn>=1 && dn<=dim) {
            lvgljs.setText(dayLabels[i], dn+"\n"+lunarDays[lunarDay(y,m,dn)-1]);
            lvgljs.setFont(dayLabels[i], CJK);
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
    lvgljs.setText(hlLabel, "宜: "+yiJi[idx][0]+" "+yiJi[(idx+1)%8][0]+
        "  忌: "+yiJi[(idx+2)%8][0]+" "+yiJi[(idx+3)%8][0]);
}

updateAll();
lvgljs.setInterval(1000, updateAll);
lvgljs.print("Weather ready");
