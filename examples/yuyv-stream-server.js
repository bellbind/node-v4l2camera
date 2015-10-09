var http = require("http");
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
            "<script>//<!--",
	    "(" + script.toString() + ")()",
	    "//--></script>",
            "</head><body>",
            "<canvas id='cam' width='352' height='288' />",
            "</body></html>",
        ].join("\n"));
        return;
    }
    if (req.url.match(/^\/.+\.yuv$/)) {
	var buf = Buffer(cam.toYUYV());
	//var buf = Buffer(cam.toRGB());
        res.writeHead(200, {
            "content-type": "image/vnd-raw",
	    "content-length": buf.length,
        });
	res.end(buf);
    }
});
server.listen(3000);

var script = function () {
    window.addEventListener("load", function (ev) {
	var minmax = function (min, v, max) {
	    return (v < min) ? min : (max < v) ? max : v;
	};
	var yuv2r = function (y, u, v) {
	    return minmax(0, (y + 359 * v) >> 8, 255);
	};
	var yuv2g = function (y, u, v) {
	    return minmax(0, (y + 88 * v - 183 * u) >> 8, 255);
	};
	var yuv2b = function (y, u, v) {
	    return minmax(0, (y + 454 * u) >> 8, 255);
	};
	var yuyv2rgba = function (yuyv, rgba, width, height) {
	    for (var i = 0; i < height; i++) {
		for (var j = 0; j < width; j += 2) {
		    var index = i * width + j;
		    var y0 = yuyv[index * 2 + 0] << 8;
		    var u = yuyv[index * 2 + 1] - 128;
		    var y1 = yuyv[index * 2 + 2] << 8;
		    var v = yuyv[index * 2 + 3] - 128;
		    rgba[index * 4 + 0] = yuv2r(y0, u, v);
		    rgba[index * 4 + 1] = yuv2g(y0, u, v);
		    rgba[index * 4 + 2] = yuv2b(y0, u, v);
		    rgba[index * 4 + 3] = 255;
		    rgba[index * 4 + 4] = yuv2r(y1, u, v);
		    rgba[index * 4 + 5] = yuv2g(y1, u, v);
		    rgba[index * 4 + 6] = yuv2b(y1, u, v);
		    rgba[index * 4 + 7] = 255;
		}
	    }
	    return rgba;
	};
	var rgb2rgba = function (rgb, rgba, width, height) {
	    for (var i = 0; i < height; i++) {
		for (var j = 0; j < width; j++) {
		    var index = i * width + j;
		    rgba[index * 4 + 0] = rgb[index * 3 + 0];
		    rgba[index * 4 + 1] = rgb[index * 3 + 1];
		    rgba[index * 4 + 2] = rgb[index * 3 + 2];
		    rgba[index * 4 + 3] = 255;
		}
	    }
	    return rgba;
	};
	
        var cam = document.getElementById("cam");
	var c2d = cam.getContext("2d");
	var image = c2d.createImageData(cam.width, cam.height);
        (function load() {
	    var req = new XMLHttpRequest();
	    req.responseType = "arraybuffer";
	    req.addEventListener("load", function (ev) {
		var yuyv = new Uint8Array(req.response);
		yuyv2rgba(yuyv, image.data, image.width, image.height);
		//var rgb = new Uint8Array(req.response);
		//rgb2rgba(rgb, image.data, image.width, image.height);
		c2d.putImageData(image, 0, 0);
		setTimeout(load, 100);
	    }, false);
	    req.open("GET", "/" + Date.now() + ".yuv", true);
	    req.send();
        })();
    }, false);
};

var cam = new v4l2camera.Camera("/dev/video0")
if (cam.configGet().formatName !== "YUYV") {
    console.log("YUYV camera required");
    process.exit(1);
}
cam.configSet({width: 352, height: 288});
cam.start();
cam.capture(function loop() {
    cam.capture(loop);
});
