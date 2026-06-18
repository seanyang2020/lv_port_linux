// Jump Jump v4 — 30fps, varied heights, keep landing pos
var SN="SN026";
var W=lvgljs.getScreenSize(), WW=W.w;
var CX=400, BASE_Y=680;
var CHARGE_MAX=800, DIST_MIN=60, DIST_MAX=300;

var ST={IDLE:0,CHARGING:1,JUMPING:2,FALLING:3}, state=ST.IDLE;
var score=0, bestScore=0, scoreLabel;
var chargeStart=0;
var charX=0, charY=0, cameraX=0;
var platPool=[], charHead, charBody, statusLabel;
var nextPlatIdx=0, lastPlatRight=0;
var animTimer=null, chargeTimer=null;

var hs=lvgljs.readFile(__dirname+"/highscore.txt");
if(hs)bestScore=parseInt(hs)||0;

var colors=["0xE74C3C","0x2ECC71","0xF1C40F","0x9B59B6","0xE67E22","0x1ABC9C","0x3498DB","0xE91E63","0x00BCD4","0xFF9800"];

// UI
lvgljs.screenColor(0x2C3E50);
lvgljs.btn("X",WW-52,8,44,44,function(){lvgljs.exit();});lvgljs.hideBackButton();
scoreLabel=lvgljs.label("0",WW-130,30);lvgljs.setTextColor(scoreLabel,0xF39C12);lvgljs.setFont(scoreLabel,28);
var hsl=lvgljs.label("Best: "+bestScore,WW-200,65);lvgljs.setTextColor(hsl,0x7F8C8D);lvgljs.setFont(hsl,14);
statusLabel=lvgljs.label("Hold to charge, release to jump",160,500);lvgljs.setTextColor(statusLabel,0x95A5A6);lvgljs.setFont(statusLabel,18);

charHead=lvgljs.panel(CX-12,0,24,24);lvgljs.setBgColor(charHead,0xFF69B4);lvgljs.setRadius(charHead,12);
charBody=lvgljs.panel(CX-16,24,32,28);lvgljs.setBgColor(charBody,0xFF69B4);

// Decor dots pool
var decorPool=[];for(var i=0;i<80;i++){var d=lvgljs.panel(0,0,8,8);lvgljs.setRadius(d,4);lvgljs.setVisible(d,0);decorPool.push({obj:d,used:false});}
function addDecor(p){
    var n=1+Math.floor(Math.random()*3); // 1-3 decorations
    var dc=["0xFF6B6B","0x4ECDC4","0xFFE66D","0xA8E6CF","0xFF8A5C","0x98D8C8","0xF7DC6F","0xBB8FCE","0x85C1E9","0xF1948A","0x73C6B6","0xF9E79F"];
    for(var j=0;j<n;j++){
        var dd=null;for(var k=0;k<decorPool.length;k++){if(!decorPool[k].used){decorPool[k].used=true;dd=decorPool[k];break;}}
        if(!dd)continue;
        var dx=10+Math.floor(Math.random()*(p.w-20));
        var dy=-8-Math.floor(Math.random()*12);
        dd.wx=p.wx+dx;dd.wy=p.wy+dy;dd.color=dc[Math.floor(Math.random()*dc.length)];
        dd.plat=p; // link to platform
    }
}
function renderDecor(dd){var sx=dd.wx-cameraX;lvgljs.setPos(dd.obj,sx,dd.wy);lvgljs.setBgColor(dd.obj,parseInt(dd.color));lvgljs.setVisible(dd.obj,1);}
function removeDecor(p){for(var i=0;i<decorPool.length;i++){if(decorPool[i].plat===p){lvgljs.setVisible(decorPool[i].obj,0);decorPool[i].used=false;decorPool[i].plat=null;}}}

// Platform pool
for(var i=0;i<25;i++){
    var p=lvgljs.panel(0,0,100,24);lvgljs.setBgColor(p,0x34495E);lvgljs.setRadius(p,4);lvgljs.setVisible(p,0);
    platPool.push({obj:p,wx:0,wy:BASE_Y,w:100,h:24,used:false});
}

// ============================================================
// Platform helpers
// ============================================================
function makePlat(rightEdge){
    var w=80+Math.floor(Math.random()*80);
    var h=24+Math.floor(Math.random()*60);
    var gap=40+Math.floor(Math.random()*110);
    var ci=Math.floor(Math.random()*colors.length);
    return {wx:rightEdge+gap,wy:BASE_Y+Math.floor(Math.random()*40)-20,w:w,h:h,cx:rightEdge+gap+w/2,color:colors[ci]};
}
function allocPlat(){
    for(var i=0;i<platPool.length;i++){if(!platPool[i].used){platPool[i].used=true;return platPool[i];}}
    var p=lvgljs.panel(0,0,100,24);lvgljs.setBgColor(p,0x34495E);lvgljs.setRadius(p,4);lvgljs.setVisible(p,0);
    var np={obj:p,wx:0,wy:BASE_Y,w:100,h:24,used:true};platPool.push(np);return np;
}
function renderPlat(p){var sx=p.wx-cameraX;lvgljs.setPos(p.obj,sx,p.wy);lvgljs.setSize(p.obj,p.w,p.h);lvgljs.setBgColor(p.obj,parseInt(p.color));lvgljs.setVisible(p.obj,1);}
function renderAll(){for(var i=0;i<platPool.length;i++){if(platPool[i].used)renderPlat(platPool[i]);}for(var i=0;i<decorPool.length;i++){if(decorPool[i].used)renderDecor(decorPool[i]);}}
function cleanPlats(){
    for(var i=0;i<platPool.length;i++){if(platPool[i].used&&platPool[i].wx+platPool[i].w<cameraX-50){lvgljs.setVisible(platPool[i].obj,0);removeDecor(platPool[i]);platPool[i].used=false;}}
    for(var i=0;i<platPool.length;i++){if(platPool[i].used&&platPool[i].wx+platPool[i].w<cameraX-50){lvgljs.setVisible(platPool[i].obj,0);platPool[i].used=false;}}
    var rm=-999;for(var i=0;i<platPool.length;i++){if(platPool[i].used){var r=platPool[i].wx+platPool[i].w;if(r>rm)rm=r;}}
    while(rm-cameraX<WW+200){
        var np=makePlat(rm);var po=allocPlat();po.wx=np.wx;po.wy=np.wy;po.w=np.w;po.h=np.h;po.cx=np.cx;po.color=np.color;po.used=true;renderPlat(po);addDecor(po);
        rm=po.wx+po.w;
    }
}

// ============================================================
// Init
// ============================================================
function initGame(){
    score=0;lvgljs.setText(scoreLabel,"0");lvgljs.setText(statusLabel,"");
    var p0=allocPlat();p0.wx=CX-50;p0.wy=BASE_Y;p0.w=100;p0.h=24;p0.cx=CX;p0.color="0x64748B";p0.used=true;
    lastPlatRight=p0.wx+p0.w;
    for(var i=0;i<10;i++){var np=makePlat(lastPlatRight);var po=allocPlat();po.wx=np.wx;po.wy=np.wy;po.w=np.w;po.h=np.h;po.cx=np.cx;po.color=np.color;po.used=true;lastPlatRight=po.wx+po.w;}
    charX=CX;charY=BASE_Y;cameraX=0;
    renderAll();
    lvgljs.setPos(charHead,CX-12,BASE_Y-52);lvgljs.setPos(charBody,CX-16,BASE_Y-28);
    state=ST.IDLE;
}

// ============================================================
// Input
// ============================================================
lvgljs.onPress(function(){if(state===ST.IDLE){state=ST.CHARGING;chargeStart=Date.now();}});
lvgljs.onRelease(function(){
    if(state===ST.CHARGING){
        var elapsed=Date.now()-chargeStart;if(elapsed>CHARGE_MAX)elapsed=CHARGE_MAX;
        var power=elapsed/CHARGE_MAX;
        var dist=DIST_MIN+Math.round(power*(DIST_MAX-DIST_MIN));
        lvgljs.print("JUMP p="+Math.round(power*100)+"% d="+dist);
        doJump(dist);
    }
});

// ============================================================
// Jump
// ============================================================
function doJump(dist){
    state=ST.JUMPING;
    var fromX=charX, fromY=charY, fromCam=cameraX;
    var landX=fromX+dist;
    var dur=Math.max(500,dist*2.0);
    var startT=Date.now();
    // Keep character at current screen position (not center!)
    lvgljs.setPos(charHead,fromX-fromCam-12,fromY-52);
    lvgljs.setPos(charBody,fromX-fromCam-16,fromY-28);
    if(animTimer)lvgljs.clearInterval(animTimer);
    animTimer=lvgljs.setInterval(33,function(){
        var t=Math.min(1,(Date.now()-startT)/dur);
        var et=t<0.5?2*t*t:-1+(4-2*t)*t;
        var wx=fromX+dist*et;
        // Gentle camera follow
        cameraX=fromCam+(landX-CX-fromCam)*et*0.6;
        var sy=fromY-200*Math.sin(Math.PI*et);
        renderAll();
        lvgljs.setPos(charHead,wx-cameraX-12,sy-52);
        lvgljs.setPos(charBody,wx-cameraX-16,sy-28);
        if(t>=1){lvgljs.clearInterval(animTimer);animTimer=null;checkLand(landX,fromY);}
    });
}

function checkLand(landX,fromY){
    for(var i=0;i<platPool.length;i++){
        var p=platPool[i];if(!p.used)continue;
        if(landX>=p.wx&&landX<=p.wx+p.w&&fromY>=p.wy-60&&fromY<=p.wy+p.h+60){
            var dx=Math.abs(landX-p.cx),pt=dx<=25?4:2;
            score+=pt;lvgljs.setText(scoreLabel,""+score);
            lvgljs.print("OK +"+pt+" s="+score);
            charX=landX;charY=p.wy;cameraX=landX-CX;
            renderAll();
            lvgljs.setPos(charHead,landX-cameraX-12,charY-52);
            lvgljs.setPos(charBody,landX-cameraX-16,charY-28);
            cleanPlats();state=ST.IDLE;
            return;
        }
    }
    doFall();
}

function doFall(){
    state=ST.FALLING;lvgljs.setText(statusLabel,"Game Over");
    var startT=Date.now(),fallId=lvgljs.setInterval(33,function(){
        var t=(Date.now()-startT)/1000;
        lvgljs.setPos(charHead,charX-cameraX-12+t*30,charY-52+t*t*300);
        lvgljs.setPos(charBody,charX-cameraX-16+t*30,charY-28+t*t*300);
        if(t>1.5){lvgljs.clearInterval(fallId);
            if(score>bestScore){bestScore=score;lvgljs.writeFile(__dirname+"/highscore.txt",""+bestScore);lvgljs.setText(hsl,"Best: "+bestScore);}
            for(var i=0;i<platPool.length;i++){lvgljs.setVisible(platPool[i].obj,0);platPool[i].used=false;}
            initGame();
        }
    });
}

// Charge visual
chargeTimer=lvgljs.setInterval(50,function(){
    if(state!==ST.CHARGING)return;
    var power=Math.min(1,(Date.now()-chargeStart)/CHARGE_MAX);
    var sq=Math.round(power*8);
    var sx=charX-cameraX;
    lvgljs.setPos(charHead,sx-12,charY-52+sq);
    lvgljs.setPos(charBody,sx-16,charY-28+sq);
});

initGame();lvgljs.setFPS(30);lvgljs.print("Jump ready "+SN);
