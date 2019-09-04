// this script for travis-ci, but its linux kernel is old 2.6 
var v4l2camera = require("./");

try {
  const camera = v4l2camera.Camera('/dev/video0');
  camera.configSet(camera.formats[6]);
  console.log(camera.controlGet(9963778));
camera.controlSet(9963778, 50)
  console.log(camera.controlGet(9963778));
  debugger;
} catch (error) {
debugger;
}
