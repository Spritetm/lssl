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

typedef struct variable_t variable_t;
typedef struct insn_block_t insn_block_t;

struct variable_t {
	const char *name;
	int size;
	int offset;
	int is_global;
	int is_arg;
	variable_t *next;
};


struct insn_t {
	int type;
	int val;
	variable_t *var;
	char *label;
	insn_t *target;

	insn_block_t *block;
	insn_t *next;
	insn_t *next_in_block;
};

typedef struct pushed_insn_t pushed_insn_t;

struct pushed_insn_t {
	insn_t *insn;
	pushed_insn_t *next;
};

struct insn_block_t {
	variable_t *vars;
	char *name;		//name, if function
	int params;		//parameter count, if function
	int var_space;	//size of local vars

	insn_block_t *parent;
	insn_block_t *child;
	insn_block_t *sibling;
	pushed_insn_t *insn_stack;
};


/*
Logic:
A buffer has a LL of blocks and a list of instructions.
* The instructions link together using their 'next' property to form
  a list in program order.
* Instructions also point to a block, so we can resolve variables properly.
* Blocks link to their parent block, so we can resolve variables recursively.
*/


struct insn_buf_t {
	insn_block_t *cur_blk;
	insn_t *first_insn;
	insn_t *cur_insn;
	insn_t *blank_insn; //if this is not NULL, use this insn rather than allocate a new one.
};

//Note: if we were to properly free all this, we'd add these elements into one
//huge linked list so we can free them from there.

static insn_block_t *insn_block_create(insn_buf_t *buf) {
	insn_block_t *ret=calloc(sizeof(insn_block_t), 1);
	return ret;
}


static variable_t *variable_create() {
	variable_t *var=calloc(sizeof(variable_t), 1);
	return var;
}

insn_buf_t *insn_buf_create() {
	insn_buf_t *ret=calloc(sizeof(insn_buf_t), 1);
	ret->cur_blk=insn_block_create(ret);
	return ret;
}

//return new inst pos in given block
static insn_t *new_insn(insn_buf_t *buf) {
	insn_t *insn;
	if (buf->blank_insn) {
		//pre-allocated insn? Use that.
		insn=buf->blank_insn;
		buf->blank_insn=NULL;
	} else {
		//Allocate insn.
		insn=calloc(sizeof(insn_t), 1);
	}
	//Link into ll.
	if (buf->first_insn) {
		buf->cur_insn->next=insn;
	} else {
		buf->first_insn=insn;
	}
	buf->cur_insn=insn;
	//set block to current
	insn->block=buf->cur_blk;
	return insn;
}

void insn_buf_start_block(insn_buf_t *buf) {
	insn_block_t *block=insn_block_create(buf);
	block->sibling=buf->cur_blk->child;
	buf->cur_blk->child=block;
	block->parent=buf->cur_blk;
	//make current
	buf->cur_blk=block;
}


void insn_buf_end_block(insn_buf_t *buf) {
	buf->cur_blk=buf->cur_blk->parent;
}

void insn_buf_name_block(insn_buf_t *buf, const char *name) {
	buf->cur_blk->name=strdup(name);
}

//add new var to given block
static variable_t *new_var(insn_block_t *blk, const char *name, int size) {
	//check already defined on this level
	for (variable_t *v=blk->vars; v!=NULL; v=v->next) {
		if (strcmp(name, v->name)==0) {
			return NULL;
		}
	}
	//create and insert in ll
	variable_t *ret=variable_create();
	ret->next=blk->vars;
	blk->vars=ret;
	//set name and size
	ret->name=strdup(name);
	ret->size=size;
	//if global, set flag
	if (blk->parent==NULL) ret->is_global=1;
	return ret;
}

bool insn_buf_add_arg(insn_buf_t *buf, const char *argname) {
	variable_t *var=new_var(buf->cur_blk, argname, 1);
	if (!var) return false;
	//mark as arg
	var->is_arg=1;
	//args have a negative offset, depending on the amount of prev args
	//but we need to know the total number of args, so we'll do fixup for this later
	var->offset=buf->cur_blk->params;
	buf->cur_blk->params++;
	return true;
}


bool insn_buf_add_var(insn_buf_t *buf, const char *name, int size) {
	variable_t *var=new_var(buf->cur_blk, name, size);
	if (!var) return false;
	return true;
}

static variable_t *insn_buf_find_var(insn_block_t *blk, const char *name) {
	//See if the var is local
	for (variable_t *v=blk->vars; v!=NULL; v=v->next) {
		if (strcmp(name, v->name)==0) {
			return v;
		}
	}
	if (blk->parent) {
		//See if parent has the var
		return insn_buf_find_var(blk->parent, name);
	}
	return NULL;
}

void insn_buf_add_ins(insn_buf_t *buf, int type, int val) {
	insn_t *insn=new_insn(buf);
	insn->type=type;
	insn->val=val;
}

//Function without 'return' returns 0 at the end implicitly.
void insn_buf_add_return_if_needed(insn_buf_t *buf) {
	if (buf->cur_insn && buf->cur_insn->type!=INSN_RETURN) {
		insn_buf_add_ins(buf, INSN_PUSH_I, 0);
		insn_buf_add_ins(buf, INSN_LEAVE, 0);
		insn_buf_add_ins(buf, INSN_RETURN, 0);
	}
}

bool insn_buf_add_ins_with_var(insn_buf_t *buf, int type, const char *varname) {
	variable_t *v=insn_buf_find_var(buf->cur_blk, varname);
	if (!v) return false;
	insn_t *insn=new_insn(buf);
	insn->type=type;
	insn->var=v;
	return true;
}

void insn_buf_add_ins_with_label(insn_buf_t *buf, int type, const char *label) {
	insn_t *insn=new_insn(buf);
	insn->type=type;
	insn->label=strdup(label);
}


//Does as the name says. Returns amount of stack space needed for all local
//variables.
static int find_local_var_size(insn_block_t *blk, int base_pos) {
	int space_base=0;
	if (blk->vars) {
		for (variable_t *v=blk->vars; v!=NULL; v=v->next) {
			if (!v->is_arg) {
				v->offset=base_pos+space_base;
				space_base+=v->size;
			}
		}
	}
	int max_space_block=0;
	//Go through all child blocks, also recursively
	//figure out the block space there; store the max needed.
	for (insn_block_t *ch=blk->child; ch!=NULL; ch=ch->sibling) {
		int n=find_local_var_size(ch, base_pos+space_base);
		if (max_space_block<n) max_space_block=n;
	}
	return space_base+max_space_block;
}

//The args to functions are stored as positive integers, but the opcode needs
//negative integers relative to the number of parameters. We convert that here.
void correct_arg_pos(insn_block_t *blk) {
	if (blk->vars) {
		for (variable_t *v=blk->vars; v!=NULL; v=v->next) {
			if (v->is_arg) {
				assert(v->offset>=0); //should not be corrected yet
				v->offset=-2-(blk->params-v->offset-1);
			}
		}
	}
}


int insn_buf_fixup(insn_buf_t *buf) {
	//Should be called on the top-level block.
	//Will find how many local variables are in each function block, find a
	//location for them on the stack, fixup any ENTER and RETURN value in
	//each function.
	//Will also find the global variables and reserve initial space for
	//them (as in, give an initial value for BP)
	//Returns value of BP.

	insn_block_t *top=buf->cur_blk;
	assert(top->parent == NULL);

	//Collect and fixup global variables.
	int bp_offset=0;
	for (variable_t *v=top->vars; v!=NULL; v=v->next) {
		assert(v->is_global);
		v->offset=bp_offset;
		bp_offset+=v->size;
	}

	for (insn_block_t *blk=top->child; blk!=NULL; blk=blk->sibling) {
		//Fill in var_space members of function
		blk->var_space=find_local_var_size(blk, 0);
		//fixup args
		correct_arg_pos(blk);
	}


	//Fixup enter/leave/return instructions
	for (insn_t *i=buf->first_insn; i!=NULL; i=i->next) {
		if (i->type==INSN_ENTER || i->type==INSN_LEAVE) {
			//can only appear on top-level functions
			assert(i->block->parent!=NULL && i->block->parent->parent==NULL);
			if (i->block->params==0) {
				i->type=INSN_NOP; //not needed; remove
			} else {
				i->val=i->block->var_space;
			}
		} else if (i->type==INSN_RETURN) {
			i->val=i->block->params;
		}
	}

	return bp_offset;
}


static void push_insn(insn_block_t *blk, insn_t *insn) {
	//create new insn ll entry and insert into stack
	pushed_insn_t *p=calloc(sizeof(pushed_insn_t), 1);
	p->insn=insn;
	p->next=blk->insn_stack;
	blk->insn_stack=p;
}

void insn_buf_push_last_insn_pos(insn_buf_t *buf) {
	push_insn(buf->cur_blk, buf->cur_insn);
}

void insn_buf_push_cur_insn_pos(insn_buf_t *buf) {
	//Pre-allocate next insn.
	insn_t *insn=calloc(sizeof(insn_t), 1);
	buf->blank_insn=insn;
	//Push it
	push_insn(buf->cur_blk, insn);
}

insn_t *insn_buf_pop_insn(insn_buf_t *buf) {
	insn_block_t *blk=buf->cur_blk;
	pushed_insn_t *p=blk->insn_stack;
	assert(p);
	insn_t *ret=p->insn;
	blk->insn_stack=p->next;
	free(p);
	return ret;
}

void insn_buf_add_ins_with_tgt(insn_buf_t *buf, int type, insn_t *tgt) {
	insn_t *insn=new_insn(buf);
	insn->type=type;
	insn->target=tgt;
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


void insn_buf_dump(insn_buf_t *buf) {
	int pos=0;
	for (insn_t *i=buf->first_insn; i!=NULL; i=i->next) {
		dump_insn(pos++, i);
	}
}
