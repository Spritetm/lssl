#pragma once
#include <stdint.h>

typedef enum {
	LSSL_VM_ERR_NONE=0,
	LSSL_VM_ERR_STACK_OVF,
	LSSL_VM_ERR_STACK_UDF,
	LSSL_VM_ERR_UNK_OP,
	LSSL_VM_ERR_ARRAY_OOB,
	LSSL_VM_ERR_DIVZERO,
	LSSL_VM_ERR_INTERNAL
} vm_error_en;

typedef struct {
	vm_error_en type;
	int32_t pc;
} vm_error_t;

static inline const char *vm_err_to_str(vm_error_en error) {
	const char *erstr[]={"none", "stack overflow", "stack underflow", 
			"unknown opcode", "array out of bounds", "divide by zero", 
			"internal error"};
	if (error<0 || error>=(sizeof(erstr)/sizeof(erstr[0]))) return "unknown error?";
	return erstr[error];
}

typedef struct lssl_vm_t lssl_vm_t;

//Initialize a VM with a program. Returns a handle to the VM.
lssl_vm_t *lssl_vm_init(uint8_t *program, int prog_len, int stack_size_words);

//Run a function using a function handle.
int32_t lssl_vm_run_function(lssl_vm_t *vm, uint32_t fn_handle, int argc, int32_t *argv, vm_error_t *error);

//Run the main function of a program
int32_t lssl_vm_run_main(lssl_vm_t *vm, vm_error_t *error);

//Free the VM.
void lssl_vm_free(lssl_vm_t *vm);

//Dump the VM stack out to stdout.
void lssl_vm_dump_stack(lssl_vm_t *vm);

//Allocate space for item_ct int32_t items on the stack
int32_t *lssl_vm_alloc_data(lssl_vm_t *vm, int item_ct);

//Get VM address for 'real' address
int32_t lssl_vm_data_addr(lssl_vm_t *vm, int32_t *ptr);

//Free earlier allocated space
void lssl_vm_free_data(lssl_vm_t *vm, int32_t *data);


