#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "insn_buf.h"

typedef struct symbol_t symbol_t;
typedef struct insn_block_t insn_block_t;

#define SYM_TYPE_LOCAL 0
#define SYM_TYPE_GLOBAL 1
#define SYM_TYPE_ARG 3
#define SYM_TYPE_FUNCTION 4

struct symbol_t {
	const char *name;
	int size;
	int offset;
	int type;
	insn_t *target; //in case of function
	symbol_t *next;
};

typedef struct src_pos_t src_pos_t;

struct src_pos_t {
	int start;
	int end;
	src_pos_t *next;
	src_pos_t *next_stack;
};

struct insn_t {
	int type;
	int val;
	symbol_t *sym;
	insn_t *target;
	int pos;

	insn_block_t *block;
	insn_t *next;
	insn_t *next_in_block;

	src_pos_t *src_pos;
};

typedef struct pushed_insn_t pushed_insn_t;

struct pushed_insn_t {
	insn_t *insn;
	pushed_insn_t *next;
	pushed_insn_t *next_stack;
};

struct insn_block_t {
	symbol_t *syms;
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
	src_pos_t *src_pos;
	src_pos_t *src_pos_stack;
};

//Note: if we were to properly free all this, we'd add these elements into one
//huge linked list so we can free them from there.

static insn_block_t *insn_block_create(insn_buf_t *buf) {
	insn_block_t *ret=calloc(sizeof(insn_block_t), 1);
	return ret;
}


static symbol_t *symbol_create() {
	symbol_t *sym=calloc(sizeof(symbol_t), 1);
	return sym;
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
	//set source start and end pos
	insn->src_pos=buf->src_pos_stack;
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


//add new var to given block
static symbol_t *new_var(insn_block_t *blk, const char *name, int size) {
	//check already defined on this level
	for (symbol_t *v=blk->syms; v!=NULL; v=v->next) {
		if (strcmp(name, v->name)==0) {
			return NULL;
		}
	}
	//create and insert in ll
	symbol_t *ret=symbol_create();
	ret->next=blk->syms;
	blk->syms=ret;
	//set name and size
	ret->name=strdup(name);
	ret->size=size;
	ret->type=SYM_TYPE_LOCAL;
	//if global, set flag
	if (blk->parent==NULL) ret->type=SYM_TYPE_GLOBAL;
	return ret;
}

bool insn_buf_add_arg(insn_buf_t *buf, const char *argname) {
	symbol_t *var=new_var(buf->cur_blk, argname, 1);
	if (!var) return false;
	//mark as arg
	var->type=SYM_TYPE_ARG;
	//args have a negative offset, depending on the amount of prev args
	//but we need to know the total number of args, so we'll do fixup for this later
	var->offset=buf->cur_blk->params;
	buf->cur_blk->params++;
	return true;
}


bool insn_buf_add_var(insn_buf_t *buf, const char *name, int size) {
	symbol_t *var=new_var(buf->cur_blk, name, size);
	if (!var) return false;
	return true;
}

static symbol_t *insn_buf_find_var(insn_block_t *blk, const char *name) {
	//See if the var is local
	for (symbol_t *v=blk->syms; v!=NULL; v=v->next) {
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

symbol_t *new_function_sym(insn_buf_t *buf, const char *name) {
	symbol_t *v=symbol_create();
	v->type=SYM_TYPE_FUNCTION;
	v->name=strdup(name);
	//find top-level block
	insn_block_t *blk=buf->cur_blk;
	while (blk->parent!=NULL) blk=blk->parent;
	//add symbol
	v->next=blk->syms;
	blk->syms=v;
	return v;
}

void insn_buf_add_ins_with_sym(insn_buf_t *buf, int type, const char *symname) {
	symbol_t *v=insn_buf_find_var(buf->cur_blk, symname);
	if (!v) {
		//Could be a function pointer.
		//Function may be defined later; create incomplete symbol for it already.
		//Note that forward defs are only allowed on top level (no function defines
		//inside of functions) so we already know we need to add this to the top
		//block.
		v=new_function_sym(buf, symname);
	}
	insn_t *insn=new_insn(buf);
	insn->type=type;
	insn->sym=v;
}

bool insn_buf_add_function(insn_buf_t *buf, insn_t *insn, const char *name) {
	symbol_t *v=insn_buf_find_var(buf->cur_blk, name);
	if (!v) {
		v=new_function_sym(buf, name);
	} else {
		if (v->type!=SYM_TYPE_FUNCTION) {
			return false;
		}
	}
	v->target=insn;
	return true;
}


//Does as the name says. Returns amount of stack space needed for all local
//variables. Also sets the offset for function vars.
static int find_local_var_size(insn_block_t *blk, int base_pos) {
	int space_base=0;
	if (blk->syms) {
		for (symbol_t *v=blk->syms; v!=NULL; v=v->next) {
			if (v->type==SYM_TYPE_LOCAL) {
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
	if (blk->syms) {
		for (symbol_t *v=blk->syms; v!=NULL; v=v->next) {
			if (v->type==SYM_TYPE_ARG) {
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


	//Decide final position for each ins. Note that if we do nop removal, we should
	//do it before here.
	int pc=0;
	for (insn_t *i=buf->first_insn; i!=NULL; i=i->next) {
		i->pos=pc;
		pc+=lssl_vm_argtypes[lssl_vm_ops[i->type].argtype].byte_size+1;
	}

	//Collect and fixup global variables.
	int bp_offset=0;
	for (symbol_t *v=top->syms; v!=NULL; v=v->next) {
		assert(v->type!=SYM_TYPE_LOCAL && v->type!=SYM_TYPE_ARG);
		if (v->type==SYM_TYPE_GLOBAL) {
			v->offset=bp_offset;
			bp_offset+=v->size;
		}
	}

	//Fixup function symbols. Note we only do this in the top block.
	for (symbol_t *v=top->syms; v!=NULL; v=v->next) {
		if (v->type==SYM_TYPE_FUNCTION) {
			if (v->target == NULL) {
				printf("Error! Undefined variable %s\n", v->name);
				exit(1);
			}
			v->offset=v->target->pos;
		}
	}

	for (insn_block_t *blk=top->child; blk!=NULL; blk=blk->sibling) {
		//Fill in var_space members of function
		blk->var_space=find_local_var_size(blk, 0);
		//fixup args
		correct_arg_pos(blk);
	}

	//Fixup enter/leave/return instructions
	//Also convert global variable load instructions.
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
		} else if (i->type==INSN_RD_VAR) {
			if (i->sym->type==SYM_TYPE_GLOBAL) i->type=INSN_RD_G_VAR;
		} else if (i->type==INSN_WR_VAR) {
			if (i->sym->type==SYM_TYPE_GLOBAL) i->type=INSN_WR_G_VAR;
		}
	}

	//Fixup targets for e.g. jump instructions
	for (insn_t *i=buf->first_insn; i!=NULL; i=i->next) {
		if (i->target) {
			i->val=i->target->pos;
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

//Note: this should point at the (nonexisting yet) *next* insn to be added.
//If this is needed, we preliminarily allocate it and the new_insn function
//returns this rather than allocate a new one.
insn_t *insn_buf_next_insn(insn_buf_t *buf) {
	insn_t *insn;
	if (buf->blank_insn) {
		//Hm, already did that. Use prev.
		insn=buf->blank_insn;
	} else {
		insn=calloc(sizeof(insn_t), 1);
		buf->blank_insn=insn;
	}
	return insn;
}

void insn_buf_push_last_insn_pos(insn_buf_t *buf) {
	push_insn(buf->cur_blk, buf->cur_insn);
}

void insn_buf_push_cur_insn_pos(insn_buf_t *buf) {
	push_insn(buf->cur_blk, insn_buf_next_insn(buf));
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

void insn_buf_change_insn_tgt(insn_buf_t *buf, insn_t *insn, insn_t *tgt) {
	insn->target=tgt;
}

static void dump_insn(insn_t *insn) {
	//keep in sync with enum in header
	int i=insn->type;
	printf("%04X\t", insn->pos);
	if (lssl_vm_ops[i].argtype==ARG_NONE) {
		printf("%s\n", lssl_vm_ops[i].op);
	} else if (lssl_vm_ops[i].argtype==ARG_INT) {
		printf("%s %d\n", lssl_vm_ops[i].op, insn->val);
	} else if (lssl_vm_ops[i].argtype==ARG_REAL) {
		printf("%s %f\n", lssl_vm_ops[i].op, (insn->val/65536.0));
	} else if (lssl_vm_ops[i].argtype==ARG_VAR) {
		printf("%s [%d] ; %s\n", lssl_vm_ops[i].op, insn->sym->offset, insn->sym->name);
	} else if (lssl_vm_ops[i].argtype==ARG_LABEL) {
		printf("%s %d\n", lssl_vm_ops[i].op, insn->val);
	} else if (lssl_vm_ops[i].argtype==ARG_TARGET) {
		printf("%s 0x%X\n", lssl_vm_ops[i].op, insn->val);
	} else if (lssl_vm_ops[i].argtype==ARG_FUNCTION) {
		printf("%s [%d] ; %s\n", lssl_vm_ops[i].op, insn->val, insn->sym->name);
	}
}


void insn_buf_dump(insn_buf_t *buf, const char *srctxt) {
	char sbuf[256];
	src_pos_t *spos=NULL;
	for (insn_t *i=buf->first_insn; i!=NULL; i=i->next) {
		if (srctxt && i->src_pos != spos) {
			//copy relevant source text to buffer and print
			memset(sbuf, 0, sizeof(sbuf));
			int l=i->src_pos->end - i->src_pos->start+1;
			if (l>=sizeof(sbuf)-1) l=sizeof(sbuf)-1;
			memcpy(sbuf, &srctxt[i->src_pos->start], l);
			//eat up ending newlines
			while (sbuf[strlen(sbuf)-1]=='\n') sbuf[strlen(sbuf)-1]=0;
			printf(" ; %s\n", sbuf);
			//ignore same srcpos for next insns
			spos=i->src_pos;
		}
		dump_insn(i);
	}
}

void insn_buf_end_src_pos(insn_buf_t *buf, int start, int end) {
	buf->src_pos_stack->start=start;
	buf->src_pos_stack->end=end;
	//remove from stack
	buf->src_pos_stack=buf->src_pos_stack->next_stack;
}

void insn_buf_new_src_pos(insn_buf_t *buf) {
	src_pos_t *pos=calloc(sizeof(src_pos_t), 1);
	//Add to ll & stack
	pos->next=buf->src_pos;
	pos->next_stack=buf->src_pos_stack;
	buf->src_pos=pos;
	buf->src_pos_stack=pos;
}


/*
What do I need to export?
*** Needed ***
- Version. Specifically if we're gonna do an enum for syscalls
- Code. Duh.
*** Debug info ***

*/




