#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "insn_buf.h"

/*
We can use one stack.
Note that statements use the stack, but they will *always* pop as much as they
push. This means that when entering a block { }, the stack pointer *always* will
be what it is at the start of the function.

After we parsed a function, we can know what the max of local variables is, and
in the function entry, we can reserve that amount of space on the stack. We can then
use relative addressing wrt a base pointer to address the local variables.

So in other words:

CALL fn:
 - push all arguments
 - push BP
 - push IP
 - jmp subroutine

subroutine:
ENTER xx 
	- shifts SP up by XX

RET xx
	- pops retval from stack
	- shifts SP down by XX
	- pop IP
	- pop BP
	- pushes retval to stack


var fixup:
	- get total amount of vars so we can adjust the ENTER and RET instructions
	- give each var a position, relative to BP
	- go through all the blocks, convert var pointer into position


*/



typedef struct {
	const char *name;
	int size;
	int offset;
	int is_global;
	int is_arg;
} variable_t;

typedef struct insn_block_t insn_block_t;

typedef struct {
	int type;
	int val;
	variable_t *var;
	insn_block_t *block;
	char *label;
} insn_t;


struct insn_block_t {
	insn_t *insns;
	char *name;
	int insns_size;
	int insns_len;
	variable_t *vars;
	int vars_size;
	int vars_len;
	insn_block_t *parent;
	int saved_addr[10];
};


struct insn_buf_t {
	insn_block_t *cur;
};

#define INC_SIZE 64


static insn_block_t *insn_block_create() {
	insn_block_t *ret=calloc(sizeof(insn_block_t), 1);
	ret->insns_size=INC_SIZE;
	ret->insns_len=0;
	ret->insns=calloc(sizeof(insn_t), ret->insns_size);
	ret->vars_size=INC_SIZE;
	ret->vars_len=0;
	ret->vars=calloc(sizeof(insn_t), ret->vars_size);
	return ret;
}

insn_buf_t *insn_buf_create() {
	insn_buf_t *ret=calloc(sizeof(insn_buf_t), 1);
	//create default block
	ret->cur=insn_block_create();
	return ret;
}

//return new inst pos in given block
//expand instruction array and fill with zeroes if needed
static insn_t *new_insn(insn_block_t *blk) {
	if(blk->insns_len==blk->insns_size) {
		blk->insns_size+=INC_SIZE;
		blk->insns=realloc(blk->insns, blk->insns_size*sizeof(insn_t));
	}
	insn_t *ret=&blk->insns[blk->insns_len];
	blk->insns_len++;
	memset(ret, 0, sizeof(insn_t));
	return ret;
}

void insn_buf_start_block(insn_buf_t *buf) {
	printf("blk start\n");
	insn_block_t *block=insn_block_create();
	//insert pseudo-instruction to indicate sub-block
	insn_t *blkinst=new_insn(buf->cur);
	blkinst->block=block;
	//swap current block
	block->parent=buf->cur;
	buf->cur=block;
}


void insn_buf_end_block(insn_buf_t *buf) {
	printf("blk end\n");
	insn_block_t *old=buf->cur->parent;
	buf->cur=old;
}

void insn_buf_name_block(insn_buf_t *buf, const char *name) {
	buf->cur->name=strdup(name);
}

static variable_t *new_var(insn_block_t *blk, const char *name) {
	//check already defined on this level
	for (int i=0; i<blk->vars_len; i++) {
		if (strcmp(name, blk->vars[i].name)==0) {
			return NULL;
		}
	}
	//expand array if needed
	if (blk->vars_len==blk->vars_size) {
		blk->vars_size+=INC_SIZE;
		blk->vars=realloc(blk->vars, blk->vars_size*sizeof(variable_t));
	}
	variable_t *ret=&blk->vars[blk->vars_len];
	blk->vars_len++;
	memset(ret, 0, sizeof(variable_t));
	ret->name=strdup(name);
	return ret;
}


bool insn_buf_add_arg(insn_buf_t *buf, const char *argname) {
	//We use a trick here: as the arguments are added on an otherwise empty buffer,
	//we can assume any variable is an argument that is added earlier.
	int pos=buf->cur->vars_len;
	variable_t *var=new_var(buf->cur, argname);
	if (!var) return false;
	var->size=1;
	var->is_arg=1;
	var->offset=-pos-2;
	return true;
}


void insn_buf_change_ins(insn_buf_t *buf, int idx, int type, int val) {
	assert(idx>=0 && idx<buf->cur->insns_len);
	buf->cur->insns[idx].type=type;
	buf->cur->insns[idx].val=val;
	buf->cur->insns[idx].var=NULL;
}


static variable_t *insn_buf_find_var(insn_block_t *blk, const char *name) {
	//See if the var is local
	for (int i=0; i<blk->vars_len; i++) {
		if (strcmp(name, blk->vars[i].name)==0) {
			return &blk->vars[i];
		}
	}
	if (blk->parent) {
		//See if parent has the var
		return insn_buf_find_var(blk->parent, name);
	}
	return NULL;
}

void insn_buf_add_ins(insn_buf_t *buf, int type, int val) {
	insn_t *insn=new_insn(buf->cur);
	insn->type=type;
	insn->val=val;
}

//Function without 'return' returns 0 at the end implicitly.
void insn_buf_add_return_if_needed(insn_buf_t *buf) {
	if (buf->cur->insns_len>0 && buf->cur->insns[buf->cur->insns_len].type!=INSN_RETURN) {
		insn_buf_add_ins(buf, INSN_PUSH_I, 0);
		insn_buf_add_ins(buf, INSN_LEAVE, 0);
		insn_buf_add_ins(buf, INSN_RETURN, 0);
	}
}

bool insn_buf_add_ins_with_var(insn_buf_t *buf, int type, const char *varname) {
	variable_t *v=insn_buf_find_var(buf->cur, varname);
	if (!v) return false;
	insn_t *insn=new_insn(buf->cur);
	insn->type=type;
	insn->val=0;
	insn->var=v;
	return true;
}

void insn_buf_add_ins_with_label(insn_buf_t *buf, int type, const char *label) {
	insn_t *insn=new_insn(buf->cur);
	insn->type=type;
	insn->label=strdup(label);
}

int insn_buf_get_cur_ip(insn_buf_t *buf) {
	return buf->cur->insns_len;
}


const char *insn_buf_var_get_name(insn_buf_t *buf, int idx) {
	if (idx<0 || idx>=buf->cur->vars_len) return "ERROR";
	return buf->cur->vars[idx].name;
}


bool insn_buf_add_var(insn_buf_t *buf, const char *name, int size) {
	variable_t *var=new_var(buf->cur, name);
	if (!var) return false;
	var->size=size;
	var->is_global=(buf->cur->parent==NULL);
	return true;
}

void insn_buf_save_addr(insn_buf_t *buf, int idx, int addr) {
	buf->cur->saved_addr[idx]=addr;
}

int insn_buf_get_saved_addr(insn_buf_t *buf, int idx) {
	return buf->cur->saved_addr[idx];
}

//Does as the name says. Returns amount of stack space needed for all local
//variables.
static int fix_vars_to_pos_on_stack(insn_block_t *blk, int base_pos) {
	int space_base=0;
	for (int i=0; i<blk->vars_len; i++) {
		if (!blk->vars[i].is_arg) {
			blk->vars[i].offset=base_pos+space_base;
			space_base+=blk->vars[i].size;
		}
	}
	int max_space_block=0;
	for (int i=0; i<blk->insns_len; i++) {
		if (blk->insns[i].block) {
			int n=fix_vars_to_pos_on_stack(blk->insns[i].block, base_pos+space_base);
			if (max_space_block<n) max_space_block=n;
		}
	}
	return space_base+max_space_block;
}

static void func_fixup(insn_block_t *blk) {
	//Fixup variables
	int space=fix_vars_to_pos_on_stack(blk, 0);
	//count arguments
	int args=0;
	while (args<blk->vars_len && blk->vars[args].is_arg) args++;
	//Fixup enter/leave/return instructions
	for (int i=0; i<blk->insns_len; i++) {
		if (blk->insns[i].type==INSN_ENTER || blk->insns[i].type==INSN_LEAVE) {
			if (space==0) {
				blk->insns[i].type=INSN_NOP; //not needed; remove
			} else {
				blk->insns[i].val=space;
			}
		} else if (blk->insns[i].type==INSN_RETURN) {
			blk->insns[i].val=args;
		}
	}
	//assign values to all instructions refering labels
	
	
}

int insn_buf_fixup(insn_buf_t *buf) {
	//Should be called on the top-level block.
	//Will find how many local variables are in each function block, find a
	//location for them on the stack, fixup any ENTER and RETURN value in
	//each function.
	//Will also find the global variables and reserve initial space for
	//them (as in, give an initial value for BP)
	//Returns value of BP.

	insn_block_t *top=buf->cur;
	assert(top->parent == NULL);

	//Collect and fixup global variables.
	//note we can't use all the functions used in func_fixup as they will recurse
	//into the sub-functions
	int bp_offset=0;
	for (int i=0; i<top->vars_len; i++) {
		top->vars[i].is_global=1;
		top->vars[i].offset=bp_offset;
		bp_offset+=top->vars[i].size;
	}


	//Fixup all code blocks in the top level.
	for (int i=0; i<top->insns_len; i++) {
		//top block should have no normal opcodes
		//note: should replace by a proper error
		assert(top->insns[i].block && "no code allowed in top level blk");
		func_fixup(top->insns[i].block);
		//todo: make sure all functions end with a return.
	}
	return bp_offset;
}



#define ARG_NONE 0
#define ARG_INT 1
#define ARG_FR 2
#define ARG_VAR 3
#define ARG_LABEL 4
#define ARG_FUNCTION 5

typedef struct {
	const char *op;
	int argtype;
} op_t;

static const op_t ops[]={
	{"NOP", ARG_NONE},
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
	{"JMP", ARG_LABEL},
	{"JNZ", ARG_LABEL},
	{"JZ", ARG_LABEL},
	{"VAR", ARG_INT},
	{"VARI", ARG_NONE},
	{"ENTER", ARG_INT},
	{"LEAVE", ARG_INT},
	{"RETURN", ARG_INT},
	{"CALL", ARG_FUNCTION},
};

static void dump_insn(int pos, insn_t *insn) {
	//keep in sync with enum in header
	int i=insn->type;
	if (i>sizeof(ops)/sizeof(op_t)) i=0; //ILL
	printf("%04X\t", pos);
	if (ops[i].argtype==ARG_NONE) {
		printf("%s\n", ops[i].op);
	} else if (ops[i].argtype==ARG_INT) {
		printf("%s %d\n", ops[i].op, insn->val);
	} else if (ops[i].argtype==ARG_FR) {
		printf("%s %f\n", ops[i].op, (insn->val/65536.0));
	} else if (ops[i].argtype==ARG_VAR) {
		printf("%s [%d] ; %s\n", ops[i].op, insn->var->offset, insn->var->name);
	} else if (ops[i].argtype==ARG_LABEL) {
		printf("%s %d\n", ops[i].op, insn->val);
	} else if (ops[i].argtype==ARG_FUNCTION) {
		printf("%s [%d] ; %s\n", ops[i].op, insn->val, insn->label);
	}
}

int insn_block_dump(insn_block_t *blk, int start_pos) {
	int pos=start_pos;
	for (int i=0; i<blk->insns_len; i++) {
		if (blk->insns[i].block) {
			printf("; block start\n");
			pos+=insn_block_dump(blk->insns[i].block, pos);
			printf("; block end\n");
		} else {
			dump_insn(pos++, &blk->insns[i]);
		}
	}
	return pos-start_pos;
}

void insn_buf_dump(insn_buf_t *buf) {
	insn_block_dump(buf->cur, 0);
}
