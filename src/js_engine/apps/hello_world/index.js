// Hello World — full-screen interactive demo
lvgljs.screenColor(0x1a1a2e);

lvgljs.label("Hello World!", 30, 20);
lvgljs.label("JavaScript + LVGL + QuickJS", 30, 50);

var clicks = 0;
var counter = lvgljs.textbox(30, 85, 200, 45);
lvgljs.setText(counter, "Clicks: 0");

lvgljs.btn("Click Me!", 30, 150, 140, 48, function() {
    clicks++;
    lvgljs.setText(counter, "Clicks: " + clicks);
});

lvgljs.btn("Reset", 190, 150, 100, 48, function() {
    clicks = 0;
    lvgljs.setText(counter, "Clicks: 0");
});

lvgljs.label("Top-right X to return,", 30, 230);
lvgljs.label("or click:", 30, 255);
lvgljs.btn("Exit App", 30, 280, 120, 44, function() {
    lvgljs.exit();
});
