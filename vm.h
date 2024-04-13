#include <stdint.h>

typedef struct lssl_vm_t lssl_vm_t;

lssl_vm_t *lssl_vm_init(uint8_t *program, int prog_len, int stack_size_words);
int32_t lssl_vm_run_function(lssl_vm_t *vm, uint32_t fn_handle, int argc, int32_t *argv, int *error);
void lssl_vm_run_main(lssl_vm_t *vm);
void lssl_vm_free(lssl_vm_t *vm);
