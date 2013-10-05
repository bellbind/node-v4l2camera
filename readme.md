# node-v4l2camera

Capturing image from USB(UVC) webcam on linux.

WARNING: it is an experimetal work. error folow is not implemented yet.

## Install

On linux:

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

(not yet registered to npm registry)

## Usage

`var v4l2camera = require("v4l2camera");`

see: examples/*.js (required "pngjs" or native "png")

## API

- `var cam = new v4l2camera.V4l2Camera(device, width, height)`
- `cam.start()`
- `cam.stop()`
- `cam.capture(afterCaptured)`: call `cam.toRGB()` in `afterCaptured()` 
- `cam.toRGB()` => array like object for each 8bit color lines RGBRGB...
- `cam.device`
- `cam.width`
- `cam.height`
