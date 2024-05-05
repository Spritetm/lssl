#include "vm.h"


void led_syscalls_init();
void led_syscalls_frame_start(lssl_vm_t *vm, vm_error_t *error);
void led_syscalls_calculate_led(lssl_vm_t *vm, int32_t led, float time, vm_error_t *error);
void led_syscalls_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b);

