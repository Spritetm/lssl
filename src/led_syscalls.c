#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>
#include "vm_syscall.h"
#include "led_syscalls.h"
#include "error.h"
#include "vm.h"

static int32_t led_cb_handle=-1;
static int32_t frame_start_cb_handle=-1;
static int32_t cur_led_col[3];

LSSL_SYSCALL_FUNCTION(syscall_register_led_cb) {
	led_cb_handle=arg[0];
	return 0;
}

LSSL_SYSCALL_FUNCTION(syscall_register_frame_start_cb) {
	frame_start_cb_handle=arg[0];
	return 0;
}


LSSL_SYSCALL_FUNCTION(syscall_led_set_rgb) {
	cur_led_col[0]=arg[0];
	cur_led_col[1]=arg[1];
	cur_led_col[2]=arg[2];
	return 0;
}

static const vm_syscall_list_entry_t led_syscalls[]={
	{"register_led_cb", syscall_register_led_cb, 1},
	{"register_frame_start_cb", syscall_register_frame_start_cb, 1},
	{"led_set_rgb", syscall_led_set_rgb, 3}
};

static const char *led_hdr=
"struct rgb_t {\n"
"	var r;\n"
"	var g;\n"
"	var b;\n"
"}\n"
"\n"
"struct hsv_t {\n"
"	var h;\n"
"	var s;\n"
"	var v;\n"
"}\n";

void led_syscalls_init() {
	vm_syscall_add_local_syscalls("led", led_syscalls, sizeof(led_syscalls)/sizeof(vm_syscall_list_entry_t), led_hdr);
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
	int32_t arg[2]={led<<16, (time*65536)};
	lssl_vm_run_function(vm, led_cb_handle, 2, arg, error);
}

void led_syscalls_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b) {
	*r=cur_led_col[0]>>16;
	*g=cur_led_col[1]>>16;
	*b=cur_led_col[2]>>16;
}
