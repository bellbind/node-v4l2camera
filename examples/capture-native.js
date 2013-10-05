
var v4l2camera = require("./build/Release/v4l2camera");

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

var cam = new v4l2camera.V4l2Camera("/dev/video0", 352, 288);
cam.start();
times(6, cam.capture.bind(cam), function () {
    var rgb = cam.toRGB();
    var img = new png.Png(Buffer(rgb), cam.width, cam.height, "rgb");
    img.encode(function (buf) {
        fs.writeFileSync("result.png", buf);
    });
    cam.stop();
});
console.log("w: " + cam.width + " h: " + cam.height);
