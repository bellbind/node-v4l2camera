
var v4l2camera = require("../");

var fs = require("fs");

var times = function (n, async, cont) {
    for (var i = 0; i < n; i++) {
        cont = (function (c) {
            return function () {async(c);};
        })(cont);
    }
    return cont();
};

var cam = new v4l2camera.Camera("/dev/video0");
cam.configSet({width: 320, height: 240});
cam.start();
times(6, cam.capture.bind(cam), function () {
    var raw = cam.frameRaw();
    var img = fs.createWriteStream("result.jpg");
    img.write(Buffer(raw));
    cam.stop();
});
console.log("w: " + cam.width + " h: " + cam.height);
