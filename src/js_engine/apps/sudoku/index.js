// Sudoku — 1280×800 landscape, popup number picker
// Difficulty: easy(35) medium(28) hard(22) clues

var W = lvgljs.getScreenSize();
var DIR = __dirname;
var SAVE_ORIG = DIR + "/save/original.json";
var SAVE_PROG = DIR + "/save/progress.json";

// ============================================================
// Theme
// ============================================================
var T = {
    bg: 0xF0F0F0,         dark: 0xE0E4EC,       light: 0xF5F6FA,
    grid: 0xCCCCCC,        sel: 0xBBC8E8,         same: 0xD0D8F0,
    conflict: 0xFFCCCC,    hint: 0x111111,        user: 0x3366CC,
    accent: 0x3366CC
};

// ============================================================
// Sudoku engine
// ============================================================
var board = [];      // 9×9 current state (0=empty)
var solution = [];   // 9×9 solution
var fixed = [];      // 9×9 booleans (true = original clue, cannot edit)
var selectedR = -1, selectedC = -1;
var difficulty = 1;  // 0=easy 1=medium 2=hard
var cluesCount = [35, 28, 22];
var filled = 0;

function initBoard() {
    board = []; solution = []; fixed = [];
    for (var r = 0; r < 9; r++) {
        board[r] = []; solution[r] = []; fixed[r] = [];
        for (var c = 0; c < 9; c++) { board[r][c] = 0; solution[r][c] = 0; fixed[r][c] = false; }
    }
}

function copyBoard(src, dst) {
    for (var r = 0; r < 9; r++)
        for (var c = 0; c < 9; c++)
            dst[r][c] = src[r][c];
}

// Backtracking solver
function solve(grid) {
    for (var r = 0; r < 9; r++) {
        for (var c = 0; c < 9; c++) {
            if (grid[r][c] === 0) {
                var nums = [1,2,3,4,5,6,7,8,9];
                shuffle(nums);
                for (var i = 0; i < 9; i++) {
                    if (isValid(grid, r, c, nums[i])) {
                        grid[r][c] = nums[i];
                        if (solve(grid)) return true;
                        grid[r][c] = 0;
                    }
                }
                return false;
            }
        }
    }
    return true; // all filled
}

function isValid(grid, r, c, n) {
    // row
    for (var i = 0; i < 9; i++) if (grid[r][i] === n) return false;
    // col
    for (var i = 0; i < 9; i++) if (grid[i][c] === n) return false;
    // 3x3 box
    var br = Math.floor(r/3)*3, bc = Math.floor(c/3)*3;
    for (var i = 0; i < 3; i++)
        for (var j = 0; j < 3; j++)
            if (grid[br+i][bc+j] === n) return false;
    return true;
}

function shuffle(a) {
    for (var i = a.length-1; i > 0; i--) {
        var j = Math.floor(Math.random()*(i+1));
        var t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

function generate(diff) {
    initBoard();
    // Fill diagonal 3x3 boxes (independent)
    for (var b = 0; b < 9; b += 3) {
        var nums = [1,2,3,4,5,6,7,8,9];
        shuffle(nums);
        for (var i = 0; i < 3; i++)
            for (var j = 0; j < 3; j++)
                solution[b+i][b+j] = nums[i*3+j];
    }
    solve(solution);
    copyBoard(solution, board);

    // Remove cells based on difficulty
    var toRemove = 81 - cluesCount[diff];
    var cells = [];
    for (var r = 0; r < 9; r++)
        for (var c = 0; c < 9; c++)
            cells.push([r,c]);
    shuffle(cells);
    for (var i = 0; i < toRemove; i++) {
        var rr = cells[i][0], cc = cells[i][1];
        board[rr][cc] = 0;
    }
    // Mark fixed cells
    for (var r = 0; r < 9; r++)
        for (var c = 0; c < 9; c++)
            fixed[r][c] = (board[r][c] !== 0);
    filled = 81 - toRemove;
    selectedR = selectedC = -1;
}

// ============================================================
// Save / Load
// ============================================================
function saveGame() {
    var orig = { board: [], solution: solution, fixed: fixed, difficulty: difficulty };
    for (var r = 0; r < 9; r++) orig.board[r] = board[r].slice();
    lvgljs.writeFile(SAVE_ORIG, JSON.stringify(orig));

    var prog = { board: [], filled: filled, difficulty: difficulty };
    for (var r = 0; r < 9; r++) prog.board[r] = board[r].slice();
    lvgljs.writeFile(SAVE_PROG, JSON.stringify(prog));
    lvgljs.print("Game saved");
}

function loadGame() {
    var data = lvgljs.readFile(SAVE_PROG);
    if (!data || data.length < 10) { lvgljs.print("No save found"); return false; }
    try {
        var prog = JSON.parse(data);
        initBoard();
        for (var r = 0; r < 9; r++)
            for (var c = 0; c < 9; c++)
                board[r][c] = prog.board[r][c];
        difficulty = prog.difficulty || 1;
        filled = prog.filled || 0;

        // Reconstruct fixed from original save
        var origData = lvgljs.readFile(SAVE_ORIG);
        if (origData && origData.length > 10) {
            var orig = JSON.parse(origData);
            for (var r = 0; r < 9; r++)
                for (var c = 0; c < 9; c++)
                    fixed[r][c] = (orig.board[r][c] !== 0);
            solution = orig.solution;
        }
        lvgljs.print("Game loaded");
        return true;
    } catch(e) { lvgljs.print("Load failed: " + e); return false; }
}

// ============================================================
// UI
// ============================================================
lvgljs.screenColor(T.bg);

// ---- Top bar ----
var topBar = lvgljs.panel(0, 0, W.w, 56);
lvgljs.setBgColor(topBar, 0x2a2a3e); lvgljs.setOpacity(topBar, 255); lvgljs.setRadius(topBar, 0);

var title = lvgljs.label("Sudoku", 20, 12, topBar);
lvgljs.setTextColor(title, 0xFFFFFF); lvgljs.setFont(title, 24);

// Difficulty buttons
var diffBtns = [], diffLabels = ["Easy", "Med.", "Hard"];
for (var d = 0; d < 3; d++) {
    var dx = 200 + d * 100;
    var db = lvgljs.btn(diffLabels[d], dx, 8, 90, 40, function(idx) {
        return function() {
            difficulty = idx; updateDiffBtns();
            generate(difficulty); renderBoard(); updateStatus();
        };
    }(d), topBar);
    lvgljs.setRadius(db, 6);
    lvgljs.setFont(db, 16);
    diffBtns.push(db);
}

// Timer
var timerLbl = lvgljs.label("00:00", W.w - 280, 12, topBar);
lvgljs.setTextColor(timerLbl, 0xFFFFFF); lvgljs.setFont(timerLbl, 22);
var timerSec = 0;

// Close button
var closeBtn = lvgljs.btn("X", W.w - 56, 6, 44, 44, function(){ lvgljs.exit(); });
lvgljs.setRadius(closeBtn, 22); lvgljs.setBgColor(closeBtn, 0x333333);
lvgljs.setOpacity(closeBtn, 120); lvgljs.setTextColor(closeBtn, 0xFFFFFF);

// ---- Board ----
var boardX = 40, boardY = 70, cellSize = 64;
var boardBg = lvgljs.panel(boardX-4, boardY-4, cellSize*9+8, cellSize*9+8);
lvgljs.setBgColor(boardBg, T.dark); lvgljs.setOpacity(boardBg, 255); lvgljs.setRadius(boardBg, 4);

var cells = [];  // 9×9 label widgets
for (var r = 0; r < 9; r++) {
    cells[r] = [];
    for (var c = 0; c < 9; c++) {
        var x = boardX + c * cellSize, y = boardY + r * cellSize;
        var cell = lvgljs.panel(x, y, cellSize-1, cellSize-1);
        // Region coloring
        var region = Math.floor(r/3)*3 + Math.floor(c/3);
        lvgljs.setBgColor(cell, region % 2 === 0 ? T.light : T.dark);
        lvgljs.setOpacity(cell, 255); lvgljs.setRadius(cell, 0);

        // Cell click → select
        lvgljs.onClick(cell, function(rr, cc) {
            return function() {
                if (selectedR === rr && selectedC === cc) { selectedR = selectedC = -1; hidePopup(); }
                else { selectedR = rr; selectedC = cc; showPopup(rr, cc); }
                renderBoard();
            };
        }(r, c));
        cells[r][c] = cell;
    }
}

// Region borders (thicker lines for 3x3 boxes)
for (var i = 1; i < 3; i++) {
    var hy = boardY + i * cellSize * 3;
    var hLine = lvgljs.panel(boardX, hy-1, cellSize*9, 3);
    lvgljs.setBgColor(hLine, T.accent); lvgljs.setOpacity(hLine, 255); lvgljs.setRadius(hLine, 0);
    var vx = boardX + i * cellSize * 3;
    var vLine = lvgljs.panel(vx-1, boardY, 3, cellSize*9);
    lvgljs.setBgColor(vLine, T.accent); lvgljs.setOpacity(vLine, 255); lvgljs.setRadius(vLine, 0);
}

// ---- Popup number picker ----
var popupVisible = false;
var popupPanel, popupBtns = [];

function createPopup() {
    popupPanel = lvgljs.panel(0, 0, 156, 210);
    lvgljs.setBgColor(popupPanel, 0x333333); lvgljs.setOpacity(popupPanel, 210);
    lvgljs.setRadius(popupPanel, 10);
    lvgljs.setVisible(popupPanel, 0);
    lvgljs.toFront(popupPanel);

    for (var i = 0; i < 9; i++) {
        var px = 8 + (i % 3) * 50, py = 8 + Math.floor(i/3) * 50;
        var btn = lvgljs.btn("" + (i+1), px, py, 44, 44, function(n) {
            return function() { placeNumber(n); };
        }(i+1), popupPanel);
        lvgljs.setBgColor(btn, 0x555555); lvgljs.setRadius(btn, 8);
        lvgljs.setTextColor(btn, 0xFFFFFF); lvgljs.setFont(btn, 20);
        popupBtns.push(btn);
    }
    // Erase button
    var eraseBtn = lvgljs.btn("Erase", 8, 160, 138, 40, function() { placeNumber(0); }, popupPanel);
    lvgljs.setBgColor(eraseBtn, 0x663333); lvgljs.setRadius(eraseBtn, 8);
    lvgljs.setTextColor(eraseBtn, 0xFFFFFF);
}
createPopup();

function showPopup(r, c) {
    var px = boardX + c * cellSize - 45;
    var py = boardY + r * cellSize + cellSize + 4;
    // Avoid edge overflow
    if (px < 10) px = 10;
    if (px > W.w - 170) px = W.w - 170;
    if (py > W.h - 230) py = boardY + r * cellSize - 220;
    lvgljs.setPos(popupPanel, px, py);
    lvgljs.setVisible(popupPanel, 1);
    popupVisible = true;
}

function hidePopup() {
    lvgljs.setVisible(popupPanel, 0);
    popupVisible = false;
}

function placeNumber(n) {
    if (selectedR < 0 || selectedC < 0) return;
    if (fixed[selectedR][selectedC]) { hidePopup(); return; }
    if (n === 0) {
        if (board[selectedR][selectedC] !== 0) filled--;
        board[selectedR][selectedC] = 0;
    } else {
        if (board[selectedR][selectedC] === 0) filled++;
        board[selectedR][selectedC] = n;
    }
    hidePopup();
    renderBoard();
    updateStatus();
    checkComplete();
}

// ---- Bottom bar ----
var bottomBar = lvgljs.panel(0, W.h - 56, W.w, 56);
lvgljs.setBgColor(bottomBar, 0x2a2a3e); lvgljs.setOpacity(bottomBar, 255); lvgljs.setRadius(bottomBar, 0);

var statusLbl = lvgljs.label("Filled: 0/81", 20, 14, bottomBar);
lvgljs.setTextColor(statusLbl, 0xCCCCCC); lvgljs.setFont(statusLbl, 18);

function addBtn(text, x, cb) {
    var b = lvgljs.btn(text, x, 8, 90, 40, cb, bottomBar);
    lvgljs.setRadius(b, 6); lvgljs.setFont(b, 16);
    lvgljs.setBgColor(b, 0x444455);
    return b;
}
addBtn("New",    W.w - 640, function(){ generate(difficulty); renderBoard(); updateStatus(); timerSec=0; });
addBtn("Save",   W.w - 540, saveGame);
addBtn("Load",   W.w - 440, function(){ if(loadGame()){ renderBoard(); updateStatus(); } });
addBtn("Delete", W.w - 340, function(){ lvgljs.deleteFile(SAVE_ORIG); lvgljs.deleteFile(SAVE_PROG); lvgljs.print("Save deleted"); });
addBtn("Hint", W.w - 240, function(){
    if (selectedR >= 0 && selectedC >= 0 && !fixed[selectedR][selectedC]) {
        board[selectedR][selectedC] = solution[selectedR][selectedC];
        if (board[selectedR][selectedC] !== 0) filled++;
        selectedR = selectedC = -1; hidePopup(); renderBoard(); updateStatus(); checkComplete();
    }
});

// ============================================================
// Rendering
// ============================================================
function hasConflict(r, c, n) {
    if (n === 0) return false;
    for (var i = 0; i < 9; i++) {
        if (i !== c && board[r][i] === n) return true;
        if (i !== r && board[i][c] === n) return true;
    }
    var br = Math.floor(r/3)*3, bc = Math.floor(c/3)*3;
    for (var i = 0; i < 3; i++)
        for (var j = 0; j < 3; j++)
            if ((br+i !== r || bc+j !== c) && board[br+i][bc+j] === n) return true;
    return false;
}

function isSameNumber(r, c) {
    if (selectedR < 0 || board[r][c] === 0 || board[selectedR][selectedC] === 0) return false;
    return board[r][c] === board[selectedR][selectedC];
}

function renderBoard() {
    for (var r = 0; r < 9; r++) {
        for (var c = 0; c < 9; c++) {
            var cell = cells[r][c];
            var region = Math.floor(r/3)*3 + Math.floor(c/3);
            var val = board[r][c];

            // Background priority: select > conflict > same > region
            if (r === selectedR && c === selectedC) {
                lvgljs.setBgColor(cell, T.sel);
            } else if (val > 0 && hasConflict(r, c, val)) {
                lvgljs.setBgColor(cell, T.conflict);
            } else if (isSameNumber(r, c)) {
                lvgljs.setBgColor(cell, T.same);
            } else {
                lvgljs.setBgColor(cell, region % 2 === 0 ? T.light : T.dark);
            }

            // Clear old label by recreating? No — setText on existing.
            // But cells are panels, need inner labels. Let's create labels on first render.
        }
    }
}

// Actually cells need inner labels. Redo cell creation to include labels.
function createCellContent(r, c) {
    var cell = cells[r][c];
    // Remove old label if any (we manage via setText on a stored label)
    // For now: recreate label each render
    var val = board[r][c];
    var region = Math.floor(r/3)*3 + Math.floor(c/3);

    // Background
    if (r === selectedR && c === selectedC) lvgljs.setBgColor(cell, T.sel);
    else if (val > 0 && hasConflict(r,c,val)) lvgljs.setBgColor(cell, T.conflict);
    else if (isSameNumber(r,c)) lvgljs.setBgColor(cell, T.same);
    else lvgljs.setBgColor(cell, region%2===0 ? T.light : T.dark);
}

// Re-implement: store labels alongside cells
var cellLabels = [];
(function initCells() {
    for (var r = 0; r < 9; r++) {
        cellLabels[r] = [];
        for (var c = 0; c < 9; c++) {
            var cell = cells[r][c];
            var lbl = lvgljs.label("", 0, 0, cell);
            lvgljs.setPos(lbl, cellSize/2 - 8, cellSize/2 - 12);
            lvgljs.setFont(lbl, 28);
            cellLabels[r][c] = lbl;
        }
    }
})();

function renderBoard() {
    for (var r = 0; r < 9; r++) {
        for (var c = 0; c < 9; c++) {
            var val = board[r][c];
            var region = Math.floor(r/3)*3 + Math.floor(c/3);
            var cell = cells[r][c];

            if (r === selectedR && c === selectedC) lvgljs.setBgColor(cell, T.sel);
            else if (val > 0 && hasConflict(r,c,val)) lvgljs.setBgColor(cell, T.conflict);
            else if (isSameNumber(r,c)) lvgljs.setBgColor(cell, T.same);
            else lvgljs.setBgColor(cell, region%2===0 ? T.light : T.dark);

            if (val > 0) {
                lvgljs.setText(cellLabels[r][c], "" + val);
                lvgljs.setTextColor(cellLabels[r][c], fixed[r][c] ? T.hint : T.user);
            } else {
                lvgljs.setText(cellLabels[r][c], "");
            }
        }
    }
}

function updateDiffBtns() {
    for (var d = 0; d < 3; d++)
        lvgljs.setBgColor(diffBtns[d], d === difficulty ? T.accent : 0x555566);
}

function updateStatus() {
    lvgljs.setText(statusLbl, "Filled: " + filled + "/81  Difficulty: " + diffLabels[difficulty]);
}

function checkComplete() {
    if (filled < 81) return;
    for (var r = 0; r < 9; r++)
        for (var c = 0; c < 9; c++)
            if (board[r][c] !== solution[r][c]) return;
    lvgljs.print("PUZZLE COMPLETE!");
    // Flash effect: set all green briefly
    for (var r = 0; r < 9; r++)
        for (var c = 0; c < 9; c++)
            lvgljs.setBgColor(cells[r][c], 0xCCFFCC);
    lvgljs.setInterval(2000, function() { renderBoard(); });
}

function updateTimer() {
    timerSec++;
    var m = Math.floor(timerSec/60), s = timerSec%60;
    lvgljs.setText(timerLbl, (m<10?"0":"")+m+":"+(s<10?"0":"")+s);
}

// ============================================================
// Init
// ============================================================
generate(difficulty);
renderBoard();
updateDiffBtns();
updateStatus();
lvgljs.toFront(closeBtn);
lvgljs.hideBackButton();
lvgljs.setInterval(1000, updateTimer);
lvgljs.print("Sudoku ready");
