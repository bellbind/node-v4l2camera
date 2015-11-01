// Capture MJPG camera frame to store as jpg file

var main = function () {
    var v4l2camera = require("../");
    
    var cam = new v4l2camera.Camera("/dev/video0");
    if (cam.configGet().formatName !== "MJPG") {
        console.log("YUYV camera required");
        process.exit(1);
    }
    cam.configSet({width: 352, height: 288});
    cam.start();
    times(6, cam.capture.bind(cam), function () {
        var raw = cam.frameRaw();
        require("fs").createWriteStream("result.jpg").end(Buffer(raw));
        cam.stop();
    });
    console.log("w: " + cam.width + " h: " + cam.height);
};

var times = function (n, async, cont) {
    return async(function rec(r) {return --n == 0 ? cont(r) : async(rec);});
};

main();

