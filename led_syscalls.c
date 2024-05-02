#include <stddef.h>
#include <stdio.h>
#include "vm_syscall.h"
#include "led_syscalls.h"

static int32_t led_cb_handle=0;
static int32_t cur_led_col[3];

int32_t register_led_cb(lssl_vm_t *vm, int32_t *args, int argct) {
	printf("register_led_cb: %d\n", args[0]);
	led_cb_handle=args[0];
	return 0;
}

int32_t led_set_rgb(lssl_vm_t *vm, int32_t *args, int argct) {
	cur_led_col[0]=args[0];
	cur_led_col[1]=args[1];
	cur_led_col[2]=args[2];
	return 0;
}

static const vm_syscall_list_entry_t led_syscalls[]={
	{"register_led_cb", register_led_cb, 1},
	{"led_set_rgb", led_set_rgb, 3}
};

void led_syscalls_init() {
	vm_syscall_add_local_syscalls(led_syscalls, sizeof(led_syscalls)/sizeof(vm_syscall_list_entry_t));
}

int led_syscalls_calculate_led(lssl_vm_t *vm, int32_t led, float time) {
	vm_error_en error=0;
	int32_t args[2]={led<<16, (time*65536)};
	lssl_vm_run_function(vm, led_cb_handle, 2, args, &error);
	if (error) {
		printf("calculate_led vm error %s\n", vm_err_to_str(error));
		return 0;
	}
	return 1;
}

void led_syscalls_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b) {
	*r=cur_led_col[0]>>16;
	*g=cur_led_col[1]>>16;
	*b=cur_led_col[2]>>16;
}
