#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include "ast.h"


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
	printf(ast_type_str[node->type]);
	if (node->name) printf(" '%s'", node->name);
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

ast_node_t *ast_new_node(ast_type_en type) {
	ast_node_t *r=calloc(sizeof(ast_node_t), 1);
	r->type=type;
	return r;
}

ast_node_t *ast_new_node_2chld(ast_type_en type, ast_node_t *c1, ast_node_t *c2) {
	assert(c1->sibling==NULL);
	assert(c2->sibling==NULL);
	ast_node_t *r=ast_new_node(type);
	r->children=c1;
	c1->sibling=c2;
	return r;
}

