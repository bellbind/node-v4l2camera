#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CAMERA_INFO = 0,
  CAMERA_FAIL = 1,
  CAMERA_ERROR = 2,
} camera_log_t;

typedef struct {
  uint8_t* start;
  size_t length;
} camera_buffer_t;

typedef struct {
  void* pointer;
  void (*log)(camera_log_t type, const char* msg, void* pointer);
} camera_context_t;

typedef struct {
  int fd;
  uint32_t width;
  uint32_t height;
  size_t buffer_count;
  camera_buffer_t* buffers;
  camera_buffer_t head;
  camera_context_t context;
} camera_t;


camera_t* camera_open(const char * device, uint32_t width, uint32_t height);
bool camera_init(camera_t* camera);
bool camera_start(camera_t* camera);
bool camera_stop(camera_t* camera);
bool camera_finish(camera_t* camera);
bool camera_close(camera_t* camera);
bool camera_capture(camera_t* camera);
uint8_t* yuyv2rgb(uint8_t* yuyv, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif
