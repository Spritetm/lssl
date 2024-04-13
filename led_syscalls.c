#include <stddef.h>
#include "vm_syscall.h"

static const vm_syscall_list_entry_t led_syscalls[]={
	{"register_led_cb", NULL, 1},
	{"led_set_rgb", NULL, 4}
};

void led_syscalls_init() {
	vm_syscall_add_local_syscalls(led_syscalls, sizeof(led_syscalls)/sizeof(vm_syscall_list_entry_t));
}

