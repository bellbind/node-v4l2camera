#ifndef CAMERA_H
#define CAMERA_H

#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#ifndef __V4L2_COMMON__
#  define CAMERA_OLD_VIDEODEV2_H
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CAMERA_INFO = 0,
  CAMERA_FAIL = 1,
  CAMERA_ERROR = 2,
} camera_log_t;
typedef void (*camera_log_func_t)(camera_log_t type, const char* msg, 
                                  void* pointer);

typedef struct {
  void* pointer;
  camera_log_func_t log;
} camera_context_t;

typedef struct {
  uint8_t* start;
  size_t length;
} camera_buffer_t;

typedef struct {
  int fd;
  bool initialized;
  uint32_t width;
  uint32_t height;
  size_t buffer_count;
  camera_buffer_t* buffers;
  camera_buffer_t head;
  camera_context_t context;
} camera_t;

camera_t* camera_open(const char * device);
bool camera_start(camera_t* camera);
bool camera_stop(camera_t* camera);
bool camera_close(camera_t* camera);

bool camera_capture(camera_t* camera);
uint8_t* yuyv2rgb(const uint8_t* yuyv, uint32_t width, uint32_t height);


typedef struct {
  uint32_t format;
  uint32_t width;
  uint32_t height;
  struct {
    uint32_t numerator;
    uint32_t denominator;
  } interval;
} camera_format_t;

typedef struct {
  size_t length;
  camera_format_t* head;
} camera_formats_t;

/* convert 4 char name and id. e.g. "YUYV" */
uint32_t camera_format_id(const char* name);
void camera_format_name(uint32_t format_id, char* name);

camera_formats_t* camera_formats_new(const camera_t* camera);
void camera_formats_delete(camera_formats_t* formats);
bool camera_config_get(camera_t* camera, camera_format_t* format);
bool camera_config_set(camera_t* camera, const camera_format_t* format);


typedef enum {
  CAMERA_CTRL_INTEGER = 1,
  CAMERA_CTRL_BOOLEAN = 2,
  CAMERA_CTRL_MENU = 3,
  CAMERA_CTRL_BUTTON = 4,
  CAMERA_CTRL_INTEGER64 = 5,
  CAMERA_CTRL_CLASS = 6,
  CAMERA_CTRL_STRING = 7,
  CAMERA_CTRL_BITMASK = 8,
  CAMERA_CTRL_INTEGER_MENU = 9,
} camera_control_type_t;

typedef union {
  uint8_t name[32];
  int64_t value;
} camera_menu_t;
  
typedef struct {
  size_t length;
  camera_menu_t* head;
} camera_menus_t;

typedef struct {
  uint32_t id;
  uint8_t name[32];
  struct {
    unsigned disabled: 1;
    unsigned grabbed: 1;
    unsigned read_only: 1;
    unsigned update: 1;
    unsigned inactive: 1;
    unsigned slider: 1;
    unsigned write_only: 1;
    unsigned volatile_value: 1;
  } flags;
  camera_control_type_t type;
  int32_t max;
  int32_t min;
  int32_t step;
  int32_t default_value;
  camera_menus_t menus;
} camera_control_t;

typedef struct {
  size_t length;
  camera_control_t* head;
} camera_controls_t;
  
camera_controls_t* camera_controls_new(const camera_t* camera);
void camera_controls_delete(camera_controls_t* controls);
bool camera_control_get(camera_t* camera, uint32_t id, int32_t* value);
bool camera_control_set(camera_t* camera, uint32_t id, int32_t value);


#ifdef __cplusplus
}
#endif

#endif
