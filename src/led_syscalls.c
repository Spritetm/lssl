#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "vm_syscall.h"
#include "led_syscalls.h"
#include "error.h"
#include "vm.h"
#include "led_map.h"

static int32_t led_cb_handle=-1;
static int32_t led_mapped_cb_handle=-1;
static int32_t frame_start_cb_handle=-1;
static int32_t cur_led_col[4];

LSSL_SYSCALL_FUNCTION(syscall_register_led_cb) {
	led_cb_handle=arg[0];
	return 0;
}

LSSL_SYSCALL_FUNCTION(syscall_register_led_mapped_cb) {
	led_mapped_cb_handle=arg[0];
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
	cur_led_col[3]=0;
	return 0;
}

LSSL_SYSCALL_FUNCTION(syscall_led_set_rgbw) {
	cur_led_col[0]=arg[0];
	cur_led_col[1]=arg[1];
	cur_led_col[2]=arg[2];
	cur_led_col[3]=arg[3];
	return 0;
}

static const vm_syscall_list_entry_t led_syscalls[]={
	{"register_led_cb", syscall_register_led_cb},
	{"register_led_mapped_cb", syscall_register_led_mapped_cb},
	{"register_frame_start_cb", syscall_register_frame_start_cb},
	{"led_set_rgb", syscall_led_set_rgb},
	{"led_set_rgbw", syscall_led_set_rgbw}
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
"}\n"
"struct mapped_pos_t {\n"
"	var x;\n"
"	var y;\n"
"	var z;\n"
"}\n"
"syscalldef register_led_cb(cb(pos, time));\n"
"syscalldef register_led_mapped_cb(cb(mapped_pos_t pos, time));\n"
"syscalldef register_frame_start_cb(cb());\n"
"syscalldef led_set_rgb(r, g, b);\n"
"syscalldef led_set_rgbw(r, g, b, w);\n";

void led_syscalls_init() {
	vm_syscall_add_local_syscalls("led", led_syscalls, sizeof(led_syscalls)/sizeof(vm_syscall_list_entry_t), led_hdr);
}

void led_syscalls_clear() {
	led_cb_handle=-1;
	frame_start_cb_handle=-1;
}

int led_syscalls_have_cb() {
	return (led_cb_handle>=0) || (led_mapped_cb_handle>=0);
}

void led_syscalls_frame_start(lssl_vm_t *vm, vm_error_t *error) {
	if (frame_start_cb_handle==-1) return;
	lssl_vm_run_function(vm, frame_start_cb_handle, 0, NULL, error);
}

void led_syscalls_calculate_led(lssl_vm_t *vm, int32_t led, float time, vm_error_t *error) {
	if (led_cb_handle>=0) {
		int32_t arg[2]={led<<16, (time*65536)};
		lssl_vm_run_function(vm, led_cb_handle, 2, arg, error);
	} else if (led_mapped_cb_handle>=0) {
		int32_t *pos=lssl_vm_alloc_data(vm, 3);
		pos[0]=led*65536;
		pos[1]=0;
		pos[2]=0;
		led_map_get_led_pos_fixed(led, pos);
		int32_t arg[2]={lssl_vm_data_addr(vm, pos), (time*65536)};
		lssl_vm_run_function(vm, led_mapped_cb_handle, 2, arg, error);
	}
}

void led_syscalls_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b) {
	*r=cur_led_col[0]>>16;
	*g=cur_led_col[1]>>16;
	*b=cur_led_col[2]>>16;
}

void led_syscalls_get_rgbw(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *w) {
	*r=cur_led_col[0]>>16;
	*g=cur_led_col[1]>>16;
	*b=cur_led_col[2]>>16;
	*w=cur_led_col[3]>>16;
}

