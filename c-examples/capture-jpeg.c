/*
 * jpeg capturing example from UVC cam
 * requires: libjpeg-dev
 * build:
 *   gcc -std=c11 -Wall -Wextra -c capture.c -o capture.o
 *   gcc -std=c11 -Wall -Wextra -c capture-jpeg.c -o capture-jpeg.o
 *   gcc -Wall -Wextra capture.o capture-jpeg.o -ljpeg -o capture-jpeg
 */

#include "../capture.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <jpeglib.h>

bool camera_frame(camera_t* camera, struct timeval timeout) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(camera->fd, &fds);
  int r = select(camera->fd + 1, &fds, 0, 0, &timeout);
  if (r == -1) exit(EXIT_FAILURE);
  if (r == 0) return false;
  return camera_capture(camera);
}


void 
jpeg(FILE* dest, uint8_t* rgb, uint32_t width, uint32_t height, int quality)
{
  JSAMPARRAY image = calloc(height, sizeof (JSAMPROW));
  for (size_t i = 0; i < height; i++) {
    image[i] = calloc(width * 3, sizeof (JSAMPLE));
    for (size_t j = 0; j < width; j++) {
      image[i][j * 3 + 0] = rgb[(i * width + j) * 3 + 0];
      image[i][j * 3 + 1] = rgb[(i * width + j) * 3 + 1];
      image[i][j * 3 + 2] = rgb[(i * width + j) * 3 + 2];
    }
  }
  
  struct jpeg_compress_struct compress;
  struct jpeg_error_mgr error;
  compress.err = jpeg_std_error(&error);
  jpeg_create_compress(&compress);
  jpeg_stdio_dest(&compress, dest);
  compress.image_width = width;
  compress.image_height = height;
  compress.input_components = 3;
  compress.in_color_space = JCS_RGB;
  jpeg_set_defaults(&compress);
  jpeg_set_quality(&compress, quality, TRUE);
  jpeg_start_compress(&compress, TRUE);
  jpeg_write_scanlines(&compress, image, height);
  jpeg_finish_compress(&compress);
  jpeg_destroy_compress(&compress);

  for (size_t i = 0; i < height; i++) {
    free(image[i]);
  }
  free(image);
}

int main(int argc, char* argv[])
{
  char* device = argc > 1 ? argv[1] : "/dev/video0";
  uint32_t width = argc > 2 ? atoi(argv[2]) : 352;
  uint32_t height = argc > 3 ? atoi(argv[3]) : 288;
  char* output = argc > 4 ? argv[4] : "result.jpg";
  
  camera_t* camera = camera_open(device);
  if (!camera) {
    fprintf(stderr, "[%s] %s\n", device, strerror(errno));
    return EXIT_FAILURE;
  }
  camera_format_t config = {0, width, height, {0, 0}};
  if (!camera_config_set(camera, &config)) goto error;
  if (!camera_config_get(camera, &config)) goto error;
  char name[5];
  camera_format_name(config.format, name);
  if (strcmp(name, "YUYV") != 0 || strcmp(name, "MJPG") != 0) {
    fprintf(stderr, "camera format [%s] is not supported\n", name);
    goto error;
  }

  if (!camera_start(camera)) goto error;
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  /* skip 5 frames for booting a cam */
  for (int i = 0; i < 5; i++) {
    camera_frame(camera, timeout);
  }
  camera_frame(camera, timeout);

  FILE* out = fopen(output, "w");
  if (strcmp(name, "YUYV") == 0) {
    unsigned char* rgb = 
      yuyv2rgb(camera->head.start, camera->width, camera->height);
    jpeg(out, rgb, camera->width, camera->height, 100);
    free(rgb);
  } else if (strcmp(name, "MJPG")) {
    fwrite(camera->head.start, camera->head.length, 1, out);
  }
  fclose(out);
  
  camera_stop(camera);
  camera_close(camera);
  return 0;
 error:
  camera_close(camera);
  return EXIT_FAILURE;  
}
