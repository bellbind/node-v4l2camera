{
  "targets": [{
    "target_name": "v4l2camera", 
    "sources": ["capture.c", "v4l2camera.cc"],
    "cflags": ["-Wall", "-Wextra", "-pedantic"],
    "cflags_c": ["-std=c11", "-Wno-unused-parameter"], 
    "cflags_cc": ["-std=c++11"]
  }]
}

