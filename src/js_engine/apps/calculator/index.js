// Working calculator — full-screen, immersive
lvgljs.screenColor(0x16213e);

var display = lvgljs.textbox(10, 10, 290, 50);
var expr = "";

function updateDisplay() {
    lvgljs.setText(display, expr || "0");
}

function clear()    { expr = ""; updateDisplay(); }
function backspace(){ expr = expr.slice(0, -1); updateDisplay(); }
function calculate() {
    try { expr = String(eval(expr)); } catch(e) { expr = "Error"; }
    updateDisplay();
}

var rows = [
    ["C",  "CE", "(",  ")" ],
    ["7",  "8",  "9",  "/" ],
    ["4",  "5",  "6",  "*" ],
    ["1",  "2",  "3",  "-" ],
    ["0",  ".",  "=",  "+" ]
];

var btnW = 65, btnH = 48, startX = 10, startY = 75, gapX = 72, gapY = 54;

for (var r = 0; r < rows.length; r++) {
    for (var c = 0; c < rows[r].length; c++) {
        var label = rows[r][c];
        var x = startX + c * gapX;
        var y = startY + r * gapY;
        (function(lbl) {
            if (lbl === "C")       lvgljs.btn(lbl, x, y, btnW, btnH, clear);
            else if (lbl === "CE") lvgljs.btn(lbl, x, y, btnW, btnH, backspace);
            else if (lbl === "=")  lvgljs.btn(lbl, x, y, btnW, btnH, calculate);
            else lvgljs.btn(lbl, x, y, btnW, btnH, function() {
                expr += lbl; updateDisplay();
            });
        })(label);
    }
}
updateDisplay();
