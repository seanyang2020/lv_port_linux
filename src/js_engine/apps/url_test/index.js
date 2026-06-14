// URL API test — removed broken URL/URLSearchParams (not available in embedded txiki.js)
// These Web APIs require JS polyfills not loaded in our embedded runtime.
// Use string operations for URL-like tasks instead.

lvgljs.screenColor(0x1a1a2e);
lvgljs.label("URL API Test", 20, 20);
lvgljs.label("URL/URLSearchParams not available", 20, 60);
lvgljs.label("in embedded txiki.js runtime.", 20, 85);
lvgljs.label("Use string operations instead:", 20, 120);
lvgljs.label("  var host = url.split('/')[2];", 20, 155);
lvgljs.label("  var params = qs.split('&');", 20, 180);
lvgljs.label("File I/O is available:", 20, 220);
lvgljs.label("  readFile / writeFile / deleteFile", 20, 245);
