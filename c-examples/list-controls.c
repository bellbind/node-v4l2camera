/*
 * print controls example from UVC cam
 * build:
 *   gcc -std=c11 -Wall -Wextra -c capture.c -o capture.o
 *   gcc -std=c11 -Wall -Wextra -c list-controls.c -o list-controls.o
 *   gcc -Wall -Wextra capture.o list-controls.o -o list-controls
 */
#include "../capture.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

const char* type_strs[] = {
  "INVALID",
  "INTEGER",
  "BOOLEAN",
  "MENU",
  "BUTTON",
  "INTEGER64",
  "CLASS",
  "STRING",
  "BITMASK",
  "INTEGER_MENU",
};

int main(int argc, char* argv[])
{
  char* device = argc > 1 ? argv[1] : "/dev/video0";
  int width = argc > 2 ? atoi(argv[2]) : 352;
  int height = argc > 3 ? atoi(argv[3]) : 288;
  
  camera_t* camera = camera_open(device, width, height);
  if (!camera) {
    fprintf(stderr, "[%s] %s\n", device, strerror(errno));
    return EXIT_FAILURE;
  }
  if (!camera_init(camera)) goto error_init;
  
  camera_controls_t* controls = camera_controls_new(camera);
  for (size_t i = 0; i < controls->length; i++) {
    int32_t value = 0;
    camera_control_get(camera, controls->head[i].id, &value);
    printf("[%s]\n", controls->head[i].name);
    printf("- id: %d\n", controls->head[i].id);
    printf("- type: %s\n", type_strs[controls->head[i].type]);
    printf("- value: %d\n", value);
    printf("- range: %d <- %d -> %d (step: %d)\n",
           controls->head[i].min, controls->head[i].default_value,  
           controls->head[i].max, controls->head[i].step);
    switch (controls->head[i].type) {
    case CAMERA_CTRL_MENU:
      puts("- menus");
      for (size_t j = 0; j < controls->head[i].menus.length; j++) {
        printf("    - %s\n", controls->head[i].menus.head[j].name);
      }
      break;
    case CAMERA_CTRL_INTEGER_MENU:
      puts("- menus");
      for (size_t j = 0; j < controls->head[i].menus.length; j++) {
        printf("    - %"PRId64"\n", controls->head[i].menus.head[j].value);
      }
      break;
    default:
      break;
    }
    puts("");
  }
  
  camera_controls_delete(controls);
  camera_close(camera);
  return EXIT_SUCCESS;

 error_init:
  camera_close(camera);
  return EXIT_FAILURE;
}
