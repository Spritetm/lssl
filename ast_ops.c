#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ast.h"
#include "error.h"
#include "vm_syscall.h"
#include "codegen.h"

//Mental node: definition of 'fixup' is finding a position (e.g. in ram) for a symbol and changing
//the instructions to match that.


typedef struct {
	ast_node_t **nodes;
	int node_pos;
	int size;
} ast_sym_list_t;

static void add_sym(ast_sym_list_t *list, ast_node_t *node) {
	if (list->node_pos==list->size) {
		list->size+=1024;
		list->nodes=realloc(list->nodes, list->size*sizeof(ast_node_t*));
	}
	list->nodes[list->node_pos++]=node;
}

static ast_node_t *find_symbol(ast_sym_list_t *list, const char *name) {
	for (int i=0; i<list->node_pos; i++) {
		if (strcmp(list->nodes[i]->name, name)==0) {
			return list->nodes[i];
		}
	}
	return NULL;
}

static void annotate_symbols(ast_node_t *node, ast_sym_list_t *syms) {
	int old_sym_pos=syms->node_pos;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		//Add local vars to list of syms.
		if (n->type==AST_TYPE_DECLARE) {
			add_sym(syms, n);
		} else if (n->type==AST_TYPE_FUNCDEFARG) {
			add_sym(syms, n);
		} else if (n->type==AST_TYPE_VAR || n->type==AST_TYPE_ASSIGN) {
			//Find variable symbol and resolve
			ast_node_t *s=find_symbol(syms, n->name);
			if (!s) {
				panic_error(n, "Undefined var/function %s", n->name);
				return;
			}
			if (s->type==AST_TYPE_FUNCDEF) {
				//Change node type to function
				n->type=AST_TYPE_FUNCPTR;
				n->returns=AST_RETURNS_FUNCTION;
			}
			n->value=s;
		} else if (n->type==AST_TYPE_FUNCCALL) {
			//Find function symbol and resolve.
			int argct=0;
			for (ast_node_t *i=n->children; i!=NULL; i=i->sibling) argct++;

			ast_node_t *s=find_symbol(syms, n->name);
			if (s) {
				if (s->type!=AST_TYPE_FUNCDEF) {
					panic_error(n, "Cannot call non-function %s", n->name);
					return;
				}
				n->value=s;
			} else {
				//Perhaps it's a syscall?
				int callno=vm_syscall_handle_for_name(n->name);
				if (callno<0) {
					panic_error(n, "Undefined var/function %s", n->name);
					return;
				} else {
					//Yep, syscall. Check if arg count matches.
					int needed_args=vm_syscall_arg_count(callno);
					if (argct!=needed_args) {
						panic_error(n, "Syscall %s requires %d args, %d given", n->name, needed_args, argct);
						return;
					}
					//Change node to reflect.
					n->type=AST_TYPE_SYSCALL;
					n->valpos=callno;
					printf("syscall %d\n", callno);
				}
			}
		}
		//Recursively process sub-nodes
		if (n->children) annotate_symbols(n->children, syms);
	}

	//restore old pos, deleting local var defs
	syms->node_pos=old_sym_pos;
}

//Adds a 'return 0' to the end of the function, if none is there.
void ast_ops_add_trailing_return(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			ast_node_t *last=n->children;
			if (last) {
				//Function is not empty. Find last AST node.
				while (last->sibling) last=last->sibling;
				if (last->type!=AST_TYPE_RETURN) {
					//Add 'return 0'
					ast_node_t *r=ast_new_node(AST_TYPE_RETURN, &n->loc);
					//Note '0' is implicit as numberi member of node is zero-initialized.
					ast_add_child(r, ast_new_node(AST_TYPE_INT, &n->loc));
					ast_add_sibling(last, r);
				}
			} else {
				//Empty function. Make 'return 0' the content.
				ast_node_t *r=ast_new_node(AST_TYPE_RETURN, &n->loc);
				ast_add_child(r, ast_new_node(AST_TYPE_INT, &n->loc));
				n->children=r;
			}
		}
	}
}


//Recursively goes through ast to attach symbols
void ast_ops_attach_symbol_defs(ast_node_t *node) {
	ast_sym_list_t *syms=calloc(sizeof(ast_sym_list_t), 1);
	syms->size=1024;
	syms->nodes=calloc(sizeof(ast_node_t*), syms->size);

	//Find global vars and function defs; these should always be accessible.
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF || n->type==AST_TYPE_DECLARE) {
			add_sym(syms, n);
		}
	}

	//Walk the entire program. We add and remove local vars on the fly and add
	//symbol pointers to var references. Note: this means we don't allow var
	//references at the top level.
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->children) annotate_symbols(n->children, syms);
	}
	free(syms->nodes);
	free(syms);
}

static int arg_size_for_fn(ast_node_t *node) {
	int arg_size=0;
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEFARG) {
			arg_size++;
		}
	}
	return arg_size;
}

//Assigns a position to the args nodes. Takes a funcdef node
static void function_number_arguments(ast_node_t *node) {
	assert(node->type==AST_TYPE_FUNCDEF);
	//Find out arg count.
	int arg_size=arg_size_for_fn(node);
	//Number args
	int i=-3-(arg_size-1);
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEFARG) {
			n->valpos=i++;
		}
	}
}


//Assign positions for local vars. 'node' should be the start of a block. Start_pos
//is the pos we should give to the 1st variable we find, etc. Returns the amount
//of stack space this block needs.
static int block_place_locals(ast_node_t * node, int start_pos) {
	//Give position to local vars. Also adds size of locals (but not subblocks) to `pos`.
	int pos=0;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_DECLARE) {
			n->valpos=start_pos+pos++;
		}
	}
	
	//Grab max size of any of the subblocks (and allocate pos for vars there)
	int subblk_max_size=0;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->children) {
			int s=block_place_locals(n->children, pos+start_pos);
			if (subblk_max_size<s) subblk_max_size=s;
		}
	}
	return pos+subblk_max_size;
}

//Figure out and set the offsets of variables and func args
//Returns size of globals.
void ast_ops_var_place(ast_node_t *node) {
	//Number arguments
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			function_number_arguments(n);
		}
	}
	
	//Find locals, that is enumerate over functions and do space allocation there.
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			int stack_space_locals=block_place_locals(n->children, 0);
			//Make and insert node so we can resolve this to an ENTER instruction
			ast_node_t *d=ast_new_node(AST_TYPE_LOCALSIZE, &node->loc);
			d->numberi=stack_space_locals;
			d->sibling=n->children;
			n->children=d;
		}
	}
}

//Replaces instructions like 'ENTER 0' with NOPs
void ast_ops_remove_useless_ops(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_INSN) {
			if (n->insn_type==INSN_ENTER && n->insn_arg==0) {
				n->insn_type=INSN_NOP;
			} else if (n->insn_type==INSN_LEAVE && n->insn_arg==0) {
				n->insn_type=INSN_NOP;
			}
		}
		if (n->children) ast_ops_remove_useless_ops(n->children);
	}
}


static int ast_ops_position_insns_from(ast_node_t *node, int pc) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_INSN) {
			n->valpos=pc;
			pc+=lssl_vm_argtypes[lssl_vm_ops[n->insn_type].argtype].byte_size;
		}
		if (n->children) {
			pc=ast_ops_position_insns_from(n->children, pc);
		}
	}
	return pc;
}

//Assigns an address to all instructions
void ast_ops_position_insns(ast_node_t *node) {
	ast_ops_position_insns_from(node, 0);
}

void fixup_enter_leave_return(ast_node_t *node, int arg_size, int localsize) {
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_INSN) {
			if (n->insn_type==INSN_RETURN) {
				n->insn_arg=arg_size;
			} else if (n->insn_type==INSN_ENTER) {
				n->insn_arg=localsize;
			} else if (n->insn_type==INSN_LEAVE) {
				n->insn_arg=localsize;
			}
		}
		if (n->children) fixup_enter_leave_return(n, arg_size, localsize);
	}
}


//Fixes up enter/leave/return instructions
void ast_ops_fixup_enter_leave_return(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			int arg_size=arg_size_for_fn(n);
			ast_node_t *ls=n->children;
			while (ls && ls->type!=AST_TYPE_LOCALSIZE) ls=ls->sibling;
			int localsize=ls->numberi;
			fixup_enter_leave_return(n, arg_size, localsize);
		}
	}
}


//Figure out and set the offsets for functions and other ops taking a target
void ast_ops_fixup_addrs(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_INSN && n->value) {
			n->insn_arg=n->value->valpos;
		}
		if (n->children) ast_ops_fixup_addrs(n->children);
	}
}

int find_pc_for_fn(ast_node_t *node) {
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_INSN) {
			return n->valpos;
		}
		if (n->children) {
			int r=find_pc_for_fn(n->children);
			if (r>=0) return r;
		}
	}
	return -1;
}

//Assign the funcdef node the address of the first instruction of the function.
//This makes it easier to resolve this later.
void ast_ops_assign_addr_to_fndef_node(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			int r=find_pc_for_fn(n);
			assert(r>=0);
			n->valpos=r;
		}
	}
}

static void fix_parents(ast_node_t *node, ast_node_t *parent) {
	while (node) {
		node->parent=parent;
		if (node->children) {
			fix_parents(node->children, node);
		}
		node=node->sibling;
	}
}

//Parsing and other ops may not set the parent correctly, but they will
//set the children properly. This fixes the parent property.
void ast_ops_fix_parents(ast_node_t *node) {
	fix_parents(node, NULL);
}

#define PROG_INC 1024

typedef struct {
	int len;
	int size;
	uint8_t *data;
} prog_t;

static void add_byte_prog(prog_t *p, uint8_t b) {
	if (p->len==p->size) {
		p->size+=PROG_INC;
		p->data=realloc(p->data, p->size);
	}
	p->data[p->len++]=b;
}

static void add_word_prog(prog_t *p, uint16_t b) {
	add_byte_prog(p, (b)&0xff);
	add_byte_prog(p, (b>>8)&0xff);
}

static void add_long_prog(prog_t *p, uint32_t b) {
	add_word_prog(p, (b)&0xffff);
	add_word_prog(p, (b>>16)&0xffff);
}

static void gen_binary(ast_node_t *node, prog_t *p) {
	for (ast_node_t *i=node; i!=NULL; i=i->sibling) {
		if (i->type==AST_TYPE_INSN) {
			int size=lssl_vm_argtypes[lssl_vm_ops[i->insn_type].argtype].byte_size;
			if (size!=0) {
				add_byte_prog(p, i->insn_type);
				if (size==1) {
					//no args
				} else if (size==2) {
					add_byte_prog(p, i->insn_arg);
				} else if (size==3) {
					add_word_prog(p, i->insn_arg);
				} else if (size==5) {
					add_long_prog(p, i->insn_arg);
				} else {
					assert(0 && "Invalid byte size for insn type");
				}
			}
		}
		if (i->children) gen_binary(i->children, p);
	}
}

static int globals_size(ast_node_t *node) {
	//Find globals first.
	int glob_size=0;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_DECLARE) {
			n->valpos=glob_size;
			glob_size++;
		}
	}
	return glob_size;
}

static int initial_pc(ast_node_t *node, const char *main_fn_name) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF && strcmp(n->name, main_fn_name)==0) {
			return n->valpos;
		}
	}
	printf("Warning: no main function %s found. Using first function defined instead.\n", main_fn_name);
	return 0;
}


uint8_t *ast_ops_gen_binary(ast_node_t *node, int *len) {
	prog_t p={};
	p.size=PROG_INC;
	p.data=malloc(PROG_INC);
	add_long_prog(&p, LSSL_VM_VER);
	add_long_prog(&p, globals_size(node));
	add_long_prog(&p, initial_pc(node, "main"));
	gen_binary(node, &p);
	*len=p.len;
	return p.data;
}

//Calls all ops (except binary generation) in the proper order
void ast_ops_do_compile(ast_node_t *prognode) {
	ast_ops_fix_parents(prognode);
	ast_ops_attach_symbol_defs(prognode);
	ast_ops_fix_parents(prognode);
	ast_ops_add_trailing_return(prognode);
	ast_ops_fix_parents(prognode);
	ast_ops_var_place(prognode);
	codegen(prognode);
	ast_ops_position_insns(prognode);
	ast_ops_fixup_enter_leave_return(prognode);
	ast_ops_remove_useless_ops(prognode);
	ast_ops_assign_addr_to_fndef_node(prognode);
	ast_ops_fixup_addrs(prognode);
	ast_dump(prognode);
}
