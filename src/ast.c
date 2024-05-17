#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "vm_defs.h"
#include "ast.h"


static void dump_insn(ast_node_t *node) {
	assert(node->type==AST_TYPE_INSN);
	int i=node->insn_type;
	printf("[@%04X] ", node->valpos);
	if (lssl_vm_ops[i].argtype==ARG_NONE || lssl_vm_ops[i].argtype==ARG_NOP) {
		printf("%s", lssl_vm_ops[i].op);
	} else if (lssl_vm_ops[i].argtype==ARG_INT) {
		printf("%s %d", lssl_vm_ops[i].op, node->insn_arg);
	} else if (lssl_vm_ops[i].argtype==ARG_REAL) {
		printf("%s %f", lssl_vm_ops[i].op, (node->insn_arg/65536.0));
	} else if (lssl_vm_ops[i].argtype==ARG_VAR) {
		printf("%s [%d] ; %s", lssl_vm_ops[i].op, node->insn_arg, node->value->name);
	} else if (lssl_vm_ops[i].argtype==ARG_STRUCT || lssl_vm_ops[i].argtype==ARG_ARRAY) {
		printf("%s [%d]", lssl_vm_ops[i].op, node->insn_arg);
	} else if (lssl_vm_ops[i].argtype==ARG_LABEL) {
		printf("%s %d", lssl_vm_ops[i].op, node->insn_arg);
	} else if (lssl_vm_ops[i].argtype==ARG_TARGET) {
		printf("%s 0x%X", lssl_vm_ops[i].op, node->insn_arg);
	} else if (lssl_vm_ops[i].argtype==ARG_FUNCTION) {
		printf("%s [%d] ; %s", lssl_vm_ops[i].op, node->insn_arg, node->value->name);
	}
}

#define AST_TYPE_ENTRY(x) #x,
const char *ast_type_str[]={
	AST_TYPES
};
#undef AST_TYPE_ENTRY


//Dumps a node plus its children. Note: NOT its siblings.
static void dump_node(ast_node_t *node, int depth) {
	const char *ret_str[]={"unk", "const", "num", "arr", "struct", "fn"};
	if (node==NULL) {
		printf("dump_node: NULL node passed\n");
		return;
	}
	if (node->type<0 || node->type>=(sizeof(ast_type_str)/sizeof(ast_type_str[0]))) {
		printf("dump_node: unknown node type %d\n", node->type);
		return;
	}
	printf(" ");
	for (int i=0; i<depth; i++) printf("|");
	if (node==NULL) {
		printf("NULL\n");
		return;
	}
	printf("%s", ast_type_str[node->type]);
	if (node->name) printf(" '%s'", node->name);
	if (node->value) printf(" (value '%s')", node->value->name);
	if (node->type==AST_TYPE_NUMBER) printf(" (%f)", node->number/65536.0);
	if (node->type==AST_TYPE_LOCALSIZE) printf(" (%i entries)", node->number);
	if (node->type==AST_TYPE_DECLARE) printf(" (position is %d, size %d)", node->valpos, node->size);
	if (node->type==AST_TYPE_FUNCDEFARG) printf(" (position is %d)", node->valpos);
	if (node->type==AST_TYPE_ARRAYREF) printf(" (size %d)", node->size);
	if (node->type==AST_TYPE_STRUCTREF) printf(" (size %d)", node->size);
	if (node->type==AST_TYPE_STRUCTMEMBER) printf(" (offset %d)", node->size);
	if (node->type==AST_TYPE_INSN) {
		printf(":\t");
		dump_insn(node);
	}
	if (node->returns) printf(" (ret=%s)", ret_str[node->returns]);
	printf("\n");
	//Dump all children
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		dump_node(n, depth+1);
	}
}

void ast_dump(ast_node_t *node) {
	if (node==NULL) {
		printf("ast_dump: NULL node passed\n");
		return;
	}
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		dump_node(n, 0);
	}
	printf("dumped\n");
}

static ast_node_t *last_alloc=NULL;

//This is technically not re-entrant because the ref to last_alloc
ast_node_t *ast_new_node(ast_type_en type, file_loc_t *loc) {
	ast_node_t *r=calloc(sizeof(ast_node_t), 1);
	r->type=type;
	if (last_alloc) last_alloc->alloc_next=r;
	last_alloc=r;
	memcpy(&r->loc, loc, sizeof(file_loc_t));
	return r;
}

void ast_free_all(ast_node_t *node) {
	while (node) {
		ast_node_t *next=node->alloc_next;
		if (node==last_alloc) last_alloc=NULL;
		free(node->name);
		free(node);
		node=next;
	}
}

ast_node_t *ast_new_node_2chld(ast_type_en type, file_loc_t *loc, ast_node_t *c1, ast_node_t *c2) {
	assert(c1->sibling==NULL);
	assert(c2->sibling==NULL);
	ast_node_t *r=ast_new_node(type, loc);
	r->children=c1;
	c1->sibling=c2;
	if (c1->returns==AST_RETURNS_CONST && c2->returns==AST_RETURNS_CONST) {
		r->returns=AST_RETURNS_CONST;
	} else if (c1->returns==AST_RETURNS_CONST && c2->returns==AST_RETURNS_NUMBER) {
		r->returns=AST_RETURNS_NUMBER;
	} else if (c1->returns==AST_RETURNS_NUMBER && c2->returns==AST_RETURNS_CONST) {
		r->returns=AST_RETURNS_NUMBER;
	} else if (c1->returns==AST_RETURNS_NUMBER && c2->returns==AST_RETURNS_NUMBER) {
		r->returns=AST_RETURNS_NUMBER;
	} else {
		//ToDo: throw error
	}
	return r;
}


//This generates a dummy node that is always the 1st item as otherwise we cannot pre-pend nodes before
//the main program.
ast_node_t *ast_gen_program_start_node() {
	file_loc_t loc={};
	return ast_new_node(AST_TYPE_PROGRAM_START, &loc);
}

//Returns the deepest AST_TYPE_ARRAYREF node.
ast_node_t *ast_find_deepest_arrayref(ast_node_t *node) {
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_ARRAYREF) {
			return ast_find_deepest_arrayref(n);
		}
	}
	return node;
}


//Returns the deepest AST_TYPE_ARRAYREF/STRUCTREF node.
ast_node_t *ast_find_deepest_ref(ast_node_t *node) {
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_ARRAYREF || n->type==AST_TYPE_STRUCTMEMBER) {
			return ast_find_deepest_ref(n);
		}
	}
	return node;
}

ast_node_t *ast_find_type(ast_node_t *node, ast_type_en type) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==type) return n;
	}
	return NULL;
}

const file_loc_t *ast_lookup_loc_for_pc(ast_node_t *node, int32_t pc) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_INSN && n->insn_type!=INSN_NOP && n->valpos==pc) {
			//insns don't have a file loc, but their parent does.
			return &n->parent->loc;
		}
		if (n->children) {
			const file_loc_t *r=ast_lookup_loc_for_pc(n->children, pc);
			if (r) return r;
		}
	}
	return NULL;
}




