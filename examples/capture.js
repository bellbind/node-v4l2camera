
var v4l2camera = require("../");

var fs = require("fs");
var pngjs = require("pngjs");

var times = function (n, async, cont) {
    for (var i = 0; i < n; i++) {
        cont = (function (c) {
            return function () {async(c);};
        })(cont);
    }
    return cont();
};

var cam = new v4l2camera.Camera("/dev/video0");
cam.configSet({width: 352, height: 288});
cam.start();
times(6, cam.capture.bind(cam), function () {
    var rgb = cam.toRGB();
    var png = new pngjs.PNG({
        width: cam.width, height: cam.height,
        deflateLevel: 1, deflateStrategy: 1,
    });
    var size = cam.width * cam.height;
    for (var i = 0; i < size; i++) {
        png.data[i * 4 + 0] = rgb[i * 3 + 0];
        png.data[i * 4 + 1] = rgb[i * 3 + 1];
        png.data[i * 4 + 2] = rgb[i * 3 + 2];
        png.data[i * 4 + 3] = 255;
    }
    png.pack().pipe(fs.createWriteStream("result.png"));
    cam.stop();
});
console.log("w: " + cam.width + " h: " + cam.height);
