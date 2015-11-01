
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
        saveAsJpg(cam.toRGB(), cam.width, cam.height, "result.jpg");
        cam.stop();
    });
    console.log("w: " + cam.width + " h: " + cam.height);
};

var times = function (n, async, cont) {
    return async(function rec(r) {return --n == 0 ? cont(r) : async(rec);});
};
var saveAsJpg = function (rgb, width, height, filename) {
    var fs = require("fs");
    var jpegjs = require("jpeg-js");
    
    var size = width * height;
    var rgba = {data: new Buffer(size * 4), width: width, height: height};
    for (var i = 0; i < size; i++) {
        rgba.data[i * 4 + 0] = rgb[i * 3 + 0];
        rgba.data[i * 4 + 1] = rgb[i * 3 + 1];
        rgba.data[i * 4 + 2] = rgb[i * 3 + 2];
        rgba.data[i * 4 + 3] = 255;
    }
    var jpeg = jpegjs.encode(rgba, 100);
    fs.createWriteStream(filename).end(Buffer(jpeg.data));
};

main();
