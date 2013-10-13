var http = require("http");
var png = require("png");
var v4l2camera = require("../");

var server = http.createServer(function (req, res) {
    //console.log(req.url);
    if (req.url === "/") {
        res.writeHead(200, {
            "content-type": "text/html;charset=utf-8",
        });
        res.end([
            "<!doctype html>",
            "<html><head><meta charset='utf-8'/>",
            "<script>(", script.toString(), ")()</script>",
            "</head><body>",
            "<img id='cam' width='352' height='288' />",
            "</body></html>",
        ].join(""));
        return;
    }
    if (req.url.match(/^\/.+\.png$/)) {
        res.writeHead(200, {
            "content-type": "image/png",
            "cache-control": "no-cache",
        });
        var rgb = Buffer(cam.toRGB());
        new png.Png(rgb, cam.width, cam.height, "rgb").encode(function (buf) {
            res.end(buf);
        });
        return;
    }
});
server.listen(3000);

var script = function () {
    window.addEventListener("load", function (ev) {
        var cam = document.getElementById("cam");
        (function load() {
            var img = new Image();
            img.addEventListener("load", function loaded(ev) {
                cam.parentNode.replaceChild(img, cam);
                img.id = "cam";
                cam = img;
                load();
            }, false);
            img.src = "/" + Date.now() + ".png";
        })();
    }, false);
};

var cam = new v4l2camera.Camera("/dev/video0")
cam.configSet({width: 352, height: 288});
cam.start();
cam.capture(function loop() {
    cam.capture(loop);
});
