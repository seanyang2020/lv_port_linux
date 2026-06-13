// URL API test — exercises URL parsing, getters, setters, search params
lvgljs.screenColor(0x1a1a2e);

var y = 8, pass = 0, fail = 0;
function result(label, ok) {
    var color = ok ? 0x00FF00 : 0xFF4444;
    var sym  = ok ? "[OK]" : "[FAIL]";
    lvgljs.label(sym + " " + label, 10, y);
    if (!ok) fail++; else pass++;
    y += 22;
}
function section(title) {
    lvgljs.label("-- " + title + " --", 10, y);
    y += 26;
}

// ---- 1. basic parsing ----
section("1. Basic URL parsing");
var u = new URL("https://example.com/path/to/page?q=hello&lang=en#footer");
result("href",     u.href     === "https://example.com/path/to/page?q=hello&lang=en#footer");
result("protocol", u.protocol === "https:");
result("hostname", u.hostname === "example.com");
result("host",     u.host     === "example.com");
result("pathname", u.pathname === "/path/to/page");
result("search",   u.search   === "?q=hello&lang=en");
result("hash",     u.hash     === "#footer");
result("port",     u.port     === "");

// ---- 2. port & auth ----
section("2. Port and authentication");
var u2 = new URL("http://user:pass@example.com:8080/path");
result("username", u2.username === "user");
result("password", u2.password === "pass");
result("host",     u2.host     === "example.com:8080");
result("hostname", u2.hostname === "example.com");
result("port",     u2.port     === "8080");
result("pathname", u2.pathname === "/path");

// ---- 3. search params ----
section("3. URLSearchParams");
var sp = new URLSearchParams("key1=val1&key2=val2&key1=val3");
result("size",       sp.size === 3);
result("has key1",   sp.has("key1") === true);
result("has key3",   sp.has("key3") === false);
result("get key1",   sp.get("key1") === "val1");
result("get key2",   sp.get("key2") === "val2");
result("getAll k1",  JSON.stringify(sp.getAll("key1")) === JSON.stringify(["val1","val3"]));

sp.append("newkey", "newval");
result("append/size", sp.size === 4);
result("append/has",  sp.has("newkey") === true);

sp.set("key2", "updated");
result("set/get",     sp.get("key2") === "updated");

sp.remove("key1");
result("remove/size", sp.size === 2);
result("remove/has",  sp.has("key1") === false);

sp.sort();
result("sort/ok",     true); // just verify no crash

sp.reset();
result("reset/size",  sp.size === 0);

// ---- 4. search params from URL ----
section("4. URL.searchParams");
var u3 = new URL("https://x.com/search?a=1&b=2&a=3");
result("sp from url", u3.searchParams !== undefined);
if (u3.searchParams) {
    result("sp size",    u3.searchParams.size === 3);
    result("sp get a",   u3.searchParams.get("a") === "1");
    result("sp getAll a", JSON.stringify(u3.searchParams.getAll("a")) === JSON.stringify(["1","3"]));
}

// ---- 5. setters ----
section("5. Setters");
var u4 = new URL("https://example.com/oldpath");
u4.pathname = "/newpath";
result("set pathname", u4.pathname === "/newpath");
u4.hash = "#section1";
result("set hash",     u4.hash === "#section1");
u4.hostname = "newhost.com";
result("set hostname", u4.hostname === "newhost.com");
u4.protocol = "http:";
result("set protocol", u4.protocol === "http:");
u4.port = "3000";
result("set port",     u4.port === "3000");

// ---- 6. relative URL with base ----
section("6. Constructor with base URL");
var base = "https://example.com/dir/page.html";
var u5 = new URL("other.html", base);
result("rel href",     u5.href === "https://example.com/dir/other.html");
var u6 = new URL("/abs/path", base);
result("abs path",     u6.pathname === "/abs/path");
result("abs href",     u6.href === "https://example.com/abs/path");

// ---- 7. origin ----
section("7. Origin");
var u7 = new URL("https://example.com:443/path");
result("origin",       u7.origin === "https://example.com");

// ---- 8. edge cases ----
section("8. Edge cases");
var u8 = new URL("HTTP://EXAMPLE.COM/Path?Q=1#H");
result("lowercase",    u8.hostname === "example.com");
result("preserve path", u8.pathname === "/Path");
result("preserve query", u8.search === "?Q=1");

var u9 = new URL("ftp://ftp.example.com/files/");
result("ftp scheme",   u9.protocol === "ftp:");
result("ftp host",     u9.hostname === "ftp.example.com");

// ---- summary ----
section("RESULTS: " + pass + "/" + (pass+fail) + " passed");
if (fail === 0) {
    lvgljs.label("ALL TESTS PASSED!", 10, y);
} else {
    lvgljs.label(fail + " TESTS FAILED!", 10, y);
}
y += 30;
