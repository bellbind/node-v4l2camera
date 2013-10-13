
var v4l2camera = require("../");

var fs = require("fs");
var png = require("png");

var times = function (n, async, cont) {
    for (var i = 0; i < n; i++) {
        cont = (function (c) {
            return function () {async(c);};
        })(cont);
    }
    return cont();
};

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

var cam = new v4l2camera.Camera("/dev/video0")
cam.configSet({width: 352, height: 288});
cam.start();
times(6, cam.capture.bind(cam), function () {
    var yuyv = cam.toYUYV();
    var rgb = yuyv2rgb(yuyv, cam.width, cam.height);;
    var img = new png.Png(Buffer(rgb), cam.width, cam.height, "rgb");
    img.encode(function (buf) {
        fs.writeFileSync("result.png", buf);
    });
    cam.stop();
});
console.log("w: " + cam.width + " h: " + cam.height);
