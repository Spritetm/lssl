#include <stdint.h>

typedef int32_t (vm_syscall_fn_t)(int32_t *args, int argct);

typedef struct {
	const char *name;
	vm_syscall_fn_t *fn;
	int argct;
} vm_syscall_list_entry_t;


void vm_syscall_add_local_syscalls(const vm_syscall_list_entry_t *syscalls, int count);
int32_t vm_syscall(int syscall, int32_t *arg, int argct);
int vm_syscall_handle_for_name(const char *name);
int vm_syscall_arg_count(int handle);
const char *vm_syscall_name(int handle);
