#include <stddef.h>
#include <stdio.h>
#include "vm_syscall.h"
#include "led_syscalls.h"
#include "error.h"
#include "vm.h"

static int32_t led_cb_handle=-1;
static int32_t frame_start_cb_handle=-1;
static int32_t cur_led_col[3];

int32_t register_led_cb(lssl_vm_t *vm, int32_t *args, int argct) {
	printf("register_led_cb: %d\n", args[0]);
	led_cb_handle=args[0];
	return 0;
}

int32_t register_frame_start_cb(lssl_vm_t *vm, int32_t *args, int argct) {
	printf("register_frame_start_cb: %d\n", args[0]);
	frame_start_cb_handle=args[0];
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
	{"register_frame_start_cb", register_frame_start_cb, 1},
	{"led_set_rgb", led_set_rgb, 3}
};

void led_syscalls_init() {
	vm_syscall_add_local_syscalls(led_syscalls, sizeof(led_syscalls)/sizeof(vm_syscall_list_entry_t));
}

void led_syscalls_clear() {
	led_cb_handle=-1;
	frame_start_cb_handle=-1;
}

int led_syscalls_have_cb() {
	return (led_cb_handle>=0);
}


void led_syscalls_frame_start(lssl_vm_t *vm, vm_error_t *error) {
	if (frame_start_cb_handle==-1) return;
	lssl_vm_run_function(vm, frame_start_cb_handle, 0, NULL, error);
}

void led_syscalls_calculate_led(lssl_vm_t *vm, int32_t led, float time, vm_error_t *error) {
	if (led_cb_handle<0) return;
	int32_t args[2]={led<<16, (time*65536)};
	lssl_vm_run_function(vm, led_cb_handle, 2, args, error);
}

void led_syscalls_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b) {
	*r=cur_led_col[0]>>16;
	*g=cur_led_col[1]>>16;
	*b=cur_led_col[2]>>16;
}
