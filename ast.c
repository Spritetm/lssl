#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "vm_defs.h"
#include "ast.h"
#include "insn.h"

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
	} else if (lssl_vm_ops[i].argtype==ARG_LABEL) {
		printf("%s %d", lssl_vm_ops[i].op, node->insn_arg);
	} else if (lssl_vm_ops[i].argtype==ARG_TARGET) {
		printf("%s 0x%X", lssl_vm_ops[i].op, node->insn_arg);
	} else if (lssl_vm_ops[i].argtype==ARG_FUNCTION) {
		printf("%s [%d] ; %s", lssl_vm_ops[i].op, node->insn_arg, node->value->name);
	}
}

const static char *ast_type_str[]={
	"INT", "FLOAT",
	"PLUS", "MINUS", "TIMES", "DIVIDE",
	"FOR", "IF", "WHILE",
	"TEQ", "TNEQ", "TL", "TLEQ",
	"FUNCDEF",
	"FUNCDEFARG",
	"FUNCCALL",
	"FUNCCALLARG",
	"BLOCK",
	"DROP",
	"ASSIGN",
	"VAR",
	"DECLARE",
	"LOCALSIZE",
	"INST",
	"RETURN",
	"SYSCALL",
};


//Dumps a node plus its children. Note: NOT its siblings.
static void dump_node(ast_node_t *node, int depth) {
	static_assert(sizeof(ast_type_str)/sizeof(ast_type_str[0])==AST_TYPE_MAX, "ast type desync");
	printf(" ");
	for (int i=0; i<depth; i++) printf("|");
	if (node==NULL) {
		printf("NULL\n");
		return;
	}
	printf("%s", ast_type_str[node->type]);
	if (node->name) printf(" '%s'", node->name);
	if (node->type==AST_TYPE_INT) printf(" (%d)", node->numberi);
	if (node->type==AST_TYPE_FLOAT) printf(" (%f)", node->numberf);
	if (node->type==AST_TYPE_LOCALSIZE) printf(" (%i entries)", node->numberi);
	if (node->type==AST_TYPE_DECLARE) printf(" (position is %d)", node->valpos);
	if (node->type==AST_TYPE_FUNCDEFARG) printf(" (position is %d)", node->valpos);
	if (node->type==AST_TYPE_INSN) {
		printf(":\t");
		dump_insn(node);
	}
	printf("\n");
	//Dump all children
	for (ast_node_t *n=node->children; n!=NULL; n=n->sibling) {
		dump_node(n, depth+1);
	}
}

void ast_dump(ast_node_t *node) {
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		dump_node(n, 0);
	}
	printf("dumped\n");
}

ast_node_t *ast_new_node(ast_type_en type, file_loc_t *loc) {
	ast_node_t *r=calloc(sizeof(ast_node_t), 1);
	r->type=type;
	memcpy(&r->loc, loc, sizeof(file_loc_t));
	return r;
}

ast_node_t *ast_new_node_2chld(ast_type_en type, file_loc_t *loc, ast_node_t *c1, ast_node_t *c2) {
	assert(c1->sibling==NULL);
	assert(c2->sibling==NULL);
	ast_node_t *r=ast_new_node(type, loc);
	r->children=c1;
	c1->sibling=c2;
	return r;
}

