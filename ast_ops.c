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

/*
Urgh, non-POD variables.

Redo The Idea Of Non-POD.
A var is a 32-bit 16.16 fixed point number.
A non-POD is a 32-bit number, divided into 2 fields:
- Pointer to the data
- Size of the data.
Because the compiler always knows the data type of a non-POD, it can
resolve any dereference.

There are two basic non-POD building blocks for now:
* 1-d arrays
* Structures.

These can be mixed: you can have an 1-d array of struct containing an 1-d 
array of 1-d arrays etc.

Dereferencing a pointer or struct actually results in one of two things:
- A pointer to the resulting non-POD, or
- The value of the POD.

For instance:
struct rgb {
	var r;
	var g;
	var b;
};

struct led {
	rgb colors[3];
	var pos;
}

led leds[20];

x=leds[3].colors[1].b

(leds)
push address_of_leds;
rd_var
([3])
push 3
arr_rel_idx sizeof(led) //rel_idx s:   pop ptr, pop ct, ptr.addr+=ct*s; ptr.size=s; push ptr
(.colors)
push 0;
struct_off sizeof(rgb*3) //struct_off s: pop ptr, pop idx, ptr.addr+=idx, ptr.size=s; push ptr
([1])
push 1;
arr_rel_idx sizeof(rgb)
(.r)
push 2
struct_off sizeof(var);
(finally, dereference to pod)
deref					// pop ptr; assert ptr.size==1; push mem[ptr.addr]


struct a_t {
	var b;
	var c[1];
	b_t d;
	b_t e[2];
}

structdef
 - declare {.returns=number}
 - declare {.returns=array}
	- arraydef {.number=1}
 - declare {.returns=struct}
	- structref {.value=node(a_t)}
 - declare {.returns=struct_array}
	- arraydef {.number=2}
	- structref {.value=node(a,t)}

var u
declare {.returns=number, .size=1;}
var v[2];
declare {.returns=obj}
 - arraydef {.number=2}
var w[2][3];
declare {.returns=obj}
 - arraydef 
  - number {3}
  - arraydef {.number=3}
a_t x;
declare {.returns=obj}
 - structdef {.value=node(a_t)}

a_t y[3]:
declare {.returns=obj}
 - arraydef {}
  - number(3)
  - structref {.value=node(a_t)}




*/



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
			int64_t v=arg[0]*arg[1];
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

static void annotate_symbols(ast_node_t *node, ast_sym_list_t *syms, int is_global) {
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
		if (n->children) annotate_symbols(n->children, syms, 0);
	}

	//restore old pos, deleting local var defs
	syms->node_pos=old_sym_pos;
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
	annotate_symbols(node, syms, 1);

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
			n->valpos=start_pos+pos;
			pos+=n->size;
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
void ast_ops_var_place(ast_node_t *node) {
	//Number arguments
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF) {
			function_number_arguments(n);
		}
	}
	
	int glob_pos=0;
	//Find globals
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_DECLARE) {
			n->valpos=glob_pos;
			glob_pos+=n->size;
		}
	}
	
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


void annotate_deref_datatype(ast_node_t *node, ast_node_t *typepos) {
	if (!node) return;
	if (!typepos) return;
	ast_node_t *r, *t;
	r=ast_find_type(node->children, AST_TYPE_ARRAYREF);
	t=ast_find_type(typepos->children, AST_TYPE_ARRAYREF);
	if (r && t) {
		ast_node_t *a=ast_new_node(AST_TYPE_DATATYPE, &node->loc);
		a->value=t;
		a->sibling=node->children;
		node->children=a;
		annotate_deref_datatype(r, t);
		return;
	} else if (r) {
		panic_error(r, "Cannot dereference this; not an array");
		return;
	}
	r=ast_find_type(node->children, AST_TYPE_STRUCTMEMBER);
	t=ast_find_type(typepos->children, AST_TYPE_STRUCTREF);
	if (r && t) {
		ast_node_t *a=ast_new_node(AST_TYPE_DATATYPE, &node->loc);
		a->value=t->value; //the STRUCTDEF associated with the STRUCTREF
		//find the structmember node associated
		ast_node_t *structmember=t->value->children;
		while (structmember!=NULL && strcmp(structmember->name, r->name)!=0) {
			structmember=structmember->sibling;
		}
		if (structmember) {
			annotate_deref_datatype(r, structmember);
		} else {
			panic_error(r, "No such member in struct: %s", r->name);
		}
		return;
	} else if (r) {
		panic_error(r, "Getting member on something that is not a struct");
		return;
	}

	//Probably a POD.
	return;
}

//Add datatype node to non-POD derefs
//effectively for each part of a deref, it attaches a node indicating the
//type of what is returned.
void ast_ops_annotate_datatype(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_DEREF) {
			annotate_deref_datatype(node, node->value);
		}
		if (n->children) ast_ops_annotate_datatype(n->children);
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
	}
	return glob_size;
}

//Program should start (after array initializers) with a jump to 'main'.
void ast_ops_add_program_start(ast_node_t *node, const char *main_fn_name) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_FUNCDEF && strcmp(n->name, main_fn_name)==0) {
			ast_node_t *d=ast_new_node(AST_TYPE_GOTO, &node->loc);
			d->name=strdup(main_fn_name);
			d->sibling=node->sibling;
			node->sibling=d;
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
void ast_ops_annotate_obj_size(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_ARRAYREF) {
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
		ast_ops_annotate_obj_size(n->children);
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
	ast_ops_fix_parents(prognode);
	ast_ops_assoc_structrefs(prognode);
	ast_ops_add_program_start(prognode, "main");
	ast_ops_collate_consts(prognode);
	ast_ops_attach_symbol_defs(prognode);
	ast_ops_annotate_datatype(prognode);
	ast_ops_fix_parents(prognode);
	ast_ops_add_trailing_return(prognode);
	ast_ops_fix_parents(prognode);
	ast_ops_var_place(prognode);
	ast_ops_add_obj_initializers(prognode);
	ast_ops_annotate_obj_size(prognode);
	codegen(prognode);
	ast_ops_fixup_enter_return(prognode);
	ast_ops_remove_useless_ops(prognode);
	ast_ops_position_insns(prognode);
	ast_ops_assign_addr_to_fndef_node(prognode);
	ast_ops_fixup_addrs(prognode);
	ast_dump(prognode);
}
