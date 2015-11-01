
var main = function () {
    var v4l2camera = require("../");

    var cam = new v4l2camera.Camera("/dev/video0");
    if (cam.configGet().formatName !== "YUYV") {
        console.log("YUYV camera required");
        process.exit(1);
    }
    cam.configSet({width: 352, height: 288});
    cam.start();
    times(6, cam.capture.bind(cam), function () {
        var rgb = yuyv2rgb(cam.toYUYV(), cam.width, cam.height);
        saveAsPng(rgb, cam.width, cam.height, "result.png");
        cam.stop();
    });
    console.log("w: " + cam.width + " h: " + cam.height);
};


var times = function (n, async, cont) {
    return async(function rec(r) {return --n == 0 ? cont(r) : async(rec);});
};
var saveAsPng = function (rgb, width, height, filename) {
    var fs = require("fs");
    var pngjs = require("pngjs");

    var png = new pngjs.PNG({
        width: width, height: height, deflateLevel: 1, deflateStrategy: 1,
    });
    var size = width * height;
    for (var i = 0; i < size; i++) {
        png.data[i * 4 + 0] = rgb[i * 3 + 0];
        png.data[i * 4 + 1] = rgb[i * 3 + 1];
        png.data[i * 4 + 2] = rgb[i * 3 + 2];
        png.data[i * 4 + 3] = 255;
    }
    png.pack().pipe(fs.createWriteStream(filename));
};

// yuyv data handling
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
var yuyv2rgb = function (yuyv, width, height) {
    var rgb = new Array(width * height * 3);
    for (var i = 0; i < height; i++) {
        for (var j = 0; j < width; j += 2) {
            var index = i * width + j;
            var y0 = yuyv[index * 2 + 0] << 8;
            var u = yuyv[index * 2 + 1] - 128;
            var y1 = yuyv[index * 2 + 2] << 8;
            var v = yuyv[index * 2 + 3] - 128;
            rgb[index * 3 + 0] = yuv2r(y0, u, v);
            rgb[index * 3 + 1] = yuv2g(y0, u, v);
            rgb[index * 3 + 2] = yuv2b(y0, u, v);
            rgb[index * 3 + 3] = yuv2r(y1, u, v);
            rgb[index * 3 + 4] = yuv2g(y1, u, v);
            rgb[index * 3 + 5] = yuv2b(y1, u, v);
        }
    }
    return rgb;
};

main();
