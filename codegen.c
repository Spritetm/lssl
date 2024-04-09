#include <stdio.h>
#include <stdlib.h>
#include <ast.h>

static insn_t *chain_insn(insn_t *old, lssl_instr_enum type, int arg) {
	insn_t *n=calloc(sizeof(insn_t), 1);
	if (old) {
		int prev_ins_size=lssl_vm_argtypes[lssl_vm_ops[old->type].type].byte_size+1;
		n->pos=old->pos+prev_ins_size;
		old->next=n;
	}
	n->type=type;
	n->value=arg;
}

static insn_t *handle_rhs_lhs_op(insn_t *cur, ast_node_t *n, lssl_instr_enum type) {
	ast_node_t *p=n->children;
	cur=codegen(cur, p); //calculate arg1
	cur=codegen(cur, p->sibling); //calculate arg2
	ret=chain_insn(ret, type, (int)(n->numberf*65536));
}

insn_t *codegen(ast_node_t *node, insn_t *cur) {
	assert(n && "Codegen called with NULL arg");
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		if (n->type==AST_TYPE_INT) {
			cur=chain_insn(cur, INSN_PUSH_R, n->numberi);
		} else if (n->type==AST_TYPE_FLOAT) {
			cur=chain_insn(cur, INSN_PUSH_R, (int)(n->numberf*65536));
		} else if (n->type==AST_TYPE_PLUS) {
			cur=handle_rhs_lhs_op(cur, n->children, INSN_ADD);
		} else if (n->type==AST_TYPE_MINUS) {
			cur=handle_rhs_lhs_op(cur, n->children, INSN_SUB);
		} else if (n->type==AST_TYPE_TIMES) {
			cur=handle_rhs_lhs_op(cur, n->children, INSN_MUL);
		} else if (n->type==AST_TYPE_DIVIDE) {
			cur=handle_rhs_lhs_op(cur, n->children, INSN_DIV);
		} else if (n->type==AST_TYPE_FOR) {
			ast_handle_t *c=n->children;
			cur=codegen(c, cur); //generate initial code
			c=c->sibling;
			cur=codegen(c, cur); //generate initial code

		} else if (n->type==AST_TYPE_IF) {
		} else if (n->type==AST_TYPE_WHILE) {
		} else if (n->type==AST_TYPE_TEQ) {
		} else if (n->type==AST_TYPE_TNEQ) {
		} else if (n->type==AST_TYPE_TL) {
		} else if (n->type==AST_TYPE_TLEQ) {
		} else if (n->type==AST_TYPE_FUNCDEF) {
		} else if (n->type==AST_TYPE_FUNCDEFARG) {
		} else if (n->type==AST_TYPE_FUNCCALL) {
		} else if (n->type==AST_TYPE_FUNCCALLARG) {
		} else if (n->type==AST_TYPE_BLOCK) {
		} else if (n->type==AST_TYPE_DROP) {
		} else if (n->type==AST_TYPE_ASSIGN) {
		} else if (n->type==AST_TYPE_VAR) {
		} else if (n->type==AST_TYPE_DECLARE) {
		} else if (n->type==AST_TYPE_LOCALSIZE) {
		} else {
			printf("Eek! Unknown ast type %d\n", n->type);
		}
	}
	return ret;
}