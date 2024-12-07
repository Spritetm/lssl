#include <stdint.h>
#include "vm.h"

typedef int32_t (vm_syscall_fn_t)(lssl_vm_t *vm, int32_t *args);

#define LSSL_SYSCALL_FUNCTION(name) static int32_t name(lssl_vm_t *vm, int32_t *arg)

typedef struct {
	const char *name;
	vm_syscall_fn_t *fn;
} vm_syscall_list_entry_t;

//Add a list of local syscalls. Note that the data 'header' and 'syscalls' point to should
//have a lifetime that lasts at least until vm_syscall_free() is called.
void vm_syscall_add_local_syscalls(const char *name, const vm_syscall_list_entry_t *syscalls, 
										int count, const char *header);
int32_t vm_syscall(lssl_vm_t *vm, int syscall, int32_t *arg);
int vm_syscall_handle_for_name(const char *name);
const char *vm_syscall_name(int handle);
void vm_syscall_free();
//returns 0 if the idx is past the end of the list
int vm_syscall_get_info(int idx, const char **name, const char **header);
