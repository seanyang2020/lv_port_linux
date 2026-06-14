// Sudoku — 1280×800, popup picker, multi-save support
var W = lvgljs.getScreenSize();
var DIR = __dirname + "/save";
var IDX_FILE = DIR + "/saves.json";

var T = {
    bg: 0xF0F0F0, dark: 0xE0E4EC, light: 0xF5F6FA,
    grid: 0x888888, sel: 0xBBC8E8, same: 0xD0D8F0,
    conflict: 0xFFCCCC, hint: 0x111111, user: 0x3366CC, accent: 0x224488
};

// Engine
var board=[], solution=[], fixed=[];
var selectedR=-1, selectedC=-1, difficulty=1, filled=0;
var cluesCount=[35,28,22], diffLabels=["Easy","Med.","Hard"];
var timerSec=0, currentSave="";

function initBoard(){
    board=[];solution=[];fixed=[];
    for(var r=0;r<9;r++){board[r]=[];solution[r]=[];fixed[r]=[];
        for(var c=0;c<9;c++){board[r][c]=0;solution[r][c]=0;fixed[r][c]=false;}}
}
function copyBoard(s,d){for(var r=0;r<9;r++)for(var c=0;c<9;c++)d[r][c]=s[r][c];}
function isValid(g,r,c,n){
    for(var i=0;i<9;i++){if(g[r][i]===n)return false;if(g[i][c]===n)return false;}
    var br=Math.floor(r/3)*3,bc=Math.floor(c/3)*3;
    for(var i=0;i<3;i++)for(var j=0;j<3;j++)if(g[br+i][bc+j]===n)return false;
    return true;
}
function shuffle(a){for(var i=a.length-1;i>0;i--){var j=Math.floor(Math.random()*(i+1));var t=a[i];a[i]=a[j];a[j]=t;}}
function solve(g){
    for(var r=0;r<9;r++)for(var c=0;c<9;c++)if(g[r][c]===0){
        var nums=[1,2,3,4,5,6,7,8,9];shuffle(nums);
        for(var i=0;i<9;i++)if(isValid(g,r,c,nums[i])){g[r][c]=nums[i];if(solve(g))return true;g[r][c]=0;}
        return false;
    }
    return true;
}
function generate(diff){
    initBoard();
    for(var b=0;b<9;b+=3){var nums=[1,2,3,4,5,6,7,8,9];shuffle(nums);
        for(var i=0;i<3;i++)for(var j=0;j<3;j++)solution[b+i][b+j]=nums[i*3+j];}
    solve(solution);copyBoard(solution,board);
    var cells=[],toRemove=81-cluesCount[diff];
    for(var r=0;r<9;r++)for(var c=0;c<9;c++)cells.push([r,c]);
    shuffle(cells);
    for(var i=0;i<toRemove;i++)board[cells[i][0]][cells[i][1]]=0;
    for(var r=0;r<9;r++)for(var c=0;c<9;c++)fixed[r][c]=(board[r][c]!==0);
    filled=81-toRemove;selectedR=selectedC=-1;
}

// Save index
function loadIndex(){
    var d=lvgljs.readFile(IDX_FILE);
    if(!d||d.length<5)return[];
    try{return JSON.parse(d);}catch(e){return[];}
}
function saveIndex(idx){lvgljs.writeFile(IDX_FILE,JSON.stringify(idx));}

// Save/Load
function saveGame(){
    var name=lvgljs.getText(saveNameBox).trim();
    if(!name){lvgljs.print("Enter a save name");return;}
    var idx=loadIndex();
    var now=new Date();
    var meta={name:name,difficulty:difficulty,filled:filled,date:now.toISOString().slice(0,16),cells:81-filled};
    // Update or add
    var found=false;
    for(var i=0;i<idx.length;i++){if(idx[i].name===name){idx[i]=meta;found=true;break;}}
    if(!found)idx.push(meta);
    saveIndex(idx);
    // Save game data
    var data={board:board,solution:solution,fixed:fixed,difficulty:difficulty,filled:filled,date:meta.date};
    var file=DIR+"/save_"+name+".json";
    lvgljs.writeFile(file,JSON.stringify(data));
    currentSave=name;
    lvgljs.print("Saved: "+name+" ("+diffLabels[difficulty]+", "+meta.cells+" empty)");
}

function loadGame(name){
    if(!name){
        // Show load popup
        var idx=loadIndex();
        if(idx.length===0){lvgljs.print("No saves found");return;}
        showLoadPopup(idx);
        return;
    }
    var file=DIR+"/save_"+name+".json";
    var d=lvgljs.readFile(file);
    if(!d||d.length<10){lvgljs.print("Save not found: "+name);return false;}
    try{
        var data=JSON.parse(d);
        initBoard();
        for(var r=0;r<9;r++)for(var c=0;c<9;c++){board[r][c]=data.board[r][c];fixed[r][c]=data.fixed[r][c];}
        solution=data.solution;difficulty=data.difficulty||1;filled=data.filled||0;
        selectedR=selectedC=-1;currentSave=name;timerSec=0;
        lvgljs.print("Loaded: "+name+" ("+diffLabels[difficulty]+", "+(data.date||"")+")");
        return true;
    }catch(e){lvgljs.print("Load failed: "+e);return false;}
}

function deleteSave(name){
    if(!name){lvgljs.print("Enter save name to delete");return;}
    var idx=loadIndex(),newIdx=[];
    for(var i=0;i<idx.length;i++)if(idx[i].name!==name)newIdx.push(idx[i]);
    saveIndex(newIdx);
    lvgljs.deleteFile(DIR+"/save_"+name+".json");
    if(currentSave===name)currentSave="";
    lvgljs.print("Deleted: "+name);
}

// ============================================================
// UI
// ============================================================
lvgljs.screenColor(T.bg);

// Top bar
var topBar=lvgljs.panel(0,0,W.w,56);
lvgljs.setBgColor(topBar,0x2a2a3e);lvgljs.setOpacity(topBar,255);lvgljs.setRadius(topBar,0);
lvgljs.label("Sudoku",20,12,topBar);lvgljs.setTextColor(topBar,0xFFFFFF);

var diffBtns=[];
for(var d=0;d<3;d++){
    var db=lvgljs.btn(diffLabels[d],200+d*100,8,90,40,function(idx){return function(){difficulty=idx;updateDiffBtns();generate(difficulty);renderBoard();updateStatus();timerSec=0;};}(d),topBar);
    lvgljs.setRadius(db,6);lvgljs.setFont(db,16);diffBtns.push(db);
}
var timerLbl=lvgljs.label("00:00",W.w-280,12,topBar);
lvgljs.setTextColor(timerLbl,0xFFFFFF);lvgljs.setFont(timerLbl,22);
var closeBtn=lvgljs.btn("X",W.w-56,6,44,44,function(){lvgljs.exit();});
lvgljs.setRadius(closeBtn,22);lvgljs.setBgColor(closeBtn,0x333333);lvgljs.setOpacity(closeBtn,120);lvgljs.setTextColor(closeBtn,0xFFFFFF);

// Board
var boardX=40,boardY=70,cellSize=64;
var boardBg=lvgljs.panel(boardX-4,boardY-4,cellSize*9+8,cellSize*9+8);
lvgljs.setBgColor(boardBg,T.dark);lvgljs.setOpacity(boardBg,255);lvgljs.setRadius(boardBg,4);

var cells=[],cellLabels=[];
for(var r=0;r<9;r++){cells[r]=[];cellLabels[r]=[];
    for(var c=0;c<9;c++){
        var x=boardX+c*cellSize,y=boardY+r*cellSize;
        var cell=lvgljs.panel(x,y,cellSize-1,cellSize-1);
        var region=Math.floor(r/3)*3+Math.floor(c/3);
        lvgljs.setBgColor(cell,region%2===0?T.light:T.dark);lvgljs.setOpacity(cell,255);lvgljs.setRadius(cell,0);
        lvgljs.onClick(cell,function(rr,cc){return function(){
            if(selectedR===rr&&selectedC===cc){selectedR=selectedC=-1;hidePopup();}
            else{selectedR=rr;selectedC=cc;showPopup(rr,cc);}
            renderBoard();
        };}(r,c));
        var lbl=lvgljs.label("",0,0,cell);lvgljs.setPos(lbl,cellSize/2-8,cellSize/2-12);lvgljs.setFont(lbl,28);
        cellLabels[r][c]=lbl;cells[r][c]=cell;
    }
}
// Region borders
for(var i=1;i<3;i++){
    var hLine=lvgljs.panel(boardX,boardY+i*cellSize*3-2,cellSize*9,4);
    lvgljs.setBgColor(hLine,T.accent);lvgljs.setOpacity(hLine,255);lvgljs.setRadius(hLine,0);lvgljs.toFront(hLine);
    var vLine=lvgljs.panel(boardX+i*cellSize*3-2,boardY,4,cellSize*9);
    lvgljs.setBgColor(vLine,T.accent);lvgljs.setOpacity(vLine,255);lvgljs.setRadius(vLine,0);lvgljs.toFront(vLine);
}

// Popup picker
var popupPanel,popupBtns=[];
function createPopup(){
    popupPanel=lvgljs.panel(0,0,156,210);lvgljs.setBgColor(popupPanel,0x333333);lvgljs.setOpacity(popupPanel,210);lvgljs.setRadius(popupPanel,10);lvgljs.setVisible(popupPanel,0);lvgljs.toFront(popupPanel);
    for(var i=0;i<9;i++){
        var px=8+(i%3)*50,py=8+Math.floor(i/3)*50;
        var btn=lvgljs.btn(""+(i+1),px,py,44,44,function(n){return function(){placeNumber(n);};}(i+1),popupPanel);
        lvgljs.setBgColor(btn,0x555555);lvgljs.setRadius(btn,8);lvgljs.setTextColor(btn,0xFFFFFF);lvgljs.setFont(btn,20);popupBtns.push(btn);
    }
    lvgljs.btn("Erase",8,160,138,40,function(){placeNumber(0);},popupPanel);lvgljs.setBgColor(popupPanel,0x663333);
}
createPopup();
function showPopup(r,c){
    var px=boardX+c*cellSize-45,py=boardY+r*cellSize+cellSize+4;
    if(px<10)px=10;if(px>W.w-170)px=W.w-170;if(py>W.h-230)py=boardY+r*cellSize-220;
    lvgljs.setPos(popupPanel,px,py);lvgljs.setVisible(popupPanel,1);
}
function hidePopup(){lvgljs.setVisible(popupPanel,0);}
function placeNumber(n){
    if(selectedR<0||selectedC<0||fixed[selectedR][selectedC]){hidePopup();return;}
    if(n===0){if(board[selectedR][selectedC]!==0)filled--;board[selectedR][selectedC]=0;}
    else{if(board[selectedR][selectedC]===0)filled++;board[selectedR][selectedC]=n;}
    hidePopup();renderBoard();updateStatus();checkComplete();
}

// Load popup
var loadPopup,loadList=[];
function createLoadPopup(){
    loadPopup=lvgljs.panel(W.w/2-200,W.h/2-180,400,360);
    lvgljs.setBgColor(loadPopup,0x333333);lvgljs.setOpacity(loadPopup,230);lvgljs.setRadius(loadPopup,12);lvgljs.setVisible(loadPopup,0);lvgljs.toFront(loadPopup);
    lvgljs.label("Select Save",140,10,loadPopup);lvgljs.setTextColor(loadPopup,0xFFFFFF);
    lvgljs.btn("Close",280,320,100,32,function(){lvgljs.setVisible(loadPopup,0);},loadPopup);
}
createLoadPopup();
function showLoadPopup(idx){
    // Clear old list items
    for(var i=0;i<loadList.length;i++)lvgljs.setVisible(loadList[i],0);
    loadList=[];
    var y=45;
    for(var i=0;i<Math.min(idx.length,8);i++){
        var s=idx[i];
        var txt=s.name+"  ["+diffLabels[s.difficulty]+"]  "+s.date+"  -"+s.cells;
        var item=lvgljs.btn(txt,20,y,360,32,function(name){return function(){
            lvgljs.setVisible(loadPopup,0);
            if(loadGame(name)){renderBoard();updateStatus();updateDiffBtns();}
        };}(s.name),loadPopup);
        lvgljs.setFont(item,14);lvgljs.setTextColor(item,0xCCCCCC);lvgljs.setBgColor(item,0x444455);lvgljs.setRadius(item,4);
        loadList.push(item);y+=38;
    }
    if(idx.length===0)lvgljs.label("(no saves)",150,100,loadPopup);
    lvgljs.setVisible(loadPopup,1);
}

// Bottom bar
var bottomBar=lvgljs.panel(0,W.h-72,W.w,72);
lvgljs.setBgColor(bottomBar,0x2a2a3e);lvgljs.setOpacity(bottomBar,255);lvgljs.setRadius(bottomBar,0);
var statusLbl=lvgljs.label("",20,6,bottomBar);lvgljs.setTextColor(statusLbl,0xCCCCCC);lvgljs.setFont(statusLbl,18);
// saveNameBox replaced by popup


function addBtn(text,x,cb){
    var b=lvgljs.btn(text,x,30,80,36,cb,bottomBar);lvgljs.setRadius(b,6);lvgljs.setFont(b,16);lvgljs.setBgColor(b,0x444455);return b;
}
addBtn("Save",W.w-480,function(){showSavePopup();});
addBtn("Load",W.w-390,function(){loadGame();});
addBtn("Del",W.w-300,function(){showDeletePopup();});
addBtn("Hint",W.w-210,function(){
    if(selectedR>=0&&selectedC>=0&&!fixed[selectedR][selectedC]){board[selectedR][selectedC]=solution[selectedR][selectedC];filled++;selectedR=selectedC=-1;hidePopup();renderBoard();updateStatus();checkComplete();}
});
addBtn("New",W.w-570,function(){generate(difficulty);renderBoard();updateStatus();timerSec=0;});

// Rendering
function hasConflict(r,c,n){
    if(n===0)return false;
    for(var i=0;i<9;i++){if(i!==c&&board[r][i]===n)return true;if(i!==r&&board[i][c]===n)return true;}
    var br=Math.floor(r/3)*3,bc=Math.floor(c/3)*3;
    for(var i=0;i<3;i++)for(var j=0;j<3;j++)if((br+i!==r||bc+j!==c)&&board[br+i][bc+j]===n)return true;
    return false;
}
function isSameNumber(r,c){return selectedR>=0&&board[r][c]!==0&&board[selectedR][selectedC]!==0&&board[r][c]===board[selectedR][selectedC];}
function renderBoard(){
    for(var r=0;r<9;r++)for(var c=0;c<9;c++){
        var val=board[r][c],region=Math.floor(r/3)*3+Math.floor(c/3),cell=cells[r][c];
        if(r===selectedR&&c===selectedC)lvgljs.setBgColor(cell,T.sel);
        else if(val>0&&hasConflict(r,c,val))lvgljs.setBgColor(cell,T.conflict);
        else if(isSameNumber(r,c))lvgljs.setBgColor(cell,T.same);
        else lvgljs.setBgColor(cell,region%2===0?T.light:T.dark);
        if(val>0){lvgljs.setText(cellLabels[r][c],""+val);lvgljs.setTextColor(cellLabels[r][c],fixed[r][c]?T.hint:T.user);}
        else lvgljs.setText(cellLabels[r][c],"");
    }
}
function updateDiffBtns(){for(var d=0;d<3;d++)lvgljs.setBgColor(diffBtns[d],d===difficulty?T.accent:0x555566);}
function updateStatus(){lvgljs.setText(statusLbl,"Filled: "+filled+"/81  "+diffLabels[difficulty]+(currentSave?"  ["+currentSave+"]":""));}
function checkComplete(){
    if(filled<81)return;
    for(var r=0;r<9;r++)for(var c=0;c<9;c++)if(board[r][c]!==solution[r][c])return;
    lvgljs.print("COMPLETE!");
    for(var r=0;r<9;r++)for(var c=0;c<9;c++)lvgljs.setBgColor(cells[r][c],0xCCFFCC);
}
function updateTimer(){timerSec++;var m=Math.floor(timerSec/60),s=timerSec%60;lvgljs.setText(timerLbl,(m<10?"0":"")+m+":"+(s<10?"0":"")+s);}

// Init
generate(difficulty);renderBoard();updateDiffBtns();updateStatus();
lvgljs.toFront(closeBtn);lvgljs.hideBackButton();
lvgljs.setInterval(1000,updateTimer);
lvgljs.print("Sudoku ready");
