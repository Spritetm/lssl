#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ast.h"
#include "codegen.h"
#include "error.h"

/*
Note: This adds instructions to within the AST. This has the downside that 
we cannot switch e.g. args around compared to the AST.
*/

static void codegen_node(ast_node_t *n);

//Inserts an instruction before the arg nodes, but after the other instructions.
static ast_node_t *insert_insn_before_arg_eval(ast_node_t *parent, lssl_instr_enum type) {
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
			p->type==AST_TYPE_LOCALSIZE || 
			p->type==AST_TYPE_DATATYPE) {
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
static ast_node_t *insert_insn_after_arg_eval(ast_node_t *parent, lssl_instr_enum type, int argnum) {
	ast_node_t *n=ast_new_node(AST_TYPE_INSN, &parent->loc);
	n->insn_type=type;
	if (parent->children==NULL) {
		//wut, no args? Hm.
		parent->children=n;
	} else {
		ast_node_t *l=nth_param(parent, argnum);
		if (l==NULL) {
			free(n);
			return NULL;
		}
		//skip over insns
		while (l->sibling!=NULL && l->sibling->type==AST_TYPE_INSN) l=l->sibling;
		//insert there
		n->sibling=l->sibling;
		l->sibling=n;
	}
	return n;
}

static ast_node_t *insert_insn_after_all_arg_eval(ast_node_t *parent, lssl_instr_enum type){
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


void handle_rhs_lhs_op(ast_node_t *node, lssl_instr_enum type) {
	codegen_node(nth_param(node, 1));
	codegen_node(nth_param(node, 2));
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
		handle_rhs_lhs_op(n, INSN_ADD);
	} else if (n->type==AST_TYPE_MINUS) {
		handle_rhs_lhs_op(n, INSN_SUB);
	} else if (n->type==AST_TYPE_TIMES) {
		handle_rhs_lhs_op(n, INSN_MUL);
	} else if (n->type==AST_TYPE_DIVIDE) {
		handle_rhs_lhs_op(n, INSN_DIV);
	} else if (n->type==AST_TYPE_FOR) {
		codegen_node(nth_param(n, 1)); //initial
		codegen_node(nth_param(n, 2)); //condition
		codegen_node(nth_param(n, 3)); //increment
		codegen_node(nth_param(n, 4)); //body
		//dummy so we can jump back there
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_NOP, 1);
		//conditional jump to end
		ast_node_t *j=insert_insn_after_arg_eval(n, INSN_JZ, 2); 
		//jump to condition
		ast_node_t *k=insert_insn_after_arg_eval(n, INSN_JMP, 3);
		//nop for address placeholder
		ast_node_t *l=insert_insn_after_arg_eval(n, INSN_NOP, 3);
		j->value=l;
		k->value=i;
	} else if (n->type==AST_TYPE_IF) {
		codegen_node(nth_param(n, 1)); //condition
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_JZ, 1);
		codegen_node(nth_param(n, 2)); //body
		ast_node_t *j=insert_insn_after_arg_eval(n, INSN_NOP, 2);
		i->value=j;
	} else if (n->type==AST_TYPE_WHILE) {
		ast_node_t *h=insert_insn_before_arg_eval(n, INSN_NOP);
		codegen_node(nth_param(n, 1)); //condition
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_JZ, 1);
		codegen_node(nth_param(n, 2)); //body
		ast_node_t *j=insert_insn_after_arg_eval(n, INSN_JMP, 2);
		j->value=h;
		ast_node_t *k=insert_insn_after_arg_eval(n, INSN_NOP, 2);
		i->value=k;
	} else if (n->type==AST_TYPE_TEQ) {
		handle_rhs_lhs_op(n, INSN_TEQ);
	} else if (n->type==AST_TYPE_TNEQ) {
		handle_rhs_lhs_op(n, INSN_TNEQ);
	} else if (n->type==AST_TYPE_TL) {
		handle_rhs_lhs_op(n, INSN_TL);
	} else if (n->type==AST_TYPE_TLEQ) {
		handle_rhs_lhs_op(n, INSN_TLEQ);
	} else if (n->type==AST_TYPE_FUNCDEF) {
		insert_insn_before_arg_eval(n, INSN_ENTER);
		codegen(n->children);
	} else if (n->type==AST_TYPE_FUNCDEFARG) {
		//na
	} else if (n->type==AST_TYPE_FUNCCALL) {
		codegen(n->children);
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
	} else if (n->type==AST_TYPE_DROP) {
		codegen_node(nth_param(n, 1));
		insert_insn_after_arg_eval(n, INSN_POP, 1);
	} else if (n->type==AST_TYPE_ASSIGN) {
		//Arg 1: address of thing to assign to
		//Arg 2: value to assign
		codegen_node(nth_param(n, 1));
		codegen_node(nth_param(n, 2));
		insert_insn_after_arg_eval(n, INSN_WR_VAR, 2);
	} else if (n->type==AST_TYPE_POST_ADD || n->type==AST_TYPE_PRE_ADD) {
		codegen_node(nth_param(n, 1));
		ast_node_t *i=insert_insn_after_arg_eval(n, n->type==AST_TYPE_POST_ADD?INSN_POST_ADD:INSN_PRE_ADD, 1);
		i->insn_arg=n->number>>16;
	} else if (n->type==AST_TYPE_RETURN) {
		codegen_node(nth_param(n, 1));
		insert_insn_after_arg_eval(n, INSN_RETURN, 1);
	} else if (n->type==AST_TYPE_DECLARE) {
		if (n->returns==AST_RETURNS_ARRAY) {
			ast_node_t *a=ast_find_type(n->children, AST_TYPE_ARRAYREF);
			assert(a && "No arrayref in returns->array declare?");
			codegen_node(nth_param(a, 1)); //size of the array, in items
			ast_node_t *j=insert_insn_after_arg_eval(n, (n->parent)?INSN_LEA:INSN_LEA_G, 1);
			j->value=n;
			ast_node_t *i=insert_insn_after_arg_eval(n, INSN_ARRAYINIT, 1);
			i->insn_arg=a->size;
		} else if (n->returns==AST_RETURNS_STRUCT) {
			ast_node_t *s=ast_find_type(n->children, AST_TYPE_STRUCTREF);
			assert(s && "No structref in returns->struct declare?");
			ast_node_t *j=insert_insn_before_arg_eval(n, (n->parent)?INSN_LEA:INSN_LEA_G);
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
		codegen(n->children);
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
		//todo
	} else if (n->type==AST_TYPE_STRUCTMEMBER) {
		//note: this can also happen in structdefs but since those aren't codegen'ned this
		//is never called for such a node.
		//todo
	} else if (n->type==AST_TYPE_DEREF) {
		//Note: deref/ref need to do LEA for POD, RD_VAR (equivalent) for non-POD.
		codegen_node(nth_param(n, 1));
		ast_node_t *i=insert_insn_before_arg_eval(n, (n->value->parent)?INSN_LEA_G:INSN_LEA);
		i->value=n->value;
		insert_insn_after_arg_eval(n, INSN_DEREF, 1);
	} else if (n->type==AST_TYPE_REF) {
		codegen_node(nth_param(n, 1));
		ast_node_t *i=insert_insn_before_arg_eval(n, (n->value->parent)?INSN_LEA_G:INSN_LEA);
		i->value=n->value;
	} else if (n->type==AST_TYPE_DATATYPE) {
		//n/a
	} else if (n->type==AST_TYPE_ARRAYREF) {
		codegen_node(nth_param(n, 1));
		ast_node_t *i=insert_insn_after_arg_eval(n, INSN_ARRAY_IDX, 1);
		i->insn_arg=n->size;
	} else if (n->type==AST_TYPE_STRUCTMEMBER) {
		codegen_node(nth_param(n, 1));
		ast_node_t *i=insert_insn_before_arg_eval(n, INSN_STRUCT_IDX);
		i->insn_arg=n->size;
	} else {
		panic_error(n, "Eek! Unknown ast type %d\n", n->type);
	}
}

void codegen(ast_node_t *node) {
	assert(node && "Codegen called with NULL arg");
	for (ast_node_t *n=node; n!=NULL; n=n->sibling) {
		codegen_node(n);
	}
}