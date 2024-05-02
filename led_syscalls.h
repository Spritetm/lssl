#include "vm.h"


void led_syscalls_init();
int led_syscalls_frame_start(lssl_vm_t *vm);
int led_syscalls_calculate_led(lssl_vm_t *vm, int32_t led, float time);
void led_syscalls_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b);

