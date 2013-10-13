/*
 * print formats example from UVC cam
 * build:
 *   gcc -std=c11 -Wall -Wextra -c capture.c -o capture.o
 *   gcc -std=c11 -Wall -Wextra -c list-formats.c -o list-formats.o
 *   gcc -Wall -Wextra capture.o list-formats.o -o list-formats
 */
#include "../capture.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

int main(int argc, char* argv[])
{
  char* device = argc > 1 ? argv[1] : "/dev/video0";
  
  camera_t* camera = camera_open(device);
  if (!camera) {
    fprintf(stderr, "[%s] %s\n", device, strerror(errno));
    return EXIT_FAILURE;
  }
  
  char name[5];
  camera_format_t format;
  camera_config_get(camera, &format);
  camera_format_name(format.format, name);
  puts("[current config]");
  printf("- [%s] w: %d, h: %d, fps: %d/%d\n",
         name, format.width, format.height,
         format.interval.denominator,
         format.interval.numerator);
  
  puts("[available formats]");
  camera_formats_t* formats = camera_formats_new(camera);
  for (size_t i = 0; i < formats->length; i++) {
    camera_format_name(formats->head[i].format, name);
    printf("- [%s] w: %d, h: %d, fps: %d/%d\n",
           name, formats->head[i].width, formats->head[i].height,
           formats->head[i].interval.denominator,
           formats->head[i].interval.numerator);
  }
  camera_formats_delete(formats);
  camera_close(camera);
  return EXIT_SUCCESS;
}
