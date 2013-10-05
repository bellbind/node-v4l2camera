#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t* start;
  size_t length;
} buffer_t;

typedef struct {
  int fd;
  uint32_t width;
  uint32_t height;
  size_t buffer_count;
  buffer_t* buffers;
  buffer_t head;
} camera_t;


camera_t* camera_open(const char * device, uint32_t width, uint32_t height);
void camera_init(camera_t* camera);
void camera_start(camera_t* camera);
void camera_stop(camera_t* camera);
void camera_finish(camera_t* camera);
void camera_close(camera_t* camera);
int camera_capture(camera_t* camera);
uint8_t* yuyv2rgb(uint8_t* yuyv, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif
