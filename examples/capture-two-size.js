
var main = function () {
    var v4l2camera = require("../");

    var cam = new v4l2camera.Camera("/dev/video0");
    if (cam.configGet().formatName !== "YUYV") {
        console.log("YUYV camera required");
        process.exit(1);
    }
    console.log("config");
    cam.configSet({width: 352, height: 288});
    console.log("start");
    cam.start();
    times(6, cam.capture.bind(cam), function () {
        saveAsPng(cam.toRGB(), cam.width, cam.height, "result1.png");
        console.log("stop");
        cam.stop(function () {
            console.log("reconfig");
            cam.configSet({width: 160, height: 120});
            console.log("restart");
            cam.start();
            times(6, cam.capture.bind(cam), function () {
                saveAsPng(cam.toRGB(), cam.width, cam.height, "result2.png");
                console.log("restop");
                cam.stop(function () {
                    cam = null;
                });
            });
        });
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

main();
