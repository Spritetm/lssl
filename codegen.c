#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ast.h"
#include "codegen.h"
#include "error.h"

/*
Note: This adds instructions to within the AST.
*/

static void codegen_node(ast_node_t *n);

//Inserts an instruction before the arg nodes, but after the other instructions.
static ast_node_t *insert_insn_before_arg_eval(ast_node_t *parent, lssl_insn_enum type) {
	ast_node_t *n=ast_new_node(AST_TYPE_INSN, &parent->loc);
	n->insn_type=type;
	ast_node_t *p=parent->children;
	if (!p || p->type!=AST_TYPE_INSN) {
		//No children, or first child is no insn. Insert in the top of the list.
		n->sibling=parent->children;
		parent->children=n;
	} else {
		//Find last insn
		while(p->sibling && p->sibling->type==AST_TYPE_INSN) p=p->sibling;
		//insert into list
		n->sibling=p->sibling;
		p->sibling=n;
	}
	return n;
}


//note: first arg has n=1
static ast_node_t *nth_param(ast_node_t *p, int n) {
	assert(n!=0); //is error
	p=p->children;
	while(p) {
		if (p->type==AST_TYPE_INSN || 
			p->type==AST_TYPE_LOCALSIZE) {
			//ignore
		} else {
			n--;
			if (n==0) return p;
		}
		p=p->sibling;
	}
	return NULL;
}


//Inserts an instruction after all args and existing instructions
static ast_node_t *insert_insn_after_arg_eval(ast_node_t *parent, lssl_insn_enum type, int argnum) {
	ast_node_t *n=ast_new_node(AST_TYPE_INSN, &parent->loc);
	n->insn_type=type;
	if (parent->children==NULL) {
		//wut, no args? Hm.
		parent->children=n;
	} else {
		ast_node_t *l=nth_param(parent, argnum);
		assert(l && "insert_insn_after_arg_eval: no nth parameter exists!");
		//skip over insns
		while (l->sibling!=NULL && l->sibling->type==AST_TYPE_INSN) l=l->sibling;
		//insert there
		n->sibling=l->sibling;
		l->sibling=n;
	}
	return n;
}

static ast_node_t *insert_insn_after_all_arg_eval(ast_node_t *parent, lssl_insn_enum type){
	ast_node_t *n=ast_new_node(AST_TYPE_INSN, &parent->loc);
	n->insn_type=type;
	if (parent->children==NULL) {
		parent->children=n;
	} else {
		ast_node_t *l=parent->children;
		while (l->sibling) l=l->sibling;
		l->sibling=n;
	}
	return n;
}

static void codegen_node(ast_node_t *n);

//Same as codegen, but checks if the arg is a ref/deref of a POD.
void codegen_node_pod(ast_node_t *n) {
	if (n->returns==AST_RETURNS_ARRAY) {
		panic_error(n, "Expected number, got array!");
	} else if (n->returns==AST_RETURNS_STRUCT) {
		panic_error(n, "Expected number, got struct!");
	} else if (n->returns==AST_RETURNS_FUNCTION) {
		panic_error(n, "Expected number, got function!");
	}
	codegen_node(n);
}


void handle_rhs_lhs_op_pod(ast_node_t *node, lssl_insn_enum type) {
	codegen_node_pod(nth_param(node, 1));
	codegen_node_pod(nth_param(node, 2));
	insert_insn_after_arg_eval(node, type, 2);
}

void handle_rhs_lhs_op(ast_node_t *node, lssl_insn_enum type) {
	codegen_node_pod(nth_param(node, 1));
	codegen_node_pod(nth_param(node, 2));
	insert_insn_after_arg_eval(node, type, 2);
}



static void codegen_node(ast_node_t *n) {
	if (!n) return;
	if (n->type==AST_TYPE_NUMBER) {
		if ((n->number&0xffff)==0) {
			ast_node_t *i=insert_insn_before_arg_eval(n, INSN_PUSH_I);
			i->insn_arg=n->number>>16;
		} else {
			ast_node_t *i=insert_insn_before_arg_eval(n, INSN_PUSH_R);
			i->insn_arg=n->number;
		}
	} else if (n->type==AST_TYPE_PLUS) {
		handle_rhs_lhs_op_pod(n, INSN_ADD);
	} else if (n->type==AST_TYPE_MINUS) {
		handle_rhs_lhs_op_pod(n, INSN_SUB);
	} else if (n->type==AST_TYPE_TIMES) {
		handle_rhs_lhs_op_pod(n, INSN_MUL);
	} else if (n->type==AST_TYPE_DIVIDE) {
		handle_rhs_lhs_op_pod(n, INSN_DIV);
	} else if (n->type==AST_TYPE_LAND) {
		handle_rhs_lhs_op_pod(n, INSN_LAND);
	} else if (n->type==AST_TYPE_LOR) {
		handle_rhs_lhs_op_pod(n, INSN_LOR);
	} else if (n->type==AST_TYPE_BAND) {
		handle_rhs_lhs_op_pod(n, INSN_BAND);
	} else if (n->type==AST_TYPE_BOR) {
		handle_rhs_lhs_op_pod(n, INSN_BOR);
	} else if (n->type==AST_TYPE_BXOR) {
		handle_rhs_lhs_op_pod(n, INSN_BXOR);
	} else if (n->type==AST_TYPE_LNOT) {
		codegen_node_pod(nth_param(n, 1));
		insert_insn_after_arg_eval(n, INSN_LNOT, 1);
	} else if (n->type==AST_TYPE_BNOT) {
		codegen_node_pod(nth_param(n, 1));
		insert_insn_after_arg_eval(n, INSN_BNOT, 1);
	} else if (n->type==AST_TYPE_FOR) {
		//Note: the body and increment are swapped in the AST wrt the order
		//they appear in the code.
		codegen_node(nth_param(n, 1)); //initial
		codegen_node_pod(nth_param(n, 2)); //condition
		codegen_node(nth_param(n, 3)); //body
		codegen_node(nth_param(n, 4)); //increment
		//dummy after initial so we can jump back there
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_NOP, 1);
		//conditional jump to end after condition
		ast_node_t *j=insert_insn_after_arg_eval(n, INSN_JZ, 2); 
		//jump to condition at end of body/increment
		ast_node_t *k=insert_insn_after_arg_eval(n, INSN_JMP, 4);
		//nop for address placeholder for conditional jump
		ast_node_t *l=insert_insn_after_arg_eval(n, INSN_NOP, 4);
		j->value=l;
		k->value=i;
	} else if (n->type==AST_TYPE_IF) {
		codegen_node_pod(nth_param(n, 1)); //condition
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_JZ, 1);
		codegen_node(nth_param(n, 2)); //body
		ast_node_t *j=insert_insn_after_arg_eval(n, INSN_NOP, 2);
		i->value=j;
	} else if (n->type==AST_TYPE_WHILE) {
		ast_node_t *h=insert_insn_before_arg_eval(n, INSN_NOP);
		codegen_node_pod(nth_param(n, 1)); //condition
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_JZ, 1);
		codegen_node(nth_param(n, 2)); //body
		ast_node_t *j=insert_insn_after_arg_eval(n, INSN_JMP, 2);
		j->value=h;
		ast_node_t *k=insert_insn_after_arg_eval(n, INSN_NOP, 2);
		i->value=k;
	} else if (n->type==AST_TYPE_TEQ) {
		handle_rhs_lhs_op_pod(n, INSN_TEQ);
	} else if (n->type==AST_TYPE_TNEQ) {
		handle_rhs_lhs_op_pod(n, INSN_TNEQ);
	} else if (n->type==AST_TYPE_TL) {
		handle_rhs_lhs_op_pod(n, INSN_TL);
	} else if (n->type==AST_TYPE_TLEQ) {
		handle_rhs_lhs_op_pod(n, INSN_TLEQ);
	} else if (n->type==AST_TYPE_FUNCDEF) {
		insert_insn_before_arg_eval(n, INSN_ENTER);
		codegen(n->children);
	} else if (n->type==AST_TYPE_FUNCDEFARG) {
		//na
	} else if (n->type==AST_TYPE_FUNCCALL) {
		if (n->children) codegen(n->children);
		ast_node_t *i=insert_insn_after_all_arg_eval(n, INSN_CALL);
		//note: this is the node of the fn, not of the 1st instr. We assign
		//that node the correct addr later.
		i->value=n->value; 
	} else if (n->type==AST_TYPE_FUNCCALLARG) {
		//na
	} else if (n->type==AST_TYPE_BLOCK) {
		insert_insn_before_arg_eval(n, INSN_SCOPE_ENTER);
		codegen(n->children);
		insert_insn_after_arg_eval(n, INSN_SCOPE_LEAVE, 1);
	} else if (n->type==AST_TYPE_MULTI) {
		codegen(n->children);
	} else if (n->type==AST_TYPE_DROP) {
		codegen_node(nth_param(n, 1));
		insert_insn_after_arg_eval(n, INSN_POP, 1);
	} else if (n->type==AST_TYPE_ASSIGN) {
		//Arg 1: address of thing to assign to
		//Arg 2: value to assign
		codegen_node_pod(nth_param(n, 1));
		codegen_node_pod(nth_param(n, 2));
		insert_insn_after_arg_eval(n, INSN_WR_VAR, 2);
	} else if (n->type==AST_TYPE_POST_ADD) {
		codegen_node_pod(nth_param(n, 1));
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_POST_ADD, 1);
		i->insn_arg=n->number;
	} else if (n->type==AST_TYPE_PRE_ADD) {
		codegen_node_pod(nth_param(n, 1));
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_PRE_ADD, 1);
		i->insn_arg=n->number;
	} else if (n->type==AST_TYPE_RETURN) {
		codegen_node_pod(nth_param(n, 1));
		insert_insn_after_arg_eval(n, INSN_RETURN, 1);
	} else if (n->type==AST_TYPE_DECLARE) {
		if (n->returns==AST_RETURNS_ARRAY) {
			ast_node_t *a=ast_find_type(n->children, AST_TYPE_ARRAYREF);
			assert(a && "No arrayref in returns->array declare?");
			ast_node_t *j=insert_insn_before_arg_eval(n, ast_is_local(n)?INSN_LEA:INSN_LEA_G);
			j->value=n;
			codegen_node_pod(nth_param(a, 1)); //size of the array, in items
			ast_node_t *i=insert_insn_after_arg_eval(n, INSN_ARRAYINIT, 1);
			i->insn_arg=a->size;
		} else if (n->returns==AST_RETURNS_STRUCT) {
			ast_node_t *s=ast_find_type(n->children, AST_TYPE_STRUCTREF);
			assert(s && "No structref in returns->struct declare?");
			ast_node_t *j=insert_insn_before_arg_eval(n, ast_is_local(n)?INSN_LEA:INSN_LEA_G);
			j->value=n;
			ast_node_t *i=insert_insn_before_arg_eval(n, INSN_STRUCTINIT);
			i->insn_arg=s->size;
		}
		//could have an assign as child
		ast_node_t *assign=ast_find_type(n->children, AST_TYPE_ASSIGN);
		if (assign) codegen_node(assign);
	} else if (n->type==AST_TYPE_LOCALSIZE) {
		//na, enter is resolved elsewhere
	} else if (n->type==AST_TYPE_INSN) {
		//ignore
	} else if (n->type==AST_TYPE_PROGRAM_START) {
		//ignore
	} else if (n->type==AST_TYPE_SYSCALL) {
		if (n->children) codegen(n->children);
		ast_node_t *i=insert_insn_after_all_arg_eval(n, INSN_SYSCALL);
		i->insn_arg=n->valpos; 
	} else if (n->type==AST_TYPE_FUNCPTR) {
		ast_node_t *i=insert_insn_before_arg_eval(n, INSN_PUSH_R);
		i->value=n->value;
	} else if (n->type==AST_TYPE_GOTO) {
		ast_node_t *j=insert_insn_before_arg_eval(n, INSN_JMP);
		j->value=n->value;
	} else if (n->type==AST_TYPE_STRUCTDEF) {
		//nothing
	} else if (n->type==AST_TYPE_STRUCTREF) {
		//n/a
	} else if (n->type==AST_TYPE_REF || n->type==AST_TYPE_DEREF) {
		codegen_node(nth_param(n, 1));
		ast_node_t *i;
		if (n->value->returns==AST_RETURNS_NUMBER) {
			i=insert_insn_before_arg_eval(n, ast_is_local(n->value)?INSN_LEA:INSN_LEA_G);
		} else {
			i=insert_insn_before_arg_eval(n, ast_is_local(n->value)?INSN_LDA:INSN_LDA_G);
		}
		i->value=n->value;
		if (n->type==AST_TYPE_DEREF) {
			insert_insn_after_all_arg_eval(n, INSN_DEREF);
		}
	} else if (n->type==AST_TYPE_ARRAYREF) {
		codegen_node_pod(nth_param(n, 1));
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_ARRAY_IDX, 1);
		i->insn_arg=n->size;
		codegen_node(nth_param(n, 2));
	} else if (n->type==AST_TYPE_STRUCTMEMBER) {
		//note: this can also happen in structdefs but since those aren't codegen'ned this
		//is never called for such a node.
		codegen_node(nth_param(n, 1));
		ast_node_t *j=insert_insn_before_arg_eval(n, INSN_PUSH_I);
		j->insn_arg=n->size;
		ast_node_t *i=insert_insn_before_arg_eval(n, INSN_STRUCT_IDX);
		i->insn_arg=n->value->size;
	} else {
		panic_error(n, "Eek! Unknown ast type %s (%d)", ast_type_str[n->type], n->type);
	}
}

void codegen(ast_node_t *node) {
	assert(node && "Codegen called with NULL arg");
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		codegen_node(n);
	}
}
