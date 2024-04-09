#pragma once
#include <stdint.h>

//We use some Deeper C Preprocessor Magic so we can keep the instruction defs and arg
//types in one place, and generate enums/structs/... from that. This keeps them from 
//going out of sync.

//name of arg, bytes in opcode for arg
#define LSSL_ARGTYPES \
	LSSL_ARGTYPE_ENTRY(NONE, 0) \
	LSSL_ARGTYPE_ENTRY(INT, 2) \
	LSSL_ARGTYPE_ENTRY(REAL, 4) \
	LSSL_ARGTYPE_ENTRY(VAR, 2) \
	LSSL_ARGTYPE_ENTRY(ARRIDX, 2) \
	LSSL_ARGTYPE_ENTRY(LABEL, 2) \
	LSSL_ARGTYPE_ENTRY(TARGET, 2) \
	LSSL_ARGTYPE_ENTRY(FUNCTION, 2)

//name of inst, type of arg
#define LSSL_INSTRUCTIONS \
	LSSL_INS_ENTRY(NOP, ARG_NONE) \
	LSSL_INS_ENTRY(PUSH_I, ARG_INT) \
	LSSL_INS_ENTRY(PUSH_R, ARG_REAL) \
	LSSL_INS_ENTRY(RD_VAR, ARG_VAR) \
	LSSL_INS_ENTRY(WR_VAR, ARG_VAR) \
	LSSL_INS_ENTRY(RD_G_VAR, ARG_VAR) \
	LSSL_INS_ENTRY(WR_G_VAR, ARG_VAR) \
	LSSL_INS_ENTRY(RD_ARR, ARG_ARRIDX) \
	LSSL_INS_ENTRY(WR_ARR, ARG_ARRIDX) \
	LSSL_INS_ENTRY(RD_G_ARR, ARG_ARRIDX) \
	LSSL_INS_ENTRY(WR_G_ARR, ARG_ARRIDX) \
	LSSL_INS_ENTRY(MUL, ARG_NONE) \
	LSSL_INS_ENTRY(DIV, ARG_NONE) \
	LSSL_INS_ENTRY(ADD, ARG_NONE) \
	LSSL_INS_ENTRY(SUB, ARG_NONE) \
	LSSL_INS_ENTRY(JMP, ARG_TARGET) \
	LSSL_INS_ENTRY(JNZ, ARG_TARGET) \
	LSSL_INS_ENTRY(JZ, ARG_TARGET) \
	LSSL_INS_ENTRY(VAR, ARG_INT) \
	LSSL_INS_ENTRY(VARI, ARG_NONE) \
	LSSL_INS_ENTRY(ENTER, ARG_INT) \
	LSSL_INS_ENTRY(LEAVE, ARG_INT) \
	LSSL_INS_ENTRY(RETURN, ARG_INT) \
	LSSL_INS_ENTRY(CALL, ARG_FUNCTION) \
	LSSL_INS_ENTRY(POP, ARG_NONE) \
	LSSL_INS_ENTRY(TEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TNEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TL, ARG_NONE) \
	LSSL_INS_ENTRY(TG, ARG_NONE) \
	LSSL_INS_ENTRY(TLEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TGEQ, ARG_NONE)

//name of syscall, no of params
//note: maybe rework args to indicate type?
#define LSSL_SYSCALLS \
	LSSL_SYSCALL_ENTRY(abs, 1) \
	LSSL_SYSCALL_ENTRY(floor, 1) \
	LSSL_SYSCALL_ENTRY(ceil, 1) \
	LSSL_SYSCALL_ENTRY(clamp, 2) \
	LSSL_SYSCALL_ENTRY(sin, 1) \
	LSSL_SYSCALL_ENTRY(cos, 1) \
	LSSL_SYSCALL_ENTRY(tan, 1) \
	LSSL_SYSCALL_ENTRY(mod, 2) \
	LSSL_SYSCALL_ENTRY(rand, 3)


#define LSSL_ARGTYPE_ENTRY(arg, argbytes) ARG_##arg,
enum lssl_argtype_enum {
	LSSL_ARGTYPES
};
#undef LSSL_ARGTYPE_ENTRY


#define LSSL_INS_ENTRY(ins, argtype) INSN_##ins,
enum lssl_instr_enum {
	LSSL_INSTRUCTIONS
};
#undef LSSL_INS_ENTRY

#define LSSL_SYSCALL_ENTRY(name, paramcount) SYSCALL_##name,
enum lssl_syscall_enum {
	LSSL_SYSCALLS
};
#undef LSSL_SYSCALL_ENTRY



typedef struct {
	const char *op;
	uint8_t argtype;
} lssl_vm_op_t;

typedef struct {
	uint8_t byte_size;
} lssl_vm_argtype_t;

typedef struct {
	const char *name;
	int params;
} lssl_vm_syscall_t;


//defined in vm_defs.c
extern const lssl_vm_op_t lssl_vm_ops[];
extern const lssl_vm_argtype_t lssl_vm_argtypes[];
extern const lssl_vm_syscall_t lssl_vm_syscalls[];

