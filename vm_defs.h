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
	LSSL_ARGTYPE_ENTRY(STRUCT, 3) \
	LSSL_ARGTYPE_ENTRY(LABEL, 3) \
	LSSL_ARGTYPE_ENTRY(TARGET, 3) \
	LSSL_ARGTYPE_ENTRY(FUNCTION, 3)

//name of inst, type of arg
#define LSSL_INSTRUCTIONS \
	LSSL_INS_ENTRY(NOP, ARG_NOP, "No operation") \
	LSSL_INS_ENTRY(PUSH_I, ARG_INT, "Push the integer I to the stack as a number") \
	LSSL_INS_ENTRY(PUSH_R, ARG_REAL, "Push the number N to the stack") \
	LSSL_INS_ENTRY(WR_VAR, ARG_NONE, "Pop address & value, write value to address") \
	LSSL_INS_ENTRY(MUL, ARG_NONE, "Pop two values and push the multiplied result") \
	LSSL_INS_ENTRY(DIV, ARG_NONE, "Pop two values and push the divided result") \
	LSSL_INS_ENTRY(ADD, ARG_NONE, "Pop two values and push the added result") \
	LSSL_INS_ENTRY(SUB, ARG_NONE, "Pop two values and push the subtracted result") \
	LSSL_INS_ENTRY(JMP, ARG_TARGET, "Jump to the argument") \
	LSSL_INS_ENTRY(JNZ, ARG_TARGET, "Pop value, jump to the argument if it's non-zero") \
	LSSL_INS_ENTRY(JZ, ARG_TARGET, "Pop value, jump to the argument if it's zero") \
	LSSL_INS_ENTRY(ENTER, ARG_INT, "Increase sp by argument to make space for local vars") \
	LSSL_INS_ENTRY(RETURN, ARG_INT, "Pop value, restore sp to bp, pop return address and bp, push value") \
	LSSL_INS_ENTRY(CALL, ARG_FUNCTION, "Push bp and pc, set bp=sp, jump to arg") \
	LSSL_INS_ENTRY(POP, ARG_NONE, "Pop value and discard") \
	LSSL_INS_ENTRY(TEQ, ARG_NONE, "Pop two values, push 1 if equal, 0 otherwise") \
	LSSL_INS_ENTRY(TNEQ, ARG_NONE, "Pop two values, push 1 if not equal, 0 otherwise") \
	LSSL_INS_ENTRY(TL, ARG_NONE, "Pop two values, push 1 if 1st is less, 0 otherwise") \
	LSSL_INS_ENTRY(TG, ARG_NONE, "Pop two values, push 1 if 1st is greater, 0 otherwise") \
	LSSL_INS_ENTRY(TLEQ, ARG_NONE, "Pop two values, push 1 if 1st is less or equal, 0 otherwise") \
	LSSL_INS_ENTRY(TGEQ, ARG_NONE, "Pop two values, push 1 if 1st is greater or equal, 0 otherwise") \
	LSSL_INS_ENTRY(SYSCALL, ARG_INT, "Perform the arg'th syscall") \
	LSSL_INS_ENTRY(DUP, ARG_NONE, "Pop a value from the stack and push it twice") \
	LSSL_INS_ENTRY(LEA, ARG_VAR, "Push the absolute address of the local var") \
	LSSL_INS_ENTRY(LEA_G, ARG_VAR, "Push the absolute adddress of the global var") \
	LSSL_INS_ENTRY(LDA, ARG_VAR, "Load the address from the local var and push it") \
	LSSL_INS_ENTRY(LDA_G, ARG_VAR, "Load the address from the global var and push it") \
	LSSL_INS_ENTRY(DEREF, ARG_NONE, "Load the address from the local var and push it") \
	LSSL_INS_ENTRY(PRE_ADD, ARG_REAL, "Pop address, load val from addr, add arg, write to addr, push val") \
	LSSL_INS_ENTRY(POST_ADD, ARG_REAL, "Pop address, load val from addr, push val, add arg, write to addr") \
	LSSL_INS_ENTRY(ARRAY_IDX, ARG_INT, "Pop addr, pop idx, addr+=idx*arg, push addr. " \
									"Note that the size of the addr returned equal arg. "\
									"Also throws an error if the addr is out of the range " \
									"of the original addr.") \
	LSSL_INS_ENTRY(STRUCT_IDX, ARG_INT, "Pop addr, pop offset, addr+=offset, push addr" \
									"Note that the size of the addr returned is the arg " \
									"to this function. Also throws an error if the offset " \
									"is outside of the size of the original addr.") \
	LSSL_INS_ENTRY(SCOPE_ENTER, ARG_NONE, "Push AP, set AP=SP") \
	LSSL_INS_ENTRY(SCOPE_LEAVE, ARG_NONE, "Set SP=AP, pop AP") \
	LSSL_INS_ENTRY(ARRAYINIT, ARG_ARRAY, "Push no, push size, [arg]=AP, AP+=no*size") \
	LSSL_INS_ENTRY(STRUCTINIT, ARG_STRUCT, "Push size, [arg]=AP, AP+=size")


typedef enum lssl_argtype_enum lssl_argtype_enum;
#define LSSL_ARGTYPE_ENTRY(arg, argbytes) ARG_##arg,
enum lssl_argtype_enum {
	LSSL_ARGTYPES
};
#undef LSSL_ARGTYPE_ENTRY

typedef enum lssl_instr_enum lssl_instr_enum;
#define LSSL_INS_ENTRY(ins, argtype, desc) INSN_##ins,
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
