#include "vm_defs.h"


#define STRINGIFY(x) #x
#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)

#define LSSL_INS_ENTRY(ins, argtype) {STRINGIFY(ins), argtype},
const lssl_vm_op_t lssl_vm_ops[]={
	LSSL_INSTRUCTIONS
};
#undef LSSL_INS_ENTRY