#pragma once
#include <stdint.h>

//Increase this if you mess with instructions, argtypes  or syscalls
//It makes previous binaries incompatible with the VM, forcing the user
//to do a recompile.
#define LSSL_VM_VER 1


/*
How Does The VM Work?
The VM has two memory spaces: the program and the stack. The program is loaded
from the compiled code and cannot be changed (=ROM). The stack is an array of
32-bit R/W memory that works as, well, a stack. It grows upward, that is, the
first byte saved is at position 0, the 2nd at position 1 etc.

Generally, this thing behaves as you'd expect from a stack-based VM: instructions
pop their operands from the stack, do stuff with it, and push the result back 
to the stack.

Specifically, there are three stack pointers:
- SP, the Stack Pointer. Always points to the top of the stack; any RAM above it
  can be seen as having undefined data.
- BP, the Base Pointer. This is the SP when a function call starts. It provides
  a handy way to get to local POD variables and the pointers to local objects: 
  these are stored directly above the base pointer. It can also be used to access
  function arguments: these are at a fixed position under the BP.
- AP, the Allocation Pointer. At the entry of each function block, this gets
  set to SP. Any local allocation of objects then happens by increasing SP
  by the needed amount and saving the address in the local variable pointer
  as referenced by BP. At the end of the function block, SP is restored to AP,
  immediately clearing all space allocated to the local objects.
*/


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
	LSSL_INS_ENTRY(MOD, ARG_NONE, "Pop two values and push the modulus") \
	LSSL_INS_ENTRY(ADD, ARG_NONE, "Pop two values and push the added result") \
	LSSL_INS_ENTRY(SUB, ARG_NONE, "Pop two values and push the subtracted result") \
	LSSL_INS_ENTRY(LAND, ARG_NONE, "Pop two values and push the logical and-ed result") \
	LSSL_INS_ENTRY(LOR, ARG_NONE, "Pop two values and push the logical or-ed result") \
	LSSL_INS_ENTRY(BAND, ARG_NONE, "Pop two values and push the binary and-ed result") \
	LSSL_INS_ENTRY(BOR, ARG_NONE, "Pop two values and push the binary or-ed result") \
	LSSL_INS_ENTRY(BXOR, ARG_NONE, "Pop two values and push the binary xor-ed result") \
	LSSL_INS_ENTRY(BNOT, ARG_NONE, "Pop value and push the bit inverted result") \
	LSSL_INS_ENTRY(LNOT, ARG_NONE, "Pop value and push the logical inverse result") \
	LSSL_INS_ENTRY(JMP, ARG_TARGET, "Jump to the argument") \
	LSSL_INS_ENTRY(JNZ, ARG_TARGET, "Pop value, jump to the argument if it's non-zero") \
	LSSL_INS_ENTRY(JZ, ARG_TARGET, "Pop value, jump to the argument if it's zero") \
	LSSL_INS_ENTRY(ENTER, ARG_INT, "Increase sp by argument to make space for local vars") \
	LSSL_INS_ENTRY(RETURN, ARG_INT, "Pop value, restore sp to bp, pop return address, bp and ap, " \
			"subtract arg from sp (to account for function args), push value") \
	LSSL_INS_ENTRY(CALL, ARG_FUNCTION, "Push ap, bp and pc, set bp=sp, jump to arg") \
	LSSL_INS_ENTRY(POP, ARG_NONE, "Pop value and discard") \
	LSSL_INS_ENTRY(TEQ, ARG_NONE, "Pop two values, push 1 if equal, 0 otherwise") \
	LSSL_INS_ENTRY(TNEQ, ARG_NONE, "Pop two values, push 1 if not equal, 0 otherwise") \
	LSSL_INS_ENTRY(TL, ARG_NONE, "Pop two values, push 1 if 1st is less, 0 otherwise") \
	LSSL_INS_ENTRY(TG, ARG_NONE, "Pop two values, push 1 if 1st is greater, 0 otherwise") \
	LSSL_INS_ENTRY(TLEQ, ARG_NONE, "Pop two values, push 1 if 1st is less or equal, 0 otherwise") \
	LSSL_INS_ENTRY(TGEQ, ARG_NONE, "Pop two values, push 1 if 1st is greater or equal, 0 otherwise") \
	LSSL_INS_ENTRY(SYSCALL, ARG_INT, "Perform a syscall. Syscall no is in lower 12 bit, " \
									"arg count is in upper 4 bit.") \
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
	LSSL_INS_ENTRY(ARRAYINIT, ARG_INT, "Pop addr, pop count, [addr]=[SP, count*arg], SP+=count*arg") \
	LSSL_INS_ENTRY(STRUCTINIT, ARG_INT, "Pop addr, [addr]=[SP, arg], SP+=arg")


typedef enum lssl_argtype_enum lssl_argtype_enum;
#define LSSL_ARGTYPE_ENTRY(arg, argbytes) ARG_##arg,
enum lssl_argtype_enum {
	LSSL_ARGTYPES
};
#undef LSSL_ARGTYPE_ENTRY

typedef enum lssl_insn_enum lssl_insn_enum;
#define LSSL_INS_ENTRY(ins, argtype, desc) INSN_##ins,
enum lssl_insn_enum {
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
