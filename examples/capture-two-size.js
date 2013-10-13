
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

var writePng = function (cam, filename) {
    var rgb = cam.toRGB();
    var img = new png.Png(Buffer(rgb), cam.width, cam.height, "rgb");
    img.encode(function (buf) {
        fs.writeFileSync(filename, buf);
    });
}


var cam = new v4l2camera.Camera("/dev/video0")
cam.config({width: 352, height: 288});
cam.start();
times(6, cam.capture.bind(cam), function () {
    writePng(cam, "result1.png");
    console.log("stop");
    cam.stop(function () {
        console.log("config");
        cam.config({width: 640, height: 480});
        console.log("restart");
        cam.start();
        cam.capture(function () {
            writePng(cam, "result2.png");
            cam.stop();
        });
    });
});

