"use strict";

const main = () => flow(function* () {
    const v4l2camera = require("../");
    
    const cam = new v4l2camera.Camera("/dev/video0");
    if (cam.configGet().formatName !== "YUYV") {
        console.log("YUYV camera required");
        process.exit(1);
    }
    console.log(`start camera ${cam.device}`);
    
    const capturing = function* (width, height, filename) {
        cam.configSet({width, height}).start();
        for (let i = 0; i < 6; i++) yield new Promise(r => cam.capture(r));
        saveAsPng(cam.toRGB(), cam.width, cam.height, filename);
        console.log(`save (${cam.width}, ${cam.height}) as ${filename}`);
        yield new Promise(r => cam.stop(r));
    };
    yield* capturing(352, 288, "result1.png");
    yield* capturing(160, 120, "result2.png");
}).then(() => console.log("finished")).catch(err => console.log(err));


// Tiny es7 `async` like function
const flow = (gfunc) => new Promise((f, r) => {
    const g = gfunc(), n = g.next.bind(g), t = g.throw.bind(g);
    const subflows = item => item.done ? f(item.value) :
              Promise.resolve(item.value).then(n, t).then(subflows, r);
    subflows(n());
});

const saveAsPng = (rgb, width, height, filename) => {
    const fs = require("fs");
    const pngjs = require("pngjs");

    const png = new pngjs.PNG({
        width, height, deflateLevel: 1, deflateStrategy: 1,
    });
    const size = width * height;
    for (let i = 0; i < size; i++) {
        png.data[i * 4 + 0] = rgb[i * 3 + 0];
        png.data[i * 4 + 1] = rgb[i * 3 + 1];
        png.data[i * 4 + 2] = rgb[i * 3 + 2];
        png.data[i * 4 + 3] = 255;
    }
    png.pack().pipe(fs.createWriteStream(filename));
};

main();
