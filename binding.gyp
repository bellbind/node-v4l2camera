# -*- mode: python -*-
{
    "targets": [{
        "target_name": "v4l2camera", 
        "sources": ["capture.c", "v4l2camera.cc"],
        "include_dirs" : [
 	    "<!(node -e \"require('nan')\")"
	],
        "cflags": ["-Wall", "-Wextra", "-pedantic", "-O3"],
        "xcode_settings": {
    	    "OTHER_CPLUSPLUSFLAGS": ["-std=c++14"],
        },
        "libraries": [
            "-lv4l2"
        ],
        "cflags_c": ["-std=c11", "-Wunused-parameter"], 
        "cflags_cc": ["-std=c++14"]
    }]
}

