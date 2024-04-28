#pragma once
#include <stdint.h>

//Increase this if you mess with instructions, argtypes  or syscalls
//It makes previous binaries incompatible with the VM, forcing the user
//to do a recompile.
#define LSSL_VM_VER 1

//We use some Deeper C Preprocessor Magic so we can keep the instruction defs and arg
//types in one place, and generate enums/structs/... from that. This keeps them from 
//going out of sync.

//name of arg, bytes in opcode
#define LSSL_ARGTYPES \
	LSSL_ARGTYPE_ENTRY(NOP, 0) \
	LSSL_ARGTYPE_ENTRY(NONE, 1) \
	LSSL_ARGTYPE_ENTRY(INT, 3) \
	LSSL_ARGTYPE_ENTRY(REAL, 5) \
	LSSL_ARGTYPE_ENTRY(VAR, 3) \
	LSSL_ARGTYPE_ENTRY(ARRAY, 3) \
	LSSL_ARGTYPE_ENTRY(LABEL, 3) \
	LSSL_ARGTYPE_ENTRY(TARGET, 3) \
	LSSL_ARGTYPE_ENTRY(FUNCTION, 3)

//name of inst, type of arg
#define LSSL_INSTRUCTIONS \
	LSSL_INS_ENTRY(NOP, ARG_NOP) \
	LSSL_INS_ENTRY(PUSH_I, ARG_INT) \
	LSSL_INS_ENTRY(PUSH_R, ARG_REAL) \
	LSSL_INS_ENTRY(WR_VAR, ARG_NONE) \
	LSSL_INS_ENTRY(MUL, ARG_NONE) \
	LSSL_INS_ENTRY(DIV, ARG_NONE) \
	LSSL_INS_ENTRY(ADD, ARG_NONE) \
	LSSL_INS_ENTRY(SUB, ARG_NONE) \
	LSSL_INS_ENTRY(JMP, ARG_TARGET) \
	LSSL_INS_ENTRY(JNZ, ARG_TARGET) \
	LSSL_INS_ENTRY(JZ, ARG_TARGET) \
	LSSL_INS_ENTRY(ENTER, ARG_INT) \
	LSSL_INS_ENTRY(RETURN, ARG_INT) \
	LSSL_INS_ENTRY(CALL, ARG_FUNCTION) \
	LSSL_INS_ENTRY(POP, ARG_NONE) \
	LSSL_INS_ENTRY(TEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TNEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TL, ARG_NONE) \
	LSSL_INS_ENTRY(TG, ARG_NONE) \
	LSSL_INS_ENTRY(TLEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TGEQ, ARG_NONE) \
	LSSL_INS_ENTRY(SYSCALL, ARG_INT) \
	LSSL_INS_ENTRY(DUP, ARG_NONE) \
	LSSL_INS_ENTRY(ARRAYINIT, ARG_ARRAY) \
	LSSL_INS_ENTRY(ARRAYINIT_G, ARG_ARRAY) \
	LSSL_INS_ENTRY(LEA, ARG_VAR) \
	LSSL_INS_ENTRY(LEA_G, ARG_VAR) \
	LSSL_INS_ENTRY(DEREF, ARG_NONE) \
	LSSL_INS_ENTRY(PRE_ADD, ARG_REAL) \
	LSSL_INS_ENTRY(POST_ADD, ARG_REAL) \
	LSSL_INS_ENTRY(ARRAY_IDX, ARG_INT) \
	LSSL_INS_ENTRY(STRUCT_IDX, ARG_INT)


typedef enum lssl_argtype_enum lssl_argtype_enum;
#define LSSL_ARGTYPE_ENTRY(arg, argbytes) ARG_##arg,
enum lssl_argtype_enum {
	LSSL_ARGTYPES
};
#undef LSSL_ARGTYPE_ENTRY

typedef enum lssl_instr_enum lssl_instr_enum;
#define LSSL_INS_ENTRY(ins, argtype) INSN_##ins,
enum lssl_instr_enum {
	LSSL_INSTRUCTIONS
};
#undef LSSL_INS_ENTRY

typedef struct {
	const char *op;
	uint8_t argtype;
} lssl_vm_op_t;

typedef struct {
	uint8_t byte_size;
} lssl_vm_argtype_t;

//defined in vm_defs.c
extern const lssl_vm_op_t lssl_vm_ops[];
extern const lssl_vm_argtype_t lssl_vm_argtypes[];
