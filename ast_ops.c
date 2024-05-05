#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ast.h"
#include "error.h"
#include "vm_syscall.h"
#include "codegen.h"

//Note: definition of 'fixup' is finding a position (e.g. in ram) for a symbol and changing
//the instructions to match that.



//The parser allows an empty array dereference (e.g. a[] rather than a[2]) anywhere, while
//it's only actually valid for the first array deref in an argument in a function declaration.
//The parser inserts these as ARRAYREF_EMPTY nodes. Here we convert them back to ARRAYREF
//if it's in a proper place, or throw an error if not.
void ast_ops_check_empty_array_deref(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_ARRAYREF_EMPTY) {
			//If this is the first deref in a function arg, the parent must be a FUNCDEFARG.
			if (n->parent->type==AST_TYPE_FUNCDEFARG) {
				n->type=AST_TYPE_ARRAYREF;
			} else {
				panic_error(n, "Cannot have empty array deref here.");
			}
		}
		if (n->children) ast_ops_check_empty_array_deref(n->children);
	}
}


//This collates const values, e.g. '(2+4)/3)' gets collated into a single const number '2'.
static void ast_ops_collate_consts_node(ast_node_t *node) {
	//We can only collate things that only have child members that are also CONST.
	int all_const=1;
	int32_t arg[16];
	int argct=0;
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		ast_ops_collate_consts_node(n);
		if (n->returns!=AST_RETURNS_CONST) all_const=0;
		if (argct<16) arg[argct++]=n->number;
	}
	if (all_const) {
		int collate_ok=1;
		if (node->type==AST_TYPE_PLUS && argct==2) {
			node->number=arg[0]+arg[1];
		} else if (node->type==AST_TYPE_MINUS && argct==2) {
			node->number=arg[0]-arg[1];
		} else if (node->type==AST_TYPE_TIMES && argct==2) {
			int64_t v=(int64_t)arg[0]*(int64_t)arg[1];
			node->number=v>>16;
		} else if (node->type==AST_TYPE_DIVIDE && argct==2) {
			node->number=(arg[0]*65536.0)/arg[1];
		} else {
			collate_ok=0;
		}
		if (collate_ok) {
			node->type=AST_TYPE_NUMBER;
			node->children=NULL; //ToDo: free nodes?
		}
	}
}

void ast_ops_collate_consts(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		ast_ops_collate_consts_node(n);
	}
}

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

//This converts nodes that refer to a symbol by 'name', to nodes that also refer to that
//symbols declaration by 'value'.
static void annotate_symbols(ast_node_t *node, ast_sym_list_t *syms, int is_global, int is_block) {
	int old_sym_pos=syms->node_pos;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		//Add local vars to list of syms.
		if (n->type==AST_TYPE_DECLARE) {
			add_sym(syms, n);
		} else if (n->type==AST_TYPE_FUNCDEFARG) {
			add_sym(syms, n);
		} else if (n->type==AST_TYPE_DEREF || 
					n->type==AST_TYPE_REF ||
					n->type==AST_TYPE_POST_ADD ||
					n->type==AST_TYPE_PRE_ADD
					) {
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
		} else if (n->type==AST_TYPE_FUNCCALL || n->type==AST_TYPE_GOTO) {
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
				if (callno<0 || n->type==AST_TYPE_GOTO) {
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
				}
			}
		}
		//Recursively process sub-nodes
		if (n->children) {
			int block=(n->type==AST_TYPE_BLOCK || n->type==AST_TYPE_FUNCDEF);
			annotate_symbols(n->children, syms, 0, block);
		}
	}
	
	if (is_block) {
		//restore old pos, deleting local var defs
		syms->node_pos=old_sym_pos;
	}
}


//Recursively goes through ast to attach symbol definitions
void ast_ops_attach_symbol_defs(ast_node_t *node) {
	ast_sym_list_t *syms=calloc(sizeof(ast_sym_list_t), 1);
	syms->size=1024;
	syms->nodes=calloc(sizeof(ast_node_t*), syms->size);

	//Find global vars and function defs; these should always be accessible, even if the definition
	//is later than where they're called/invoked.
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF || n->type==AST_TYPE_DECLARE) {
			add_sym(syms, n);
		}
	}

	//Walk the entire program. We add and remove local vars on the fly and add
	//symbol pointers to var references.
	annotate_symbols(node, syms, 1, 1);

	free(syms->nodes);
	free(syms);
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
					//Note '0' is implicit as number member of node is zero-initialized.
					ast_add_child(r, ast_new_node(AST_TYPE_NUMBER, &n->loc));
					ast_add_sibling(last, r);
				}
			} else {
				//Empty function. Make 'return 0' the content.
				ast_node_t *r=ast_new_node(AST_TYPE_RETURN, &n->loc);
				ast_add_child(r, ast_new_node(AST_TYPE_NUMBER, &n->loc));
				n->children=r;
			}
		}
	}
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
	int i=-4-(arg_size-1);
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
			n->valpos=start_pos+pos;
			pos+=n->size;
		}
	}
	
	//Grab max size of any of the subblocks (and allocate pos for vars there)
	int subblk_max_size=0;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->children) {
			if (n->type==AST_TYPE_BLOCK) {
				int s=block_place_locals(n->children, pos+start_pos);
				if (subblk_max_size<s) subblk_max_size=s;
			} else {
				int s=block_place_locals(n->children, pos+start_pos);
				pos+=s;
			}
		}
	}
	return pos+subblk_max_size;
}

int var_place_globals(ast_node_t *node, int off) {
	//Find globals
	int glob_pos=off;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_DECLARE) {
			n->valpos=glob_pos;
			glob_pos+=n->size;
		} else if (n->children && n->type!=AST_TYPE_FUNCDEF) {
			glob_pos=var_place_globals(n->children, glob_pos);
		}
	}
	return glob_pos;
}

//Figure out and set the offsets of variables and func args
void ast_ops_var_place(ast_node_t *node) {
	//Number arguments
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			function_number_arguments(n);
		}
	}
	
	//Place globals
	var_place_globals(node, 0);
	
	//Find locals, that is enumerate over functions and do space allocation there.
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			int stack_space_locals=block_place_locals(n->children, 0);
			//Make and insert node so we can resolve this to an ENTER instruction
			ast_node_t *d=ast_new_node(AST_TYPE_LOCALSIZE, &node->loc);
			d->number=stack_space_locals;
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
			}
		}
		if (n->children) ast_ops_remove_useless_ops(n->children);
	}
}


//Associate STRUCTREF to STRUCTDEF node
static void assoc_structrefs(ast_node_t *root, ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_STRUCTREF) {
			//Find associated structdef
			for (ast_node_t *m=root; m!=NULL; m=m->sibling) {
				if (m->type==AST_TYPE_STRUCTDEF && strcmp(m->name, n->name)==0) {
					n->value=m;
					break;
				}
			}
			if (n->value==NULL) {
				panic_error(n, "Undefined struct type %s", n->name);
			}
		}
		if (n->children) assoc_structrefs(root, n->children);
	}
}

void ast_ops_assoc_structrefs(ast_node_t *node) {
	assoc_structrefs(node, node);
}


ast_node_t *find_deref_return_type(ast_node_t *node, ast_node_t *type, int is_first) {
	ast_node_t *n_a=ast_find_type(node->children, AST_TYPE_ARRAYREF);
	ast_node_t *n_s=ast_find_type(node->children, AST_TYPE_STRUCTMEMBER);
	ast_node_t *d_a=ast_find_type(type->children, AST_TYPE_ARRAYREF);
	ast_node_t *d_s=ast_find_type(type->children, AST_TYPE_STRUCTREF);
	if (n_a && !d_a) {
		panic_error(node, "Dereferencing non-array!");
	} else if (n_s && !d_s) {
		panic_error(node, "Trying to get member of non-struct!");
	} else if (!n_a && !n_s) {
		return type;
	} else if (n_a && d_a) {
		return find_deref_return_type(n_a, d_a, 0);
	} else if (n_s && d_s) {
		ast_node_t *d_m=d_s->value->children;
		while (strcmp(d_m->name, n_s->name)!=0 && d_m!=NULL) d_m=d_m->sibling;
		if (d_m==NULL) {
			panic_error(node, "No member %s in struct %s!\n", n_s->name, d_s->value->name);
			return NULL;
		}
		return find_deref_return_type(n_s, d_m, 0);
	}
	return NULL;
}

//Returns a pointer to whatever struct or array or POD the deref eventually returns.
ast_node_t *get_deref_return_type(ast_node_t *node) {
	return find_deref_return_type(node, node->value, 1);
}

int ast_ops_node_is_pod(ast_node_t *node) {
	ast_node_t *n_a=ast_find_type(node->children, AST_TYPE_ARRAYREF);
	ast_node_t *n_s=ast_find_type(node->children, AST_TYPE_STRUCTMEMBER);
	return (!n_a && !n_s);
}

int check_var_type_matches_fn_arg_type(ast_node_t *vardef, ast_node_t *fndef, int initial_array_var) {
	//Matches if both are PODs. Does not match if one is but the other isn't.
	if (ast_ops_node_is_pod(vardef)) {
		return ast_ops_node_is_pod(fndef);
	} else if (ast_ops_node_is_pod(fndef)) {
		return 0;
	}

	//First, catch function pointers. Function pointers are allowed to syscalls,
	//but not to normal functions.

	//Next, walk both variable def and function arg def to see if they're the same
	ast_node_t *n_a=ast_find_type(vardef->children, AST_TYPE_ARRAYREF);
	ast_node_t *f_a=ast_find_type(fndef->children, AST_TYPE_ARRAYREF);
	if (n_a && f_a) {
		if (!initial_array_var) {
			//See if arrays are the same size.
			ast_node_t *n_n=ast_find_type(n_a->children, AST_TYPE_NUMBER);
			ast_node_t *f_n=ast_find_type(f_a->children, AST_TYPE_NUMBER);
			if (n_n->number != f_n->number) return 0;
		}
		//See if array types match.
		return check_var_type_matches_fn_arg_type(n_a, f_a, 0);
	}

	ast_node_t *n_s=ast_find_type(vardef->children, AST_TYPE_STRUCTREF);
	ast_node_t *f_s=ast_find_type(fndef->children, AST_TYPE_STRUCTREF);
	if (n_s && f_s) {
		return n_s->value == f_s->value;
	}

	//No match.
	ast_dump(vardef);
	ast_dump(fndef);
	return 0;
}


void ast_ops_fix_function_args(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCCALL) {
			//iterate over function args
			//note this code is fscked. It needs to iterate over both the derefs in the
			//function call in parallel with the funcdefargs of the function definition
			//and check if the function definition is a POD, and if so change the DEREF
			//to a REF.
			//probably also want to check if function args are the same type...
			ast_node_t *funcdef=n->value;
			ast_node_t *funcdefarg=ast_find_type(funcdef->children, AST_TYPE_FUNCDEFARG);
			ast_node_t *funcarg=ast_find_type(n->children, AST_TYPE_DEREF);
			while (funcarg && funcdefarg) {
				//Check if arguments have the same type.
				//Also, if argument is a POD, we need to modify the DEREF to REF.
				for (ast_node_t *i=n->children; i!=NULL; i=i->sibling) {
					if (i->type==AST_TYPE_DEREF) {
						ast_node_t *d_t=get_deref_return_type(i);
						if (!check_var_type_matches_fn_arg_type(d_t, funcdefarg, 1)) {
							panic_error(n, "Function arg doesn't match type defined by function declaration!");
						}
						if (!ast_ops_node_is_pod(d_t)) {
							i->type=AST_TYPE_REF;
						}
					}
				}
				
				//find next arg
				funcdefarg=ast_find_type(funcdefarg->sibling,  AST_TYPE_FUNCDEFARG);
				funcarg=ast_find_type(funcarg->sibling, AST_TYPE_DEREF);
			}
			if (funcdefarg) {
				panic_error(node, "Not enough arguments to function '%s'", funcdef->name);
			}
			if (funcarg) {
				panic_error(node, "Too many arguments to function '%s'", funcdef->name);
			}
			//Check if one of the arguments is a function pointer.
			if (n->children) {
				ast_node_t *fa=ast_find_type(n->children, AST_TYPE_FUNCPTR);
				if (fa) {
					panic_error(fa, "Function pointer argument to non-syscall function is not allowed.");
				}
			}
		}
		if (n->children) ast_ops_fix_function_args(n->children);
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

void fixup_enter_return(ast_node_t *node, int arg_size, int localsize) {
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_INSN) {
			if (n->insn_type==INSN_RETURN) {
				n->insn_arg=arg_size;
			} else if (n->insn_type==INSN_ENTER) {
				n->insn_arg=localsize;
			}
		}
		if (n->children) fixup_enter_return(n, arg_size, localsize);
	}
}


//Fixes up enter/return instructions
void ast_ops_fixup_enter_return(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			int arg_size=arg_size_for_fn(n);
			ast_node_t *ls=n->children;
			while (ls && ls->type!=AST_TYPE_LOCALSIZE) ls=ls->sibling;
			int localsize=ls->number;
			fixup_enter_return(n, arg_size, localsize);
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
			glob_size+=n->size;
		}
		if (n->type!=AST_TYPE_FUNCDEF) {
			glob_size+=globals_size(n->children);
		}
	}
	return glob_size;
}

//Program should start (after array initializers) with a jump to 'main'.
void ast_ops_add_program_start(ast_node_t *node, const char *main_fn_name) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF && strcmp(n->name, main_fn_name)==0) {
			ast_node_t *d=ast_new_node(AST_TYPE_GOTO, &node->loc);
			d->name=strdup(main_fn_name);
			//need to put this at end of program
			ast_node_t *last=node;
			while (last->sibling) last=last->sibling;
			last->sibling=d;
			return;
		}
	}
	printf("Warning: no function '%s' found. Using first function defined instead.\n", main_fn_name);
}


static int size_of_obj(ast_node_t *node) {
	int ret=0;
	if (!node) return 1;
	if (node->type==AST_TYPE_STRUCTDEF) {
		for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
			if (n->type==AST_TYPE_STRUCTMEMBER) {
				ret+=size_of_obj(n);
			}
		}
	} else if (node->type==AST_TYPE_STRUCTMEMBER) {
		ast_node_t *a=ast_find_type(node->children, AST_TYPE_ARRAYREF);
		if (!a) a=ast_find_type(node, AST_TYPE_STRUCTREF);
		if (a) {
			ret=size_of_obj(a);
		} else {
			ret=1;
		}
	} else if (node->type==AST_TYPE_ARRAYREF) {
		ast_node_t *s=ast_find_type(node->children, AST_TYPE_NUMBER);
		if (!s) {
			panic_error(node, "Non-toplevel array size must be const");
			return 0;
		}
		int count=s->number>>16;
		ast_node_t *a=ast_find_type(node->children, AST_TYPE_ARRAYREF);
		if (!a) a=ast_find_type(node->children, AST_TYPE_STRUCTREF);
		if (a) {
			ret=count*size_of_obj(a);
		} else {
			ret=count;
		}
	} else if (node->type==AST_TYPE_STRUCTREF) {
		ret=size_of_obj(node->value); //the structdef
	} else {
		ret=1; //probably POD
	}
	return ret;
}


//Also annotates return type
void ast_ops_annotate_obj_decl_size(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_ARRAYREF) {
			//This also takes array indices in ref/deref nodes, but we'll
			//overwrite that later anyway.
			n->parent->returns=AST_RETURNS_ARRAY;
			//this will get overwritten when we search deeper, if needed
			n->returns=AST_RETURNS_NUMBER;
			ast_node_t *a=ast_find_type(node->children, AST_TYPE_ARRAYREF);
			if (!a) a=ast_find_type(node->children, AST_TYPE_STRUCTREF);
			if (a) {
				n->size=size_of_obj(a);
			} else {
				n->size=1;
			}
		}
		if (n->type==AST_TYPE_STRUCTREF) {
			n->parent->returns=AST_RETURNS_STRUCT;
			//this will get overwritten when we search deeper, if needed
			n->returns=AST_RETURNS_NUMBER;
			n->size=size_of_obj(n);
		}
		if (n->type==AST_TYPE_STRUCTDEF) {
			for (ast_node_t *i=n->children; i!=0; i=i->sibling) {
				if (i->type==AST_TYPE_STRUCTMEMBER) {
					i->size=size_of_obj(i);
				}
			}
		}
		ast_ops_annotate_obj_decl_size(n->children);
	}
}

void annotate_obj_ref_size(ast_node_t *n, ast_node_t *d) {
	ast_node_t *nt=NULL;
	if (n->type==AST_TYPE_ARRAYREF) {
		ast_node_t *a=ast_find_type(d->children, AST_TYPE_ARRAYREF);
		if (!a) {
			panic_error(n, "Cannot dereference non-array object");
		} else {
			n->size=a->size;
			n->value=a;
			nt=a;
		}
	} else if (n->type==AST_TYPE_STRUCTMEMBER) {
		ast_node_t *s=ast_find_type(d->children, AST_TYPE_STRUCTREF);
		//structref references to a structdef, and we need to select the proper child
		ast_node_t *m=s->value->children;
		int off=0;
		while (m!=NULL && strcmp(m->name, n->name)!=0) {
			off+=m->size;
			m=m->sibling;
		}
		if (!m) {
			panic_error(n, "No such member in struct '%s'", s->value->name);
		} else {
			n->size=off;
			n->value=m;
			nt=m;
		}
	}
	if (nt) {
		ast_node_t *nn=ast_find_type(n->children, AST_TYPE_STRUCTMEMBER);
		if (!nn) nn=ast_find_type(n->children, AST_TYPE_ARRAYREF);
		if (nn) {
			annotate_obj_ref_size(nn, nt);
		}
	}
}


void ast_ops_annotate_obj_ref_size(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_REF || n->type==AST_TYPE_DEREF) {
			ast_node_t *nn=ast_find_type(n->children, AST_TYPE_STRUCTMEMBER);
			if (!nn) nn=ast_find_type(n->children, AST_TYPE_ARRAYREF);
			if (nn) annotate_obj_ref_size(nn, n->value);
		}
		ast_ops_annotate_obj_ref_size(n->children);
	}
}


void ast_ops_move_functions_down(ast_node_t *node) {
	//On the global level, we want all functions on the bottom and all non-functions
	//(variable declarations, initializers, etc) up top.
	ast_node_t *last=node;
	while (last->sibling) last=last->sibling;
	
	ast_node_t *orig_last=last;
	ast_node_t *prev=NULL;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n==orig_last) break;
		if (n->type==AST_TYPE_FUNCDEF) {
			//Surgery: move to bottom of program
			prev->sibling=n->sibling; //cut out of list
			n->sibling=NULL;		  //n is going to be at end of list
			last->sibling=n;		  //add to end of program
			last=n;					  //n is now new node
			n=prev;					  //otherwise we'll loop as n moved
		}
		prev=n;
	}
}

void ast_ops_add_obj_initializers(ast_node_t *node) {
	//On the global level, we want to collect all initializers and put them at the very
	//start of the program.
	ast_node_t *prev=NULL;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_DECLARE) {
			//Surgery: move to top of program
			prev->sibling=n->sibling;
			n->sibling=node->sibling;
			node->sibling=n;
			//otherwise we'll loop as n moved
			n=prev;
		}
		//save for next iteration
		prev=n;
	}
}

uint8_t *ast_ops_gen_binary(ast_node_t *node, int *len) {
	prog_t p={};
	p.size=PROG_INC;
	p.data=malloc(PROG_INC);
	add_long_prog(&p, LSSL_VM_VER);
	add_long_prog(&p, globals_size(node));
	gen_binary(node, &p);
	*len=p.len;
	return p.data;
}

//Calls all ops (except binary generation) in the proper order
void ast_ops_do_compile(ast_node_t *prognode) {
//	ast_dump(prognode); exit(0);
	ast_ops_fix_parents(prognode);
	ast_ops_check_empty_array_deref(prognode);
	ast_ops_assoc_structrefs(prognode);
	ast_ops_add_program_start(prognode, "main");
	ast_ops_collate_consts(prognode);
	ast_ops_attach_symbol_defs(prognode);
	ast_ops_fix_parents(prognode);
	ast_ops_add_trailing_return(prognode);
	ast_ops_fix_parents(prognode);
	ast_ops_var_place(prognode);
	ast_ops_add_obj_initializers(prognode);
	ast_ops_move_functions_down(prognode);
	ast_ops_annotate_obj_decl_size(prognode);
	ast_ops_annotate_obj_ref_size(prognode);
	ast_ops_fix_function_args(prognode);
	codegen(prognode);
	ast_ops_fixup_enter_return(prognode);
	ast_ops_remove_useless_ops(prognode);
	ast_ops_position_insns(prognode);
	ast_ops_assign_addr_to_fndef_node(prognode);
	ast_ops_fixup_addrs(prognode);
	//ast_dump(prognode);
}
