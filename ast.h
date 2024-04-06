#pragma once
#include <stdint.h>

typedef struct ast_node_t ast_node_t;

typedef enum {
	AST_TYPE_INT=0,
	AST_TYPE_FLOAT,
	AST_TYPE_PLUS, AST_TYPE_MINUS, AST_TYPE_TIMES, AST_TYPE_DIVIDE,
	AST_TYPE_FOR, AST_TYPE_IF, AST_TYPE_WHILE,
	AST_TYPE_TEQ, AST_TYPE_TNEQ, AST_TYPE_TL, AST_TYPE_TLEQ,
	AST_TYPE_FUNCDEF,
	AST_TYPE_FUNCDEFARG,
	AST_TYPE_FUNCCALL,
	AST_TYPE_FUNCCALLARG,
	AST_TYPE_BLOCK,
	AST_TYPE_DROP,
	AST_TYPE_ASSIGN,
	AST_TYPE_VAR,
	AST_TYPE_DECLARE,
	AST_TYPE_MAX //always last item
} ast_type_en;


struct ast_node_t {
	int type;
	char *name;
	int32_t numberi;
	float numberf;
	char *str;
	ast_node_t *parent;
	ast_node_t *children;
	ast_node_t *sibling;
};

static inline void ast_add_child(ast_node_t *parent, ast_node_t *n) {
	if (parent->children==NULL) {
		parent->children=n;
	} else {
		ast_node_t *p=parent->children;
		while(p->sibling!=NULL) p=p->sibling;
		p->sibling=n;
	}
}

static inline void ast_add_sibling(ast_node_t *t, ast_node_t *n) {
	while (t->sibling!=NULL) t=t->sibling;
	t->sibling=n;
}


ast_node_t *ast_new_node(ast_type_en type);
ast_node_t *ast_new_node_2chld(ast_type_en type, ast_node_t *c1, ast_node_t *c2);
void ast_dump(ast_node_t *node);


