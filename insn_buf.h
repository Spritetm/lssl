#pragma once

enum ins_enum {
	INSN_ILL=0,
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
};

typedef struct {
	const char *name;
	int size;
} variable_t;

typedef struct {
	int type;
	int val;
} insn_t;


typedef struct insn_buf_t insn_buf_t;

insn_buf_t *insn_buf_create();
void insn_buf_add_ins(insn_buf_t *buf, int type, int val);
void insn_buf_add_var(insn_buf_t *buf, const char *name, int size);
int insn_buf_find_var(insn_buf_t *buf, const char *name);
void insn_buf_dump(insn_buf_t *buf);
