#include "capture.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libv4l2.h>

typedef struct {
	uint16_t width;
	uint16_t height;
} frame_size_t;

frame_size_t frame_sizes[] = {
   { 640, 480 }
  ,{ 800, 600 }
  ,{ 1024, 768 }
  ,{ 1440, 1080 }
  //,{ 1920, 1440 }
  ,{ 1024, 576 }
  ,{ 1280, 720 }
  ,{ 1920, 1080 }
  //,{ 2560, 1440 }
  //,{ 3840, 2160 }
};

uint8_t frame_intervals[] = {
  5,
  10,
  15,
  20,
  30,
  //45,
  //60,
  //90,
  //120
};

static void log_stderr(camera_log_t type, const char* msg, void* pointer) {
  (void) pointer;
  switch (type) {
  case CAMERA_ERROR:
    fprintf(stderr, "ERROR [%s] %d: %s\n", msg, errno, strerror(errno));
    return;
  case CAMERA_FAIL:
    fprintf(stderr, "FAIL [%s]\n", msg);
    return;
  case CAMERA_INFO:
    fprintf(stderr, "INFO [%s]\n", msg);
    return;
  }
}

static bool error(const camera_t* camera, const char * msg)
{
  camera->context.log(CAMERA_ERROR, msg, camera->context.pointer);
  return false;
}
static bool failure(const camera_t* camera, const char * msg)
{
  camera->context.log(CAMERA_FAIL, msg, camera->context.pointer);
  return false;
}

static int xioctl(int fd, unsigned long int request, void* arg)
{
  for (int i = 0; i < 100; i++) {
    int r = v4l2_ioctl(fd, request, arg);
    if (r != -1 || errno != EINTR) return r;
  }
  return -1;
}



camera_t* camera_open(const char * device)
{
  int fd = v4l2_open(device, O_RDWR | O_NONBLOCK, 0);
  if (fd == -1) return NULL;
  
  camera_t* camera = malloc(sizeof (camera_t));
  camera->fd = fd;
  camera->initialized = false;
  camera->width = 0;
  camera->height = 0;
  camera->buffer_count = 0;
  camera->buffers = NULL;
  camera->head.length = 0;
  camera->head.start = NULL;
  camera->context.pointer = NULL;
  camera->context.log = &log_stderr;
  return camera;
}

static void free_buffers(camera_t* camera, size_t count)
{
  for (size_t i = 0; i < count; i++) {
    v4l2_munmap(camera->buffers[i].start, camera->buffers[i].length);
  }
  free(camera->buffers);
  camera->buffers = NULL;
  camera->buffer_count = 0;
}

static bool camera_init(camera_t* camera) {
  struct v4l2_capability cap;
  if (xioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1)
    return error(camera, "VIDIOC_QUERYCAP");
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    return failure(camera, "no capture");
  if (!(cap.capabilities & V4L2_CAP_STREAMING))
    return failure(camera, "no streaming");

  struct v4l2_cropcap cropcap;
  memset(&cropcap, 0, sizeof cropcap);
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(camera->fd, VIDIOC_CROPCAP, &cropcap) == 0) {
    struct v4l2_crop crop;
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect;
    if (xioctl(camera->fd, VIDIOC_S_CROP, &crop) == -1) {
      // cropping not supported
    }
  }
  camera->initialized = true;
  return true;
}

static bool camera_buffer_prepare(camera_t* camera)
{
  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof req);
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (xioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1)
    return error(camera, "VIDIOC_REQBUFS");
  camera->buffer_count = req.count;
  camera->buffers = calloc(req.count, sizeof (camera_buffer_t));

  size_t buf_max = 0;
  for (size_t i = 0; i < camera->buffer_count; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (xioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1) {
      free_buffers(camera, i);
      return error(camera, "VIDIOC_QUERYBUF");
    }
    if (buf.length > buf_max) buf_max = buf.length;
    camera->buffers[i].length = buf.length;
    camera->buffers[i].start = 
      v4l2_mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, 
           camera->fd, buf.m.offset);
    if (camera->buffers[i].start == MAP_FAILED) {
      free_buffers(camera, i);
      return error(camera, "mmap");
    }
  }
  camera->head.start = calloc(buf_max, sizeof (uint8_t));
  return true;
}

static void camera_buffer_finish(camera_t* camera)
{
  free_buffers(camera, camera->buffer_count);
  free(camera->head.start);
  camera->head.length = 0;
  camera->head.start = NULL;
}

static bool camera_load_settings(camera_t* camera)
{
  struct v4l2_format format;
  memset(&format, 0, sizeof format);
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(camera->fd, VIDIOC_G_FMT, &format) == -1)
    return error(camera, "VIDIOC_G_FMT");
  camera->width = format.fmt.pix.width;
  camera->height = format.fmt.pix.height;
  return true;
}

static bool camera_load(camera_t* camera)
{
  if (!camera->initialized) {
    if (!camera_init(camera)) return false;
  }
  if (camera->buffer_count == 0) {
    if (!camera_load_settings(camera)) return false;
    if (!camera_buffer_prepare(camera)) return false;
  }
  return true;
}

bool camera_stop(camera_t* camera)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1) 
    return error(camera, "VIDIOC_STREAMOFF");
  camera_buffer_finish(camera);
  
  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof req);
  req.count = 0;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (xioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1)
    return error(camera, "VIDIOC_REQBUFS 0");
  return true;
}

bool camera_start(camera_t* camera)
{
  if (!camera_load(camera)) return false;

  for (size_t i = 0; i < camera->buffer_count; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) 
      return error(camera, "VIDIOC_QBUF");
  }
  
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(camera->fd, VIDIOC_STREAMON, &type) == -1) 
    return error(camera, "VIDIOC_STREAMON");
  return true;
}


bool camera_close(camera_t* camera)
{
  if (camera->buffer_count > 0) {
    camera_stop(camera);
  }
  for (int i = 0; i < 10; i++) {
    if (v4l2_close(camera->fd) != -1) break;
  }
  free(camera);
  return true;
}


//[[capturing]
bool camera_capture(camera_t* camera)
{
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof buf);
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if (xioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1) return false;
  memcpy(camera->head.start, camera->buffers[buf.index].start, buf.bytesused);
  camera->head.length = buf.bytesused;
  if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) return false;
  return true;
}


static inline int minmax(int min, int v, int max)
{
  return (v < min) ? min : (max < v) ? max : v;
}
static inline uint8_t yuv2r(int y, int u, int v)
{
  (void) u; return minmax(0, (y + 359 * v) >> 8, 255);
}
static inline uint8_t yuv2g(int y, int u, int v)
{
  return minmax(0, (y + 88 * v - 183 * u) >> 8, 255);
}
static inline uint8_t yuv2b(int y, int u, int v)
{
  (void) v; return minmax(0, (y + 454 * u) >> 8, 255);
}
uint8_t* yuyv2rgb(const uint8_t* yuyv, uint32_t width, uint32_t height)
{
  uint8_t* rgb = calloc(width * height * 3, sizeof (uint8_t));
  for (size_t i = 0; i < height; i++) {
    for (size_t j = 0; j < width; j += 2) {
      size_t index = i * width + j;
      size_t index2 = index * 2, index3 = index * 3;
      int y0 = yuyv[index2 + 0] << 8;
      int u = yuyv[index2 + 1] - 128;
      int y1 = yuyv[index2 + 2] << 8;
      int v = yuyv[index2 + 3] - 128;
      rgb[index3 + 0] = yuv2r(y0, u, v);
      rgb[index3 + 1] = yuv2g(y0, u, v);
      rgb[index3 + 2] = yuv2b(y0, u, v);
      rgb[index3 + 3] = yuv2r(y1, u, v);
      rgb[index3 + 4] = yuv2g(y1, u, v);
      rgb[index3 + 5] = yuv2b(y1, u, v);
    }
  }
  return rgb;
}


//[formats and config]
uint32_t camera_format_id(const char* name)
{
  //assert(strlen(name) == 4);
  return (uint32_t) name[0] | ((uint32_t) name[1] << 8) |
    ((uint32_t) name[2] << 16) | ((uint32_t) name[3] << 24);
}
void camera_format_name(uint32_t format_id, char* name)
{
  name[0] = format_id & 0xff;
  name[1] = (format_id >> 8) & 0xff;
  name[2] = (format_id >> 16) & 0xff;
  name[3] = (format_id >> 24) & 0xff;
  name[4] = '\0';
}

static bool camera_format_set(camera_t* camera, const camera_format_t* format)
{
  if (format->width > 0 && format->height > 0) {
    uint32_t pixformat = format->format ? format->format : V4L2_PIX_FMT_YUYV;
    struct v4l2_format vformat;
    memset(&vformat, 0, sizeof vformat);
    vformat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vformat.fmt.pix.width = format->width;
    vformat.fmt.pix.height = format->height;
    vformat.fmt.pix.pixelformat = pixformat;
    vformat.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(camera->fd, VIDIOC_S_FMT, &vformat) == -1)
      return error(camera, "VIDIOC_S_FMT");
  }
  if (format->interval.numerator != 0 && format->interval.denominator != 0) {
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof parm);
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = format->interval.numerator;
    parm.parm.capture.timeperframe.denominator = format->interval.denominator;
    if (xioctl(camera->fd, VIDIOC_S_PARM, &parm) == -1)
      return error(camera, "VIDIOC_S_PARM");    
  }
  return true;
}

static bool camera_format_get(camera_t* camera, camera_format_t* format)
{
  struct v4l2_format vformat;
  memset(&vformat, 0, sizeof vformat);
  vformat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(camera->fd, VIDIOC_G_FMT, &vformat) == -1)
    return error(camera, "VIDIOC_G_FMT");
  
  format->format = vformat.fmt.pix.pixelformat;
  format->width = vformat.fmt.pix.width;
  format->height = vformat.fmt.pix.height;
  
  struct v4l2_streamparm parm;
  memset(&parm, 0, sizeof parm);
  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(camera->fd, VIDIOC_G_PARM, &parm) == -1)
    return error(camera, "VIDIOC_G_PARM");
  format->interval.numerator = parm.parm.capture.timeperframe.numerator;
  format->interval.denominator = parm.parm.capture.timeperframe.denominator;
  return true;
}

bool camera_config_get(camera_t* camera, camera_format_t* format)
{
  return camera_format_get(camera, format);
}
bool camera_config_set(camera_t* camera, const camera_format_t* format)
{
  if (camera->buffer_count > 0) {
    if (!camera_stop(camera)) return false;
  }
  if (!camera->initialized) {
    if (!camera_init(camera)) return false;
  }
  if (!camera_format_set(camera, format)) return false;
  if (!camera_load_settings(camera)) return false;
  return camera_buffer_prepare(camera);
}

camera_formats_t*  camera_formats_new(const camera_t* camera)
{
  camera_formats_t* ret = malloc(sizeof (camera_formats_t));
  ret->length = 0;
  ret->head = NULL;
  for (uint32_t i = 0; ; i++) {
    struct v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.index = i;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(camera->fd, VIDIOC_ENUM_FMT, &fmt) == -1) break;
    //printf("[%s]\n", fmt.description);
    for (uint32_t j = 0; ; j++) {
      struct v4l2_frmsizeenum frmsize;
      memset(&frmsize, 0, sizeof frmsize);
      frmsize.index = j;
      frmsize.pixel_format = fmt.pixelformat;
      if (v4l2_ioctl(camera->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == -1) break;
      if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { // discrete framesizes
        for (uint32_t k = 0; ; k++) { // discrete frame sizes
          struct v4l2_frmivalenum frmival;
          memset(&frmival, 0, sizeof frmival);
          frmival.index = k;
          frmival.pixel_format = fmt.pixelformat;
          frmival.width = frmsize.discrete.width;
          frmival.height = frmsize.discrete.height;
          //printf("checking frameintervals for discrete size: %dx%dpx\n", frmival.width, frmival.height);
          if (v4l2_ioctl(camera->fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == -1) {
            //printf("frameintervals ioctl failed\n");
            break;
          }
          if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) { // discrete interval
            //printf("\t- fps: %d/%d\n",
            //       frmival.discrete.numerator,frmival.discrete.denominator);
            add_format(ret, fmt.pixelformat, frmival.width, frmival.height, frmival.discrete.numerator, frmival.discrete.denominator);
          } else { // stepwise intervals
            uint8_t max = frmival.stepwise.max.numerator/frmival.stepwise.max.denominator;
            uint8_t min = frmival.stepwise.min.numerator/frmival.stepwise.min.denominator;
            for (uint8_t i = 0; ; i++) {
              if (frame_intervals[i] >= min && frame_intervals[i] <= max) {
                //printf("\t- fps: %d\n",frame_intervals[i]);
                add_format(ret, fmt.pixelformat, frmsize.discrete.width, frmsize.discrete.height, frame_intervals[i], 1 /* denominator */);
              }
            }
          }
        }
      } else {
        for (uint8_t l = 0; l < sizeof(frame_sizes) / sizeof(frame_size_t); l++) { // step wise frame sizes
          //printf("checking frameintervals for stepwise %dx%dpx\n", frame_sizes[l].width, frame_sizes[l].height);
          for (uint32_t k = 0; ; k++) { // step wise frame intervals
            struct v4l2_frmivalenum frmival;
            memset(&frmival, 0, sizeof frmival);
            frmival.index = k;
            frmival.pixel_format = fmt.pixelformat;
            frmival.width = frame_sizes[l].width;
            frmival.height = frame_sizes[l].height;
            if (v4l2_ioctl(camera->fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) != -1) {
              if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                //printf("\t- fps: %d/%d\n",
                    //       frmival.discrete.numerator,frmival.discrete.denominator);
                add_format(ret, fmt.pixelformat, frmival.width, frmival.height, frmival.discrete.numerator, frmival.discrete.denominator);
              } else {
                uint8_t min = frmival.stepwise.max.denominator/frmival.stepwise.max.numerator;
                uint8_t max = frmival.stepwise.min.denominator/frmival.stepwise.min.numerator;
                for (uint8_t m = 0; m < sizeof(frame_intervals) / sizeof(uint8_t); m++) {
                  if (frame_intervals[m] >= min && frame_intervals[m] <= max) {
                    //printf("\t- fps: %d\n",frame_intervals[i]);
                    add_format(ret, fmt.pixelformat, frmival.width, frmival.height, 1, frame_intervals[m] /* denominator */);
                  }
                }
                break;
              }
            } else {
              //printf("frameintervals ioctl failed\n");
              break;
            }
          }
        }
      }
    }
  }

  return ret;
}

void add_format(camera_formats_t* ret, uint32_t format, uint32_t width, uint32_t height, uint32_t numerator, uint32_t denominator)
{
  //printf("\tadding format: %d w:%d h:%d n:%d d:%d\n", format, width, height, numerator, denominator);
  ret->head = realloc(ret->head, (ret->length + 1) * sizeof (camera_format_t));
  ret->head[ret->length].format = format;
  ret->head[ret->length].width = width;
  ret->head[ret->length].height = height;
  ret->head[ret->length].interval.numerator = numerator;
  ret->head[ret->length].interval.denominator = denominator;
  ret->length++;
}

void camera_formats_delete(camera_formats_t* formats)
{
  free(formats->head);
  free(formats);
}


//[controls]
static void 
camera_menu_copy(camera_menu_t* menu, struct v4l2_querymenu* qmenu)
{
  memcpy(menu->name, qmenu->name, sizeof qmenu->name);
}
#ifndef CAMERA_OLD_VIDEODEV2_H
static void 
camera_integer_menu_copy(camera_menu_t* menu, struct v4l2_querymenu* qmenu)
{
  menu->value = qmenu->value;
}
#endif
static void 
camera_controls_menus(const camera_t* camera, camera_control_t* control)
{
  void (*copy)(camera_menu_t*, struct v4l2_querymenu*) = &camera_menu_copy;
  switch (control->type) {
  case CAMERA_CTRL_MENU:
    break;
#ifndef CAMERA_OLD_VIDEODEV2_H
  case CAMERA_CTRL_INTEGER_MENU:
    copy = &camera_integer_menu_copy;
    break;
#endif
  default:
    control->menus.length = 0;
    control->menus.head = NULL;
    return;
  }
  control->menus.length = control->max + 1;
  control->menus.head = calloc(control->menus.length, sizeof (camera_menu_t));
  for (uint32_t mindex = 0; mindex < control->menus.length; mindex++) {
    struct v4l2_querymenu qmenu;
    memset(&qmenu, 0, sizeof qmenu);
    qmenu.id = control->id;
    qmenu.index = mindex;
    if (v4l2_ioctl(camera->fd, VIDIOC_QUERYMENU, &qmenu) == 0) {
      copy(&control->menus.head[mindex], &qmenu);
    } else {
      error(camera, "VIDIOC_QUERYMENU");
    }
  }
}
static camera_control_t* 
camera_controls_query(const camera_t* camera, camera_control_t* control_list)
{
  camera_control_t* control_list_last = control_list;
  struct v4l2_queryctrl qctrl;
  
  for (qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
       v4l2_ioctl(camera->fd, VIDIOC_QUERYCTRL, &qctrl) == 0;
       qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL) {
    camera_control_t* control = control_list_last++;
    control->id = qctrl.id;
    memcpy(control->name, qctrl.name, sizeof qctrl.name);
    control->flags.disabled = (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) != 0;
    control->flags.grabbed = (qctrl.flags & V4L2_CTRL_FLAG_GRABBED) != 0;
    control->flags.read_only = (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) != 0;
    control->flags.update = (qctrl.flags & V4L2_CTRL_FLAG_UPDATE) != 0;
    control->flags.inactive = (qctrl.flags & V4L2_CTRL_FLAG_INACTIVE) != 0;
    control->flags.slider = (qctrl.flags & V4L2_CTRL_FLAG_SLIDER) != 0;
    control->flags.write_only = (qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY) != 0;
    control->flags.volatile_value = 
      (qctrl.flags & V4L2_CTRL_FLAG_VOLATILE) != 0;
    control->type = qctrl.type;
    control->max = qctrl.maximum;
    control->min = qctrl.minimum;
    control->step = qctrl.step;
    control->default_value = qctrl.default_value;
    camera_controls_menus(camera, control);
  }
  return control_list_last;
}
camera_controls_t* camera_controls_new(const camera_t* camera)
{
  camera_control_t control_list[V4L2_CID_MAX_CTRLS]; // It can't be a higher qty :P
  camera_control_t* control_list_last = 
    camera_controls_query(camera, control_list);
  camera_controls_t* controls = malloc(sizeof (camera_controls_t));
  controls->length = control_list_last - control_list;
  controls->head = calloc(controls->length, sizeof (camera_control_t));
  for (size_t i = 0; i < controls->length; i++) {
    controls->head[i] = control_list[i];
  }
  return controls;
}

void camera_controls_delete(camera_controls_t* controls)
{
  for (size_t i = 0; i < controls->length; i++) {
    free(controls->head[i].menus.head);
  }
  free(controls);
}

bool camera_control_get(camera_t* camera, uint32_t id, int32_t* value)
{
  int ioctl_result;
  struct v4l2_control ctrl;
  ctrl.id = id;
  ctrl.value = 0;

  for(int i = 0; i < 100; i++) {
    ioctl_result = v4l2_ioctl(camera->fd, VIDIOC_G_CTRL, &ctrl);
    if (ioctl_result != -1)
      break;
    else if(errno != EIO)
      return error(camera, "VIDIOC_G_CTRL");
  }

  if(ioctl_result == -1)
    return error(camera, "VIDIOC_G_CTRL");

  *value = ctrl.value;
  return true;
}

bool camera_control_set(camera_t* camera, uint32_t id, int32_t value)
{
  struct v4l2_control ctrl;
  ctrl.id = id;
  ctrl.value = value;
  if (v4l2_ioctl(camera->fd, VIDIOC_S_CTRL, &ctrl) == -1)
    return error(camera, "VIDIOC_S_CTRL");
  return true;
}
