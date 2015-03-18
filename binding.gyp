{
  "targets": [{
    "target_name": "v4l2camera", 
    "sources": ["capture.c", "v4l2camera.cc"],
    "include_dirs" : [
 	   "<!(node -e \"require('nan')\")"
	],
    "cflags": ["-Wall", "-Wextra", "-pedantic"],
    "xcode_settings": {
    	"OTHER_CPLUSPLUSFLAGS": ["-std=c++11"],
    },
    "cflags_c": ["-std=c11", "-Wno-unused-parameter"], 
    "cflags_cc": ["-std=c++11"]
  }]
}

