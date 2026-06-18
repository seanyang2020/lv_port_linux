// Snake — 3 difficulty levels, draggable D-pad
var SN="SNAKE16";
var W=lvgljs.getScreenSize(),WW=W.w,WH=W.h;
var GAMEW=WW-20; // full screen width
var GS=22,GX=10,GY=70,GW=Math.floor(GAMEW/GS),GH=Math.floor((WH-290)/GS);
var levels=[{name:"Easy",speed:250},{name:"Normal",speed:180},{name:"Hard",speed:120}];
var difficulty=levels[1]; // Normal
var DIR={UP:[0,-1],DOWN:[0,1],LEFT:[-1,0],RIGHT:[1,0]};
var dir=DIR.RIGHT,nxDir=DIR.RIGHT;
var snake=[],score=0,best=0,timer=null,state="IDLE";
var snakeObs=[],foods=[],foodObs=[],scoreLbl,bestLbl,statusLbl;
var colors=["0x2ECC71","0x27AE60","0x1ABC9C","0x16A085","0x2ECC71"];

var hs=lvgljs.readFile(__dirname+"/snake_high.txt");if(hs)best=parseInt(hs)||0;

// UI
lvgljs.screenColor(0x0F0F23);
lvgljs.btn("X",WW-52,8,44,44,function(){lvgljs.exit();});lvgljs.hideBackButton();
var bb=lvgljs.panel(GX-4,GY-4,GW*GS+8,GH*GS+8);lvgljs.setBgColor(bb,0x0A0A1A);lvgljs.setOpacity(bb,255);lvgljs.setBorder(bb,2,0x4ECDC4);
scoreLbl=lvgljs.label("Score: 0",14,8);lvgljs.setTextColor(scoreLbl,0x4ECDC4);lvgljs.setFont(scoreLbl,22);
bestLbl=lvgljs.label("Best: "+best,14,34);lvgljs.setTextColor(bestLbl,0x7F8C8D);lvgljs.setFont(bestLbl,13);
statusLbl=lvgljs.label("",180,WH-70);lvgljs.setTextColor(statusLbl,0x5D6D7E);lvgljs.setFont(statusLbl,15);
// Difficulty buttons (bottom-left, full name on button)
var lvlBtns=[];
for(var l=0;l<3;l++){(function(lv){
    var b=lvgljs.btn(levels[lv].name,16,WH-80-lv*30,80,22);
    lvlBtns[lv]=b;
    lvgljs.setBgColor(b,lv===1?0xE74C3C:0x34495E);lvgljs.setTextColor(b,0xECF0F1);lvgljs.setFont(b,12);lvgljs.setRadius(b,4);
    lvgljs.onClick(b,function(){
        difficulty=levels[lv];lvgljs.print("LVL="+difficulty.name+" speed="+difficulty.speed);
        for(var k=0;k<3;k++)lvgljs.setBgColor(lvlBtns[k],k===lv?0xE74C3C:0x34495E);
    });
})(l);}

// Snake & food pools
for(var i=0;i<400;i++){var s=lvgljs.panel(0,0,GS-2,GS-2);lvgljs.setRadius(s,4);lvgljs.setVisible(s,0);snakeObs.push(s);}
for(var i=0;i<5;i++){var f=lvgljs.panel(0,0,GS-2,GS-2);lvgljs.setRadius(f,GS/2);lvgljs.setVisible(f,0);foodObs.push(f);foods.push(null);}

// D-pad
var DPX=WW-220,DPY=WH-210; // right side, larger
var dpBg=lvgljs.panel(DPX,DPY,200,180);lvgljs.setBgColor(dpBg,0x16213E);lvgljs.setOpacity(dpBg,200);lvgljs.setRadius(dpBg,16);
lvgljs.onClick(dpBg,function(){if(state==="OVER"){reset();}});
function mkb(dx,dy,w,h,t,d){
    var b=lvgljs.btn(t,DPX+dx,DPY+dy,w,h);lvgljs.setBgColor(b,0x2C3E50);lvgljs.setTextColor(b,0xECF0F1);lvgljs.setFont(b,24);lvgljs.setRadius(b,10);
    lvgljs.onClick(b,function(){go(d);});return b;
}
mkb(75,10,55,55,"▲",DIR.UP);mkb(75,115,55,55,"▼",DIR.DOWN);
mkb(10,60,55,55,"◀",DIR.LEFT);mkb(140,60,55,55,"▶",DIR.RIGHT);

function go(d){
    if(state==="IDLE"||state==="OVER")reset();
    // Prevent reverse-direction (e.g. RIGHT→LEFT kills instantly)
    if(dir[0]+d[0]!==0||dir[1]+d[1]!==0)nxDir=d;
}

// ============================================================
function spawnFoods(){
    for(var j=0;j<foodObs.length;j++){lvgljs.setVisible(foodObs[j],0);foods[j]=null;}
    var n=1+Math.floor(Math.random()*3);
    for(var j=0;j<n;j++){
        var ok=false,fx,fy;
        while(!ok){fx=Math.floor(Math.random()*GW);fy=Math.floor(Math.random()*GH);ok=true;
            for(var k=0;k<snake.length;k++){if(snake[k].x===fx&&snake[k].y===fy){ok=false;break;}}
            for(var k=0;k<j;k++){if(foods[k]&&foods[k].x===fx&&foods[k].y===fy){ok=false;break;}}
        }
        foods[j]={x:fx,y:fy};lvgljs.setPos(foodObs[j],GX+fx*GS,GY+fy*GS);
        lvgljs.setBgColor(foodObs[j],j===0?0xFF6B6B:j===1?0xFFE66D:0x4ECDC4);lvgljs.setVisible(foodObs[j],1);
    }
}
var lastMove=0;
// Single persistent timer at high rate, throttle game ticks
if(!timer)timer=lvgljs.setInterval(30,function(){
    if(state!=="PLAYING")return;
    var now=Date.now();
    if(now-lastMove<difficulty.speed)return;
    lastMove=now;
    dir=nxDir;var h=snake[0],nx=h.x+dir[0],ny=h.y+dir[1];
    if(nx<0||nx>=GW||ny<0||ny>=GH){gameOver();return;}
    for(var i=0;i<snake.length;i++){if(snake[i].x===nx&&snake[i].y===ny){gameOver();return;}}
    snake.unshift({x:nx,y:ny});
    var ate=-1;for(var j=0;j<foods.length;j++){if(foods[j]&&nx===foods[j].x&&ny===foods[j].y){ate=j;break;}}
    if(ate>=0){
        score+=(ate+1)*5;lvgljs.setText(scoreLbl,"Score: "+score);
        foods[ate]=null;lvgljs.setVisible(foodObs[ate],0);
        var ag=true;for(var j=0;j<foods.length;j++){if(foods[j])ag=false;}if(ag)spawnFoods();
    }else{snake.pop();}
    renderSnake();
});
function gameOver(){
    state="OVER";
    lvgljs.setText(statusLbl,"Game Over! Tap pad");
    if(score>best){best=score;lvgljs.writeFile(__dirname+"/snake_high.txt",""+best);lvgljs.setText(bestLbl,"Best: "+best);}
}
function reset(){
    var my=Math.floor(GH/2);
    snake=[{x:5,y:my},{x:4,y:my},{x:3,y:my},{x:2,y:my}];dir=nxDir=DIR.RIGHT;
    score=0;spawnFoods();lvgljs.setText(scoreLbl,"Score: 0");state="PLAYING";
}
function renderSnake(){
    for(var i=0;i<snakeObs.length;i++){lvgljs.setVisible(snakeObs[i],0);}
    for(var i=0;i<snake.length&&i<snakeObs.length;i++){
        var s=snakeObs[i];lvgljs.setBgColor(s,parseInt(colors[i%colors.length]));
        lvgljs.setPos(s,GX+snake[i].x*GS+1,GY+snake[i].y*GS+1);lvgljs.setVisible(s,1);
    }
}
// Init
lvgljs.setFPS(20);renderSnake();
