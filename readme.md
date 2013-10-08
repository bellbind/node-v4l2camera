# node-v4l2camera

Capturing images from USB(UVC) webcam on linux machines.

## Requirements

- node >= 0.10.x
- video4linux2 headers
- c and c++ compiler with -std=c11 and std=c++11
    - gcc >= 4.7

## Install

On linux machines:

```bash
npm install v4l2camera
```

## Usage

```js
var v4l2camera = require("v4l2camera");

var cam = new v4l2camera.Camera("/dev/video0", 352, 288);
cam.start();
cam.capture(function (success) {
  var rgb = cam.toRGB();
  require("fs").writeFileSync("result.raw", Buffer(rgb));
  cam.stop();
});
```

For more detail see: examples/*.js (required "pngjs" or native "png" modules)

## API

- `var cam = new v4l2camera.Camera(device, width, height)`
- `cam.start()`
- `cam.stop()`
- `cam.capture(afterCaptured)`: call `cam.toRGB()` in `afterCaptured(true)` 
- `cam.toYUYV()` => Array for each 8bit color lines YUYVYUYV...
- `cam.toRGB()` => Array for each 8bit color lines RGBRGB...
- `cam.device`
- `cam.width`
- `cam.height`


## Build for Development

On linux machines:

```bash
cd myproject
mkdir -p node_modules
cd node_modules
git clone https://github.com/bellbind/node-v4l2camera.git v4l2camera
cd v4l2camera
npm install
cd ../..
```

"build/Release/v4l2camera.node" is exist after the build.

## Tested Environments

- Ubuntu raring armhf on BeagleBone Black with Buffalo BSW13K10H
- Ubuntu raring amd64 on Acer Aspire One with its facecam

## Licenses

[MIT](http://opensource.org/licenses/MIT) and 
[LGPL-3.0](http://opensource.org/licenses/LGPL-3.0) dual
