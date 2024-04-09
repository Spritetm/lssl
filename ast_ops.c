#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ast.h"

//Mental node: definition of 'fixup' is finding a position (e.g. in ram) for a symbol.


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
		} else if (n->type==AST_TYPE_VAR || n->type==AST_TYPE_ASSIGN) {
			//Find variable symbol and resolve
			ast_node_t *s=find_symbol(syms, n->name);
			if (!s) {
				printf("Undefined var/function %s\n", n->name);
				exit(1);
			}
			if (s->type==AST_TYPE_FUNCDEF) {
				printf("Cannot use function %s in expression! (Forgot the brackets?)\n", s->name);
				exit(1);
			}
			n->value=s;
		} else if (n->type==AST_TYPE_FUNCCALL) {
			//Find function symbol and resolve. ToDo: check if
			//function arg types correspond to declared arg types.
			ast_node_t *s=find_symbol(syms, n->name);
			if (!s) {
				printf("Undefined var/function %s\n", n->name);
				exit(1);
			}
			if (s->type!=AST_TYPE_FUNCDEF) {
				printf("Cannot call non-function %s\n", s->name);
				exit(1);
			}
			n->value=s;
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
			assert(last);
			while (last->sibling) last=last->sibling;
			if (last->type!=AST_TYPE_RETURN) {
				ast_node_t *r=ast_new_node(AST_TYPE_RETURN, &n->loc);
				ast_add_sibling(last, r);
				//defaults to an int with value 0
				ast_add_child(r, ast_new_node(AST_TYPE_INT, &n->loc));
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
	int i=-2-(arg_size-1);
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
	int pos=start_pos;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_DECLARE) {
			n->valpos=pos++;
		}
	}
	
	//Grab max size of any of the subblocks (and allocate pos for vars there)
	int subblk_max_size=0;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->children) {
			int s=block_place_locals(n->children, pos);
			if (subblk_max_size<s) subblk_max_size=s;
		}
	}
	return pos+subblk_max_size;
}

//Figure out and set the offsets of variables and func args
//Returns size of globals.
int ast_ops_var_place(ast_node_t *node) {
	//Find globals first.
	int glob_size=0;
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_DECLARE) {
			n->valpos=glob_size;
			glob_size++;
		}
	}

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

	return glob_size;
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


//Fixes up these instructions
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





