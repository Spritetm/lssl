#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "insn_buf.h"

struct insn_buf_t {
	insn_t *insns;
	int insns_size;
	int insns_len;
	variable_t *vars;
	int vars_size;
	int vars_len;
	int *block_starts;
	int block_starts_size;
	int block_starts_len;
};

#define INC_SIZE 64


insn_buf_t *insn_buf_create() {
	insn_buf_t *ret=calloc(sizeof(insn_buf_t), 1);
	ret->insns_size=INC_SIZE;
	ret->insns_len=0;
	ret->insns=calloc(sizeof(insn_t), ret->insns_size);
	ret->vars_size=INC_SIZE;
	ret->vars_len=0;
	ret->vars=calloc(sizeof(insn_t), ret->vars_size);
	ret->block_starts_size=INC_SIZE;
	ret->block_starts_len=0;
	ret->block_starts=calloc(sizeof(int), ret->block_starts_size);
}

void insn_buf_add_ins(insn_buf_t *buf, int type, int val) {
	if(buf->insns_len==buf->insns_size) {
		buf->insns_size+=INC_SIZE;
		buf->insns=realloc(buf->insns, buf->insns_size*sizeof(insn_t));
	}
	buf->insns[buf->insns_len].type=type;
	buf->insns[buf->insns_len].val=val;
	buf->insns_len++;
}

int insn_buf_find_var(insn_buf_t *buf, const char *name) {
	for (int i=0; i<buf->vars_len; i++) {
		if (strcmp(name, buf->vars[i].name)==0) {
			return i;
		}
	}
	printf("Eek! Var not defined '%s'\n");
	exit(0);
}


void insn_buf_add_var(insn_buf_t *buf, const char *name, int size) {
	//todo: check for already defined
	if(buf->vars_len==buf->vars_size) {
		buf->vars_size+=INC_SIZE;
		buf->vars=realloc(buf->vars, buf->vars_size*sizeof(variable_t));
	}
	buf->vars[buf->vars_len].name=strdup(name);
	buf->vars[buf->vars_len].size=size;
	buf->vars_len++;
}



#define ARG_NONE 0
#define ARG_INT 1
#define ARG_FR 2
#define ARG_VAR 3

typedef struct {
	const char *op;
	int argtype;
} op_t;

static const op_t ops[]={
	{"ILL", ARG_NONE},
	{"PUSH_I", ARG_INT},
	{"ADD_FR_I", ARG_FR},
	{"RD_VAR", ARG_VAR},
	{"WR_VAR", ARG_VAR},
	{"RD_ARR", ARG_VAR},
	{"WR_ARR", ARG_VAR},
	{"MUL", ARG_NONE},
	{"DIV", ARG_NONE},
	{"ADD", ARG_NONE},
	{"SUB", ARG_NONE},
};

static void dump_insn(int pos, insn_t *insn) {
	//keep in sync with enum in header
	int i=insn->type;
	if (i>sizeof(ops)/sizeof(op_t)) i=0; //ILL
	printf("% 4X\t", pos);
	if (ops[i].argtype==ARG_NONE) {
		printf("%s\n", ops[i].op);
	} else if (ops[i].argtype==ARG_INT) {
		printf("%s %d\n", ops[i].op, insn->val);
	} else if (ops[i].argtype==ARG_FR) {
		printf("%s %f\n", ops[i].op, (insn->val/65536.0));
	} else if (ops[i].argtype==ARG_VAR) {
		//todo
		printf("%s [var]\n", ops[i].op);
	}
}

void insn_buf_dump(insn_buf_t *buf) {
	for (int i=0; i<buf->insns_len; i++) {
		dump_insn(i, &buf->insns[i]);
	}
}



