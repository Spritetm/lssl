#pragma once

#include <stdbool.h>

/*
VM works as such:

Program runs on a big stack, all instructions are stackbased.
Global variables live at the beginning of the stack. Stack grows
and shrinks to accomodate local vars and return addrs etc
Two registers: IP (Instruction Pointer) and FP (Frame Pointer)

When a function is called, or a block is entered, the stack will
make room for all local variables needed. The local variables will
get an address relative to the stack pointer on entry of the function
(but not of the block!). As the compiler always knows what the state
of the stack pointer is at any given time, it can use a relative-to-SP
address to grab the relevant local variable.

*/

enum ins_enum {
	INSN_NOP=0,
	INSN_PUSH_I,
	INSN_ADD_FR_I,
	INSN_RD_VAR,
	INSN_WR_VAR,
	INSN_RD_ARR,
	INSN_WR_ARR,
	INSN_MUL,
	INSN_DIV,
	INSN_ADD,
	INSN_SUB,
	INSN_JMP,
	INSN_JNZ,
	INSN_JZ,
	INSN_VAR,
	INSN_VARI,
	INSN_ENTER,
	INSN_LEAVE,
	INSN_RETURN,
	INSN_CALL,
	INSN_POP,
};

typedef struct insn_buf_t insn_buf_t;
typedef struct insn_t insn_t;



insn_buf_t *insn_buf_create();
void insn_buf_start_block(insn_buf_t *buf);
void insn_buf_end_block(insn_buf_t *buf);
void insn_buf_name_block(insn_buf_t *buf, const char *name);
bool insn_buf_add_arg(insn_buf_t *buf, const char *argname);
void insn_buf_add_ins(insn_buf_t *buf, int type, int val);
bool insn_buf_add_ins_with_var(insn_buf_t *buf, int type, const char *varname);
void insn_buf_add_ins_with_label(insn_buf_t *buf, int type, const char *label);
void insn_buf_add_return_if_needed(insn_buf_t *buf);
bool insn_buf_add_var(insn_buf_t *buf, const char *name, int size);
void insn_buf_dump(insn_buf_t *buf);
int insn_buf_fixup(insn_buf_t *buf);
void insn_buf_push_last_insn_pos(insn_buf_t *buf);
void insn_buf_push_cur_insn_pos(insn_buf_t *buf);
insn_t *insn_buf_pop_insn(insn_buf_t *buf);
void insn_buf_add_ins_with_tgt(insn_buf_t *buf, int type, insn_t *tgt);
void insn_buf_change_insn_tgt(insn_buf_t *buf, insn_t *insn, insn_t *tgt);
insn_t *insn_buf_next_insn(insn_buf_t *buf);
