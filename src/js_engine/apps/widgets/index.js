// Widgets showcase — full-screen
lvgljs.screenColor(0x0f3460);

lvgljs.label("Widgets Showcase", 20, 15);

lvgljs.btn("Primary", 15, 55, 110, 44, function() {
    lvgljs.print("Primary clicked!");
});
lvgljs.btn("Success", 140, 55, 110, 44, function() {
    lvgljs.screenColor(0x0f4f0f);
});
lvgljs.btn("Warning", 15, 110, 110, 44, function() {
    lvgljs.screenColor(0x4f4f0f);
});
lvgljs.btn("Danger", 140, 110, 110, 44, function() {
    lvgljs.screenColor(0x4f0f0f);
});

lvgljs.label("Full-screen immersive mode", 20, 180);
lvgljs.label("No C title bar!", 20, 210);
lvgljs.label("Click X (top-right) to exit", 20, 240);
