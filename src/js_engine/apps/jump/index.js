// Jump Jump — event-driven, zero idle CPU
var W=lvgljs.getScreenSize(), H=W.h, WW=W.w;
var CX=380; // character fixed X
var G=0.33; // distance per ms factor
var MIN_D=70, MAX_D=320, MIN_T=50, MAX_T=800;

// State
var ST={IDLE:0,CHARGE:1,JUMP:2,LAND:3,DEAD:4}, state=ST.IDLE;
var pressTime=0, score=0, highScore=0, charY=0, platX=0, scrollX=0;
var curPlat=0, jumpTimer=null, jumpStart=0, jumpDist=0, jumpFromX=0, jumpFromY=0;
var bonusTimer=null, bonusPlat=-1;
var platforms=[]; // {x,y,w,h,type,color,centerX,gotBonus}
var charObj, headObj, bodyObj, scoreLbl, highLbl, statusLbl;

// Load high score
var hs=lvgljs.readFile(__dirname+"/highscore.txt");
if(hs)highScore=parseInt(hs)||0;

// Colors
var COLORS_A=["0xE74C3C","0x2ECC71","0xF1C40F","0x9B59B6","0xE67E22","0x1ABC9C","0x3498DB"];
var COLORS_B=["0x7F8C8D","0x95A5A6","0x5D6D7E"];
var COLORS_C=["0x566573","0x6C7A89","0x4A5568"];

// ============================================================
// UI
// ============================================================
lvgljs.screenColor(0x2C3E50);
lvgljs.btn("X",WW-52,8,44,44,function(){lvgljs.exit();});
lvgljs.hideBackButton();

// Score display
scoreLbl=lvgljs.label("0",WW-130,20);
lvgljs.setTextColor(scoreLbl,0xF39C12);lvgljs.setFont(scoreLbl,28);
highLbl=lvgljs.label("Best: "+highScore,WW-200,55);
lvgljs.setTextColor(highLbl,0x7F8C8D);lvgljs.setFont(highLbl,14);
statusLbl=lvgljs.label("Tap to jump",260,600);
lvgljs.setTextColor(statusLbl,0x95A5A6);lvgljs.setFont(statusLbl,22);

// Character: white circle head + triangle body
headObj=lvgljs.panel(CX-12,0,24,24);lvgljs.setBgColor(headObj,0xF39C12);
lvgljs.setRadius(headObj,12);lvgljs.setOpacity(headObj,255);
bodyObj=lvgljs.panel(CX-16,24,32,28);lvgljs.setBgColor(bodyObj,0xF39C12);lvgljs.setOpacity(bodyObj,255);

// Create platform pool
for(var i=0;i<10;i++){
    var p=lvgljs.panel(0,0,110,22);
    lvgljs.setBgColor(p,0x34495E);lvgljs.setOpacity(p,255);lvgljs.setRadius(p,4);
    lvgljs.setVisible(p,0);
    platforms.push({obj:p,x:0,y:0,w:110,h:22,type:2,color:"0xCBD5E1",cx:0,gotBonus:false});
}

// ============================================================
// Platform generation
// ============================================================
function newPlatform(lastX,lastW){
    var gap=70+Math.floor(Math.random()*241); // 70-310
    var t=Math.floor(Math.random()*10);
    var type=2,color="0xCBD5E1",w=110,h=22;
    if(t<3){ // A类: bonus
        type=0;color=COLORS_A[Math.floor(Math.random()*COLORS_A.length)];w=110;h=22;
    }else if(t<6){ // B类: decorative
        type=1;color=COLORS_B[Math.floor(Math.random()*COLORS_B.length)];
        if(Math.random()>0.5){w=140;h=85;}else{w=110;h=22;}
    }else{ // C类: plain
        type=2;color=COLORS_C[Math.floor(Math.random()*COLORS_C.length)];
        if(Math.random()>0.5){w=140;h=85;}else{w=110;h=22;}
    }
    var x=lastX+lastW+gap;
    return {x:x,y:0,w:w,h:h,type:type,color:color,cx:x+w/2,gotBonus:false};
}

// ============================================================
// State transitions
// ============================================================
function setState(s){state=s;}
lvgljs.onPress(function(){
    if(state===ST.IDLE){
        setState(ST.CHARGE);pressTime=Date.now();
        // Compress character
        lvgljs.setPos(headObj,CX-12,5);lvgljs.setPos(bodyObj,CX-16,29);
    }
});
lvgljs.onRelease(function(){
    if(state===ST.CHARGE){
        var elapsed=Date.now()-pressTime;
        if(elapsed<MIN_T)elapsed=MIN_T;if(elapsed>MAX_T)elapsed=MAX_T;
        var dist=MIN_D+(elapsed-MIN_T)*G;
        doJump(dist);setState(ST.JUMP);
    }
});

function doJump(dist){
    jumpDist=dist;jumpStart=Date.now();jumpFromX=CX;jumpFromY=charY||0;
    // Restore character
    lvgljs.setPos(headObj,CX-12,0);lvgljs.setPos(bodyObj,CX-16,24);
    // Start jump timer
    if(jumpTimer)lvgljs.clearInterval(jumpTimer);
    var jumpDur=Math.max(300,dist*1.5); // ms
    var jumpId=lvgljs.setInterval(16,function(){
        var elapsed=Date.now()-jumpStart;
        var t=Math.min(1,elapsed/jumpDur);
        var x=jumpFromX+dist*t;
        // Parabolic arc
        var arcH=120*Math.sin(Math.PI*t);
        var y=jumpFromY-arcH;
        lvgljs.setPos(headObj,x-12,y);lvgljs.setPos(bodyObj,x-16,y+24);
        if(t>=1){
            lvgljs.clearInterval(jumpId);jumpTimer=null;
            checkLanding(dist);
        }
    });
    jumpTimer=jumpId;
}

// ============================================================
// Landing & scoring
// ============================================================
function checkLanding(dist){
    // Find which platform the character landed on
    var charCX=jumpFromX+dist; // center X of character
    var landed=-1;
    for(var i=0;i<platforms.length;i++){
        var p=platforms[i];
        if(!p.obj)continue;
        var px=p.x-scrollX;
        if(charCX>=px&&charCX<=px+p.w&&p.y<=charY&&p.y+p.h>=charY){landed=i;break;}
    }
    if(landed<0){
        doDie();return;
    }
    var p=platforms[landed];
    // Score: center +4, edge +2
    var dx=Math.abs(charCX-(p.cx-scrollX));
    var basePt=dx<=25?4:2;
    score+=basePt;
    lvgljs.setText(scoreLbl,""+score);
    // Scroll
    var targetX=CX-(p.cx-scrollX);
    var scrollAmt=targetX-scrollX;
    scrollX=targetX;
    // Move all platforms
    for(var i=0;i<platforms.length;i++){
        if(platforms[i].obj)lvgljs.setPos(platforms[i].obj,platforms[i].x-scrollX,platforms[i].y);
    }
    // Update character Y to platform top
    charY=p.y;
    lvgljs.setPos(headObj,CX-12,charY-52);lvgljs.setPos(bodyObj,CX-16,charY-28);
    // A类 bonus
    if(p.type===0&&!p.gotBonus){
        p.gotBonus=true;bonusPlat=landed;
        bonusTimer=lvgljs.setInterval(1500,function(){
            lvgljs.clearInterval(bonusTimer);bonusTimer=null;
            score+=3;lvgljs.setText(scoreLbl,""+score);
        });
    }
    // Clean off-screen platforms, generate new
    cleanPlatforms();
    setState(ST.IDLE);
}

function doDie(){
    setState(ST.DEAD);
    lvgljs.setText(statusLbl,"Game Over");
    // Rotate fall animation
    var deg=0,startT=Date.now();
    var dieId=lvgljs.setInterval(30,function(){
        deg=(Date.now()-startT)/1200*90;
        if(deg>=90){deg=90;lvgljs.clearInterval(dieId);resetGame();}
        // Simulate rotation: move head right, body rotates around
        lvgljs.setPos(headObj,CX-12+deg*0.3,charY-52-deg*0.5);
        lvgljs.setPos(bodyObj,CX-16+deg*0.3,charY-28-deg*0.5);
    });
}

function resetGame(){
    if(score>highScore){highScore=score;lvgljs.writeFile(__dirname+"/highscore.txt",""+highScore);lvgljs.setText(highLbl,"Best: "+highScore);}
    score=0;scrollX=0;charY=0;bonusPlat=-1;
    lvgljs.setText(scoreLbl,"0");lvgljs.setText(statusLbl,"Tap to jump");
    lvgljs.setPos(headObj,CX-12,0);lvgljs.setPos(bodyObj,CX-16,24);
    // Reset platforms
    for(var i=0;i<platforms.length;i++){if(platforms[i].obj)lvgljs.setVisible(platforms[i].obj,0);}
    initPlatforms();
    setState(ST.IDLE);
}

// ============================================================
// Platform pool management
// ============================================================
var poolIdx=0;
function getPoolObj(){
    for(var i=0;i<platforms.length;i++){
        if(!platforms[i].visible){
            platforms[i].visible=true;
            return platforms[i];
        }
    }
    return platforms[poolIdx++%platforms.length];
}

function cleanPlatforms(){
    var visible=[];
    for(var i=0;i<platforms.length;i++){
        var p=platforms[i];
        var sx=p.x-scrollX;
        if(sx+p.w<-20){lvgljs.setVisible(p.obj,0);p.visible=false;}
        else visible.push(i);
    }
    // Generate new platforms to the right
    var rightmost=-999;
    for(var i=0;i<platforms.length;i++){
        if(platforms[i].visible){var rx=platforms[i].x+platforms[i].w;if(rx>rightmost)rightmost=rx;}
    }
    while(rightmost-scrollX<WW+200){
        var lastW=110;
        for(var i=platforms.length-1;i>=0;i--){
            if(platforms[i].visible){lastW=platforms[i].w;break;}
        }
        var np=newPlatform(rightmost,0);
        // Find a hidden platform object
        for(var i=0;i<platforms.length;i++){
            if(!platforms[i].visible){
                var po=platforms[i];
                po.x=np.x;po.y=800+Math.random()*40;po.w=np.w;po.h=np.h;po.type=np.type;
                po.color=np.color;po.cx=np.cx;po.gotBonus=false;po.visible=true;
                lvgljs.setPos(po.obj,po.x-scrollX,po.y);
                lvgljs.setSize(po.obj,po.w,po.h);lvgljs.setBgColor(po.obj,parseInt(po.color));
                lvgljs.setVisible(po.obj,1);
                rightmost=np.x+np.w;
                break;
            }
        }
    }
}

function initPlatforms(){
    var lastX=-50,lastW=110;
    // First platform under character
    var p0=getPoolObj();
    p0.x=CX-55;p0.y=720;p0.w=110;p0.h=22;p0.type=1;p0.color="0x64748B";p0.cx=CX;p0.gotBonus=false;p0.visible=true;
    lvgljs.setPos(p0.obj,p0.x,p0.y);lvgljs.setSize(p0.obj,110,22);lvgljs.setBgColor(p0.obj,0xE74C3C);
    lvgljs.setVisible(p0.obj,1);
    lastX=CX-55;lastW=110;
    for(var i=0;i<6;i++){
        var np=newPlatform(lastX,lastW);
        np.y=720+Math.random()*40;
        var po=getPoolObj();
        po.x=np.x;po.y=np.y;po.w=np.w;po.h=np.h;po.type=np.type;po.color=np.color;po.cx=np.cx;po.gotBonus=false;po.visible=true;
        lvgljs.setPos(po.obj,po.x,po.y);lvgljs.setSize(po.obj,po.w,po.h);lvgljs.setBgColor(po.obj,parseInt(po.color));
        lvgljs.setVisible(po.obj,1);
        lastX=np.x;lastW=np.w;
    }
    // Position character on first platform
    charY=p0.y;
    lvgljs.setPos(headObj,CX-12,charY-52);lvgljs.setPos(bodyObj,CX-16,charY-28);
}

// ============================================================
// Start
// ============================================================
initPlatforms();
lvgljs.setFPS(30); lvgljs.print("Jump ready");
