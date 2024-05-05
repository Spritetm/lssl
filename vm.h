#pragma once
#include <stdint.h>

typedef enum {
	LSSL_VM_ERR_NONE=0,
	LSSL_VM_ERR_STACK_OVF,
	LSSL_VM_ERR_STACK_UDF,
	LSSL_VM_ERR_UNK_OP,
	LSSL_VM_ERR_ARRAY_OOB
} vm_error_en;

typedef struct {
	vm_error_en type;
	int32_t pc;
} vm_error_t;

static inline const char *vm_err_to_str(vm_error_en error) {
	const char *erstr[]={"none", "stack overflow", "stack underflow", 
			"unknown opcode", "array out of bounds"};
	return erstr[error];
}

typedef struct lssl_vm_t lssl_vm_t;

lssl_vm_t *lssl_vm_init(uint8_t *program, int prog_len, int stack_size_words);
int32_t lssl_vm_run_function(lssl_vm_t *vm, uint32_t fn_handle, int argc, int32_t *argv, vm_error_t *error);
int32_t lssl_vm_run_main(lssl_vm_t *vm, vm_error_t *error);
void lssl_vm_free(lssl_vm_t *vm);
void lssl_vm_dump_stack(lssl_vm_t *vm);
