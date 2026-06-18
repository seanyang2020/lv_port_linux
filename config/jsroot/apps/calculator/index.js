/**
 * Calculator — fullscreen scientific calculator
 * Adapts to portrait (800×1280) and landscape (1280×800).
 */
var SCR = lvgljs.getScreenSize();
var WW = SCR.w, WH = SCR.h;
var isLand = WW > WH;

lvgljs.screenColor(0x12151A);
lvgljs.hideBackButton();
lvgljs.setFPS(20);

var C_DISP = 0x0A0C10;
var C_NUM  = 0x2A3038;
var C_SCI  = 0x1C222A;
var C_OP   = 0x3D4550;
var C_EQ   = 0xE67E22;
var C_AC   = 0xC0392B;
var C_TEXT = 0xF0F2F5;
var C_MUTED = 0x8B939E;
var C_ACCENT = 0xF5A623;

var expr = "";
var justEval = false;
var angleDeg = true;

var M = isLand ? 14 : 16;
var gap = isLand ? 8 : 10;
var topBar = isLand ? 48 : 56;
var dispH = isLand ? Math.floor(WH * 0.22) : Math.floor(WH * 0.18);
if (dispH < 100) dispH = 100;
if (dispH > 200) dispH = 200;

var gridTop = topBar + dispH + M;
var gridH = WH - gridTop - M;
var gridW = WW - M * 2;
var COLS = 5, ROWS = 8;
var btnW = Math.floor((gridW - gap * (COLS - 1)) / COLS);
var btnH = Math.floor((gridH - gap * (ROWS - 1)) / ROWS);
var fontBtn = btnH >= 70 ? 22 : (btnH >= 54 ? 18 : 14);
var fontSci = btnH >= 70 ? 15 : (btnH >= 54 ? 13 : 11);

/* ---- chrome ---- */
var title = lvgljs.label("Scientific", M, isLand ? 12 : 14);
lvgljs.setFont(title, isLand ? 18 : 20);
lvgljs.setTextColor(title, C_MUTED);

var modeLbl = lvgljs.label("DEG", M + (isLand ? 130 : 150), isLand ? 14 : 16);
lvgljs.setFont(modeLbl, 16);
lvgljs.setTextColor(modeLbl, C_ACCENT);

var exitBtn = lvgljs.btn("X", WW - M - 44, 8, 44, 40, function () { lvgljs.exit(); });
lvgljs.setBgColor(exitBtn, C_OP);
lvgljs.setRadius(exitBtn, 10);
lvgljs.setFont(exitBtn, 18);
lvgljs.setTextColor(exitBtn, C_TEXT);

var dispPanel = lvgljs.panel(M, topBar, gridW, dispH);
lvgljs.setBgColor(dispPanel, C_DISP);
lvgljs.setRadius(dispPanel, 16);
lvgljs.setOpacity(dispPanel, 255);
lvgljs.setBorder(dispPanel, 1, 0x1E242C);

var exprLbl = lvgljs.label("0", M + 16, topBar + 12);
lvgljs.setFont(exprLbl, isLand ? 20 : 22);
lvgljs.setTextColor(exprLbl, C_MUTED);

var resultLbl = lvgljs.label("0", M + 16, topBar + Math.floor(dispH * 0.45));
lvgljs.setFont(resultLbl, isLand ? 36 : 42);
lvgljs.setTextColor(resultLbl, C_TEXT);

function refreshDisp() {
    var e = expr || "0";
    if (e.length > 36) e = "..." + e.slice(-34);
    lvgljs.setText(exprLbl, e);
}
function setResult(t) {
    var s = String(t);
    if (s.length > 18) {
        var n = Number(t);
        if (!isNaN(n) && isFinite(n)) s = n.toPrecision(12);
    }
    lvgljs.setText(resultLbl, s);
}

function toRad(x) { return angleDeg ? x * Math.PI / 180 : x; }
function fromRad(x) { return angleDeg ? x * 180 / Math.PI : x; }
function fact(n) {
    n = Math.floor(Number(n));
    if (n < 0 || n > 170 || isNaN(n)) throw new Error("fact");
    var r = 1;
    for (var i = 2; i <= n; i++) r *= i;
    return r;
}
function log10(x) { return Math.log(x) / Math.LN10; }

function prepare(s) {
    s = s.replace(/PI/g, "PI"); /* keep */
    s = s.replace(/(\d+(?:\.\d+)?|\))!/g, "fact($1)");
    /* a**b -> pow(a,b)  (QuickJS-safe, repeat for chains) */
    var guard = 0;
    while (s.indexOf("**") >= 0 && guard++ < 32) {
        s = s.replace(
            /((?:\([^()]*\)|[A-Za-z_]\w*|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?))\*\*((?:\([^()]*\)|[A-Za-z_]\w*|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?))/,
            "pow($1,$2)"
        );
    }
    s = s.replace(/(\d+(?:\.\d+)?)%/g, "($1/100)");
    return s;
}

function sanitize(s) {
    if (/[^0-9+\-*/().eE\s%!,a-zA-Z_]/.test(s)) return false;
    if (/\b(Function|eval|window|globalThis|import|require)\b/.test(s)) return false;
    return true;
}

function evaluate() {
    if (!expr) { setResult("0"); return; }
    try {
        var s = prepare(expr);
        if (!sanitize(s)) throw new Error("bad");
        var val = Function(
            "sin", "cos", "tan", "asin", "acos", "atan",
            "ln", "log", "sqrt", "pow", "fact", "abs", "PI", "E",
            "return (" + s + ");"
        )(
            function (x) { return Math.sin(toRad(x)); },
            function (x) { return Math.cos(toRad(x)); },
            function (x) { return Math.tan(toRad(x)); },
            function (x) { return fromRad(Math.asin(x)); },
            function (x) { return fromRad(Math.acos(x)); },
            function (x) { return fromRad(Math.atan(x)); },
            Math.log, log10, Math.sqrt, Math.pow, fact, Math.abs,
            Math.PI, Math.E
        );
        if (typeof val !== "number" || !isFinite(val)) throw new Error("nan");
        if (Math.abs(val) < 1e-12) val = 0;
        var out = (Math.abs(val - Math.round(val)) < 1e-10)
            ? String(Math.round(val))
            : String(parseFloat(val.toPrecision(12)));
        setResult(out);
        expr = out;
        justEval = true;
        refreshDisp();
    } catch (e) {
        setResult("Error");
        justEval = true;
        lvgljs.print("calc error: " + e);
    }
}

function needsMul() {
    if (!expr) return false;
    return /[0-9)PIE]$/.test(expr);
}

function append(tok) {
    if (justEval) {
        if (/^[0-9.]/.test(tok)) expr = "";
        else if (!/^[+\-*/]/.test(tok) && tok !== "**") expr = "";
        justEval = false;
    }
    if ((tok === "PI" || tok === "E") && needsMul()) expr += "*";
    if (tok === "(" && needsMul()) expr += "*";
    expr += tok;
    refreshDisp();
}

function clearAll() {
    expr = "";
    justEval = false;
    refreshDisp();
    setResult("0");
}
function backspace() {
    if (justEval) { clearAll(); return; }
    if (expr.slice(-2) === "PI") expr = expr.slice(0, -2);
    else expr = expr.slice(0, -1);
    refreshDisp();
}
function toggleSign() {
    if (!expr) return;
    if (justEval) justEval = false;
    if (/^-?\d+(\.\d+)?([eE][+-]?\d+)?$/.test(expr)) {
        expr = (expr.charAt(0) === "-") ? expr.slice(1) : ("-" + expr);
    } else {
        expr = "-(" + expr + ")";
    }
    refreshDisp();
}
function toggleAngle() {
    angleDeg = !angleDeg;
    lvgljs.setText(modeLbl, angleDeg ? "DEG" : "RAD");
    if (degBtn) lvgljs.setText(degBtn, angleDeg ? "DEG" : "RAD");
}
function wrapFn(name) {
    if (justEval) justEval = false;
    if (expr && /[0-9)PIE]$/.test(expr)) expr = name + "(" + expr + ")";
    else expr += name + "(";
    refreshDisp();
}
function applyUnary(kind) {
    if (!expr) {
        if (kind === "sqrt") { append("sqrt("); return; }
        return;
    }
    if (justEval) justEval = false;
    if (kind === "sqr") expr = "pow((" + expr + "),2)";
    else if (kind === "inv") expr = "1/(" + expr + ")";
    else if (kind === "sqrt") expr = "sqrt(" + expr + ")";
    else if (kind === "fact") expr = "(" + expr + ")!";
    refreshDisp();
}

var degBtn = 0;
var keys = [
    [
        { t: "sin",  k: "wrap", a: "sin", c: C_SCI },
        { t: "cos",  k: "wrap", a: "cos", c: C_SCI },
        { t: "tan",  k: "wrap", a: "tan", c: C_SCI },
        { t: "log",  k: "wrap", a: "log", c: C_SCI },
        { t: "ln",   k: "wrap", a: "ln",  c: C_SCI }
    ],
    [
        { t: "asin", k: "wrap", a: "asin", c: C_SCI },
        { t: "acos", k: "wrap", a: "acos", c: C_SCI },
        { t: "atan", k: "wrap", a: "atan", c: C_SCI },
        { t: "10^x", k: "fn",   a: "pow10", c: C_SCI },
        { t: "e^x",  k: "fn",   a: "exp",  c: C_SCI }
    ],
    [
        { t: "sqrt", k: "un",   a: "sqrt", c: C_SCI },
        { t: "x^2",  k: "un",   a: "sqr",  c: C_SCI },
        { t: "x^y",  k: "op",   a: "**",   c: C_SCI },
        { t: "1/x",  k: "un",   a: "inv",  c: C_SCI },
        { t: "n!",   k: "un",   a: "fact", c: C_SCI }
    ],
    [
        { t: "pi", k: "const", a: "PI", c: C_SCI },
        { t: "e",  k: "const", a: "E",  c: C_SCI },
        { t: "(",  k: "dig",   a: "(",  c: C_OP },
        { t: ")",  k: "dig",   a: ")",  c: C_OP },
        { t: "%",  k: "dig",   a: "%",  c: C_OP }
    ],
    [
        { t: "7",  k: "dig", a: "7", c: C_NUM },
        { t: "8",  k: "dig", a: "8", c: C_NUM },
        { t: "9",  k: "dig", a: "9", c: C_NUM },
        { t: "/",  k: "op",  a: "/", c: C_OP },
        { t: "AC", k: "ac",  a: "",  c: C_AC }
    ],
    [
        { t: "4",  k: "dig", a: "4", c: C_NUM },
        { t: "5",  k: "dig", a: "5", c: C_NUM },
        { t: "6",  k: "dig", a: "6", c: C_NUM },
        { t: "*",  k: "op",  a: "*", c: C_OP },
        { t: "BK", k: "bk",  a: "",  c: C_OP }
    ],
    [
        { t: "1",    k: "dig",  a: "1", c: C_NUM },
        { t: "2",    k: "dig",  a: "2", c: C_NUM },
        { t: "3",    k: "dig",  a: "3", c: C_NUM },
        { t: "-",    k: "op",   a: "-", c: C_OP },
        { t: "+/-",  k: "sign", a: "",  c: C_OP }
    ],
    [
        { t: "0",   k: "dig",   a: "0", c: C_NUM },
        { t: ".",   k: "dig",   a: ".", c: C_NUM },
        { t: "=",   k: "eq",    a: "",  c: C_EQ },
        { t: "+",   k: "op",    a: "+", c: C_OP },
        { t: "DEG", k: "angle", a: "",  c: C_SCI }
    ]
];

function onKey(spec) {
    var k = spec.k, a = spec.a;
    if (k === "dig" || k === "op" || k === "const") append(a);
    else if (k === "eq") evaluate();
    else if (k === "ac") clearAll();
    else if (k === "bk") backspace();
    else if (k === "sign") toggleSign();
    else if (k === "angle") toggleAngle();
    else if (k === "wrap") wrapFn(a);
    else if (k === "un") applyUnary(a);
    else if (k === "fn") {
        if (justEval) justEval = false;
        if (a === "pow10") expr = "pow(10," + (expr || "0") + ")";
        else if (a === "exp") expr = "pow(E," + (expr || "0") + ")";
        refreshDisp();
    }
}

for (var r = 0; r < ROWS; r++) {
    for (var c = 0; c < COLS; c++) {
        var spec = keys[r][c];
        var x = M + c * (btnW + gap);
        var y = gridTop + r * (btnH + gap);
        (function (sp) {
            var b = lvgljs.btn(sp.t, x, y, btnW, btnH, function () { onKey(sp); });
            lvgljs.setBgColor(b, sp.c);
            lvgljs.setRadius(b, Math.min(14, Math.floor(btnH / 5)));
            var f = (sp.t.length > 2 || sp.c === C_SCI) ? fontSci : fontBtn;
            lvgljs.setFont(b, f);
            lvgljs.setTextColor(b, C_TEXT);
            lvgljs.setBorder(b, 0);
            if (sp.k === "angle") degBtn = b;
        })(spec);
    }
}

lvgljs.toFront(exitBtn);
refreshDisp();
setResult("0");
lvgljs.print("Calculator ready " + WW + "x" + WH + (isLand ? " land" : " port"));
