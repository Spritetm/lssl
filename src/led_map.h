#include <stdint.h>


void led_map_create(int size, int dimensions);
void led_map_set_led_pos(int led, float x, float y, float z);
int led_map_get_led_pos(int led, float *x, float *y, float *z);
int led_map_get_led_pos_fixed(int led, int32_t *pos);

int led_map_get_size();
int led_map_get_dimensions();
void *led_map_get_blob(int *len_bytes);
//Note: calling this passes ownership from the region pointed to by data
//over to the led_map logic (which will free() it when the time comes)
void led_map_set_blob(void *data, int len_bytes);
