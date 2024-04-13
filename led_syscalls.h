#include "vm.h"


void led_syscalls_init();
void led_syscalls_calculate_led(lssl_vm_t *vm, int32_t led, float time);
void led_syscalls_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b);

