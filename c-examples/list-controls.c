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

const char* type_names[] = {
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
const char* bool_names[] = {
  "false",
  "true",
};

int main(int argc, char* argv[])
{
  char* device = argc > 1 ? argv[1] : "/dev/video0";
  
  camera_t* camera = camera_open(device);
  if (!camera) {
    fprintf(stderr, "[%s] %s\n", device, strerror(errno));
    return EXIT_FAILURE;
  }
  
  camera_controls_t* controls = camera_controls_new(camera);
  for (size_t i = 0; i < controls->length; i++) {
    camera_control_t* control = &controls->head[i];
    int32_t value = 0;
    camera_control_get(camera, control->id, &value);
    printf("[%s]\n", control->name);
    printf("- id: %d\n", control->id);
    printf("- type: %s\n", type_names[control->type]);
    printf("- value: %d\n", value);
    printf("- range: %d <- %d -> %d (step: %d)\n",
           control->min, control->default_value, control->max, control->step);
    switch (control->type) {
    case CAMERA_CTRL_MENU:
      puts("- menus");
      for (size_t j = 0; j < control->menus.length; j++) {
        printf("    - %zu: [%s]\n", j, control->menus.head[j].name);
      }
      break;
    case CAMERA_CTRL_INTEGER_MENU:
      puts("- menus");
      for (size_t j = 0; j < control->menus.length; j++) {
        printf("    - %zu: %"PRId64"\n", j, control->menus.head[j].value);
      }
      break;
    default: break;
    }
    puts("- flags");
    printf("    - disabled: %s\n", bool_names[control->flags.disabled]);
    printf("    - grabbed: %s\n", bool_names[control->flags.grabbed]);
    printf("    - read only: %s\n", bool_names[control->flags.read_only]);
    printf("    - update: %s\n", bool_names[control->flags.update]);
    printf("    - inactive: %s\n", bool_names[control->flags.inactive]);
    printf("    - slider: %s\n", bool_names[control->flags.slider]);
    printf("    - write_only: %s\n", bool_names[control->flags.write_only]);
    printf("    - volatile: %s\n", bool_names[control->flags.volatile_value]);
    puts("");
  }
  
  camera_controls_delete(controls);
  camera_close(camera);
  return EXIT_SUCCESS;
}
