# node-v4l2camera

Capturing images from USB(UVC) webcam on linux machines.

## Requirements

- node >= 4.x
- video4linux2 headers
- c and c++ compiler with `-std=c11` and `-std=c++14`
    - gcc >= 4.9

## Install

On linux machines:

```bash
npm install v4l2camera
```

- package details: https://npmjs.org/package/v4l2camera

## Usage

```js
var v4l2camera = require("v4l2camera");

var cam = new v4l2camera.Camera("/dev/video0");
if (cam.configGet().formatName !== "MJPG") {
  console.log("NOTICE: MJPG camera required");
  process.exit(1);
}
cam.start();
cam.capture(function (success) {
  var frame = cam.frameRaw();
  require("fs").createWriteStream("result.jpg").end(Buffer(frame));
  cam.stop();
});
```

For more detail see: examples/*.js (required "pngjs" modules)

## API

Initializing and Configuration API

- `var cam = new v4l2camera.Camera(device)`
    - `device`: e.g. `"/dev/video0"`
- `cam.formats`: Array of available frame formats
- `var format = cam.formats[n]`
    - `format.formatName`: Name of pixel format. e.g. `"YUYV"`, `"MJPG"`
    - `format.format`: ID number of pixel format
    - `format.width`: Frame width
    - `format.height`: Frame height
    - `format.interval.numerator` and `format.interval.denominator`
      : Capturing interval per `numerator/denominator` seconds 
      (e.g. 30fps is 1/30)
- `cam.configSet(format)`
  : Set capture `width`, `height`, `interval` per `numerator/denominator` sec
  if the members exist in the `format` object
- `cam.configGet()` : Get a `format` object of current config

Capturing API (control flow)

- `cam.start()`
- `cam.stop(afterStoped)`
    - call re-`config(format)` or re-`start()` in `afterStoped()` callback
- `cam.capture(afterCaptured)`: Do cache a current captured frame
    - use `cam.frameRaw()` in `afterCaptured(true)` callback

Capturing API (frame access)

- `cam.frameRaw()`: Get the cached raw frame as `Uint8Array`
   (YUYU frame is array of YUYV..., MJPG frame is single JPEG compressed data)
- `cam.toYUYV()`: Get the cached frame as `Uint8Array` of pixels YUYVYUYV...
   (will be deprecated method)
- `cam.toRGB()`: Get the cached frame as `Uint8Array` of pixels RGBRGB...
   (will be deprecated method)

Capturing API (camera frame info)

- `cam.device`: the device file name e.g. `"/dev/video0"`
- `cam.width`: pixel width of the camera
- `cam.height`: pixel height of the camera

Control API

- `cam.controls`: Array of the control information
- `cam.controlGet(id)`: Get int value of the control of the `id`
  (id is one of cam.controls[n].id)
- `cam.controlSet(id, value)`: Set int value of the control of the `id`
- `var control = cam.controls[n]`: Control spec
    - `control.id`: Control `id` for controlGet and controlSet
    - `control.name`: Control name string
    - `control.type`: `"int"`, `"bool"`, `"button"`, `"menu"` or other types
    - `control.max`, `control.min`, `control.step`: value should be
      `min <= v` and `v <= max` and `(v - min) % step === 0`
    - `control.default`: default value of the control
    - `control.flags`: Several bool flags of the controls
    - `control.menu`: Array of items. 
      A control value is the index of the menu item when type is `"menu"`.

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

- Ubuntu wily armhf on BeagleBone Black with USB Buffalo BSW13K10H
- Ubuntu wily amd64 on Acer Aspire One with its screen facecam
- [Travis-CI (build only)](https://travis-ci.org/bellbind/node-v4l2camera):
  ![Build Status](https://travis-ci.org/bellbind/node-v4l2camera.svg)

## Licenses

[MIT](http://opensource.org/licenses/MIT) and 
[LGPL-3.0](http://opensource.org/licenses/LGPL-3.0) dual
