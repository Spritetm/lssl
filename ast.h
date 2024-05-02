#pragma once
#include <stdint.h>
#include "file_loc.h"
#include "insn.h"

typedef struct ast_node_t ast_node_t;


#define AST_TYPES \
	AST_TYPE_ENTRY(NUMBER) \
	AST_TYPE_ENTRY(PLUS) AST_TYPE_ENTRY(MINUS) AST_TYPE_ENTRY(TIMES) AST_TYPE_ENTRY(DIVIDE) \
	AST_TYPE_ENTRY(FOR) AST_TYPE_ENTRY(IF) AST_TYPE_ENTRY(WHILE) \
	AST_TYPE_ENTRY(TEQ) AST_TYPE_ENTRY(TNEQ) AST_TYPE_ENTRY(TL) AST_TYPE_ENTRY(TLEQ) \
	AST_TYPE_ENTRY(FUNCDEF) \
	AST_TYPE_ENTRY(FUNCDEFARG) \
	AST_TYPE_ENTRY(FUNCCALL) \
	AST_TYPE_ENTRY(FUNCCALLARG) \
	AST_TYPE_ENTRY(BLOCK) \
	AST_TYPE_ENTRY(DROP) \
	AST_TYPE_ENTRY(ASSIGN) \
	AST_TYPE_ENTRY(VAR) \
	AST_TYPE_ENTRY(DECLARE) \
	AST_TYPE_ENTRY(LOCALSIZE) \
	AST_TYPE_ENTRY(INSN) \
	AST_TYPE_ENTRY(RETURN) \
	AST_TYPE_ENTRY(SYSCALL) \
	AST_TYPE_ENTRY(FUNCPTR) \
	AST_TYPE_ENTRY(ARRAYREF) AST_TYPE_ENTRY(ARRAYREF_EMPTY) \
	AST_TYPE_ENTRY(POST_ADD) AST_TYPE_ENTRY(PRE_ADD) \
	AST_TYPE_ENTRY(PROGRAM_START) \
	AST_TYPE_ENTRY(GOTO) \
	AST_TYPE_ENTRY(STRUCTDEF) AST_TYPE_ENTRY(STRUCTREF) AST_TYPE_ENTRY(STRUCTMEMBER) \
	AST_TYPE_ENTRY(DATATYPE) \
	AST_TYPE_ENTRY(DEREF) \
	AST_TYPE_ENTRY(REF)

#define AST_TYPE_ENTRY(x) AST_TYPE_##x,
typedef enum {
	AST_TYPES
} ast_type_en;
#undef AST_TYPE_ENTRY

typedef enum {
	AST_RETURNS_UNK=0,
	AST_RETURNS_CONST,
	AST_RETURNS_NUMBER,
	AST_RETURNS_ARRAY,
	AST_RETURNS_STRUCT,
	AST_RETURNS_FUNCTION
} ast_returns_t;

struct ast_node_t {
	ast_type_en type;
	ast_returns_t returns;
	char *name; //if not NULL, memory here is owned by the node
	int32_t number;
	int size;
	file_loc_t loc;
	ast_node_t *value; //e.g. the node for a var def in case of a var ref, of func def for func ref
	int valpos; //actual location in memory of value
	ast_node_t *parent;
	ast_node_t *children;
	ast_node_t *sibling;
	ast_node_t *alloc_next;
	lssl_insn_enum insn_type;
	int insn_arg;
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


ast_node_t *ast_gen_program_start_node();
ast_node_t *ast_new_node(ast_type_en type, file_loc_t *loc);
void ast_free_all(ast_node_t *node);
ast_node_t *ast_new_node_2chld(ast_type_en type, file_loc_t *loc, ast_node_t *c1, ast_node_t *c2);
ast_node_t *ast_find_deepest_arrayref(ast_node_t *node);
ast_node_t *ast_find_deepest_ref(ast_node_t *node);
ast_node_t *ast_find_type(ast_node_t *node, ast_type_en type);
void ast_dump(ast_node_t *node);


