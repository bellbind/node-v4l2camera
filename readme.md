# node-v4l2camera

Capturing images from USB(UVC) webcam on linux machines.

## Requirements

- node >= 0.10.x
- video4linux2 headers
- c and c++ compiler with `-std=c11` and `-std=c++11`
    - gcc >= 4.7

## Install

On linux machines:

```bash
npm install v4l2camera
```

## Usage

```js
var v4l2camera = require("v4l2camera");

var cam = new v4l2camera.Camera("/dev/video0");
cam.start();
cam.capture(function (success) {
  var rgb = cam.toRGB();
  require("fs").writeFileSync("result.raw", Buffer(rgb));
  cam.stop();
});
```

For more detail see: examples/*.js (required "pngjs" or native "png" modules)

## API

Initializing API

- `var cam = new v4l2camera.Camera(device)`
- `cam.formats`: Array of available frame formats
- `var format = cam.formats[n]`
    - `format.formatName`: name of pixel format. e.g. `"YUYV"`
    - `format.format`: id of pixel format
    - `format.width`: frame width
    - `format.height`: frame height
    - `format.interval.numerator` and `format.interval.denominator`
      : capturing interval per `numerator/denominator` seconds 
      (e.g. 30fps is 1/30)
- `cam.config({width: w, height: h, interval: {numerator: n, denominator: d}})`
  : set capture width, height and interval per `n/d` sec

Capturing API

- `cam.start()`
- `cam.stop()`
- `cam.capture(afterCaptured)`: cache a current captured frame
    - call `cam.toRGB()` in `afterCaptured(true)` callback
- `cam.toYUYV()`: get the cached frame as 8bit int Array of pixels YUYVYUYV...
- `cam.toRGB()`: get the cached frame as 8bit int Array of pixels RGBRGB...
- `cam.device`
- `cam.width`
- `cam.height`

Control API

- `cam.controls`: Array of the control information
- `cam.controlGet(id)`: Get int value of the control of `id`
  (id is one of cam.controls[n].id)
- `cam.controlSet(id, value)`: Set int value of the control of `id`
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

- Ubuntu raring armhf on BeagleBone Black with USB Buffalo BSW13K10H
- Ubuntu raring amd64 on Acer Aspire One with its screen facecam

## Licenses

[MIT](http://opensource.org/licenses/MIT) and 
[LGPL-3.0](http://opensource.org/licenses/LGPL-3.0) dual
