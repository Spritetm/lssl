%{
#include <stdio.h>
#include <math.h>
#include "lexer.h"
#include "lexer_gen.h"
#include "ast.h"
#include "error.h"



# define YYLLOC_DEFAULT(Cur, Rhs, N)                      \
do                                                        \
  if (N)                                                  \
    {                                                     \
      (Cur).pos_start    = YYRHSLOC(Rhs, 1).pos_start;    \
      (Cur).first_line   = YYRHSLOC(Rhs, 1).first_line;   \
      (Cur).first_column = YYRHSLOC(Rhs, 1).first_column; \
      (Cur).last_line    = YYRHSLOC(Rhs, N).last_line;    \
      (Cur).last_column  = YYRHSLOC(Rhs, N).last_column;  \
      (Cur).pos_end      = YYRHSLOC(Rhs, N).pos_end;      \
    }                                                     \
  else                                                    \
    {                                                     \
      (Cur).first_line   = (Cur).last_line   =            \
        YYRHSLOC(Rhs, 0).last_line;                       \
      (Cur).first_column = (Cur).last_column =            \
        YYRHSLOC(Rhs, 0).last_column;                     \
      (Cur).pos_start   = (Cur).pos_end      =            \
        YYRHSLOC(Rhs, 0).pos_end;                         \
    }                                                     \
while (0)

%}


%token TOKEN_EOL
%token TOKEN_PLUS TOKEN_MINUS TOKEN_TIMES TOKEN_SLASH
%token TOKEN_LPAREN TOKEN_RPAREN
%token TOKEN_SEMICOLON
%token TOKEN_COMMA
%token TOKEN_PERIOD
%token TOKEN_ASSIGN
%token TOKEN_NUMBER
%token TOKEN_VAR
%token TOKEN_STR
%token TOKEN_CURLOPEN TOKEN_CURLCLOSE
%token TOKEN_SQBOPEN TOKEN_SQBCLOSE
%token TOKEN_FOR TOKEN_WHILE TOKEN_IF
%token TOKEN_FUNCTION
%token TOKEN_EQ TOKEN_NEQ TOKEN_L TOKEN_G TOKEN_LEQ TOKEN_GEQ
%token TOKEN_RETURN
%token TOKEN_PLUSPLUS TOKEN_MINUSMINUS
%token TOKEN_STRUCT

%define api.pure full
%parse-param {ast_node_t **program}
%param {yyscan_t scanner}
%locations
%define api.location.type {file_loc_t}

%union{
	int number;
	char *str;
	ast_node_t *ast;
}

%type<number> TOKEN_NUMBER
%type<str> TOKEN_STR
%type<ast> input input_line statement block funcdef stdaloneexpr
%type<ast> funcdefargs while_statement if_statement for_statement assignment
%type<ast> expr compf factor br_term term func_call funccallargs
%type<ast> funcdefarg return_statement 
%type<ast> structdef structmembers structmember 
%type<ast> dereference array_dereference var_deref var_ref
%type<ast> vardef vardef_pod vardef_nonpod
%%

program: input {
		*program=$1;
	}

input: %empty {
		$$=NULL;
	}
| input input_line {
		if ($2==NULL) {
			//Statement was empty.
			$$=$1;
		} else if ($1!=NULL) {
			//have earlier input, add as sibling
			ast_add_sibling($1, $2);
			$$=$1;
		} else {
			//earlier input is empty
			$$=$2;
		}
	}

statement_end: TOKEN_EOL | TOKEN_SEMICOLON | statement_end TOKEN_EOL | statement_end TOKEN_SEMICOLON

input_line: statement statement_end
| block

block: TOKEN_CURLOPEN input TOKEN_CURLCLOSE {
		$$=ast_new_node(AST_TYPE_BLOCK, &@$);
		$$->children=$2;
	}

statement: %empty {
		$$=NULL;
	}
| vardef
| stdaloneexpr
| assignment
| funcdef
| for_statement
| while_statement
| if_statement
| return_statement
| structdef


structdef: TOKEN_STRUCT TOKEN_STR TOKEN_CURLOPEN structmembers TOKEN_CURLCLOSE {
		$$=ast_new_node(AST_TYPE_STRUCTDEF, &@$);
		$$->name=strdup($2);
		$$->children=$4;
	}


structmembers: %empty {
		$$=NULL;
	}
| statement_end {
		$$=NULL;
	}
| structmembers structmember statement_end {
		if ($1) {
			$$=$1;
			ast_add_sibling($$, $2);
		} else {
			$$=$2;
		}
	}


structmember: TOKEN_VAR TOKEN_STR array_dereference {
		$$=ast_new_node(AST_TYPE_STRUCTMEMBER, &@$);
		$$->size=1;
		$$->name=strdup($2);
		if ($3) {
			$$->children=$3;
			$$->returns=$3->returns;
		} else {
			$$->returns=AST_RETURNS_NUMBER;
		}
	}
| TOKEN_STR TOKEN_STR array_dereference {
		$$=ast_new_node(AST_TYPE_STRUCTMEMBER, &@$);
		$$->size=1;
		$$->name=strdup($2);
		ast_node_t *a=ast_new_node(AST_TYPE_STRUCTREF, &@$);
		a->name=strdup($1);
		if ($3) {
			//Need to insert the struct at the end of the array chain
			ast_node_t *n=ast_find_deepest_arrayref($3);
			ast_add_child(n, a);
			ast_add_child($$, $3);
			$$->returns=$3->returns;
		} else {
			ast_add_child($$, a);
			$$->returns=AST_RETURNS_STRUCT;
		}
	}


stdaloneexpr: expr {
		//eval the expr but ignore the result
		$$=ast_new_node(AST_TYPE_DROP, &@$);
		$$->children=$1;
	}

funcdef: TOKEN_FUNCTION TOKEN_STR TOKEN_LPAREN funcdefargs TOKEN_RPAREN TOKEN_CURLOPEN input TOKEN_CURLCLOSE {
		$$=ast_new_node(AST_TYPE_FUNCDEF, &@$);
		$$->name=strdup($2);
		$$->returns=AST_RETURNS_NUMBER; //functions can only return a number for now
		if ($4) ast_add_child($$, $4);
		ast_add_child($$, $7);
	}

funcdefargs: %empty { 
		$$=NULL; 
	}
| funcdefarg
| funcdefargs TOKEN_COMMA funcdefarg {
		ast_add_sibling($1, $3);
		$$=$1;
	}

funcdefarg: vardef_nonpod {
		$$=$1;
		$$->type=AST_TYPE_FUNCDEFARG;
	}
| vardef_pod {
		$$=$1;
		$$->type=AST_TYPE_FUNCDEFARG;
	}

while_statement: TOKEN_WHILE TOKEN_LPAREN expr TOKEN_RPAREN input_line {
		$$=ast_new_node(AST_TYPE_WHILE, &@$);
		ast_add_child($$, $3);
		ast_add_child($$, $5);
	}

if_statement: TOKEN_IF TOKEN_LPAREN expr TOKEN_RPAREN input_line {
		$$=ast_new_node(AST_TYPE_IF, &@$);
		ast_add_child($$, $3);
		ast_add_child($$, $5);
	}

for_statement: TOKEN_FOR TOKEN_LPAREN statement TOKEN_SEMICOLON expr TOKEN_SEMICOLON statement TOKEN_RPAREN input_line {
		$$=ast_new_node(AST_TYPE_FOR, &@$);
		ast_add_child($$, $3);
		ast_add_child($$, $5);
		ast_add_child($$, $7);
		ast_add_child($$, $9);
	}

assignment: var_ref TOKEN_ASSIGN expr {
		$$=ast_new_node(AST_TYPE_ASSIGN, &@$);
		ast_add_child($$, $1);
		ast_add_child($$, $3);
	}

vardef: TOKEN_VAR TOKEN_STR TOKEN_ASSIGN expr {
		//ToDo: what if non-pod assignment?
		$$=ast_new_node(AST_TYPE_DECLARE, &@$);
		$$->name=strdup($2);
		$$->size=1;

		ast_node_t *a=ast_new_node(AST_TYPE_ASSIGN, &@$);
		a->name=strdup($2);
		a->returns=$4->returns;
		ast_add_child($$, a);
		ast_add_child(a, $4);
	}
| TOKEN_VAR vardef_pod {
		$$=$2;
	}
| vardef_nonpod {
		$$=$1;
	}

vardef_pod: TOKEN_STR array_dereference {
		$$=ast_new_node(AST_TYPE_DECLARE, &@$);
		$$->size=1;
		$$->name=strdup($1);
		if ($2) {
			$$->children=$2;
			$$->returns=$2->returns;
		} else {
			$$->returns=AST_RETURNS_NUMBER;
		}
	}

vardef_nonpod: TOKEN_STR TOKEN_STR array_dereference {
		$$=ast_new_node(AST_TYPE_DECLARE, &@$);
		$$->size=1;
		$$->name=strdup($2);
		ast_node_t *a=ast_new_node(AST_TYPE_STRUCTREF, &@$);
		a->name=strdup($1);
		if ($3) {
			//Need to insert the struct at the end of the array chain
			ast_node_t *n=ast_find_deepest_arrayref($3);
			ast_add_child(n, a);
			ast_add_child($$, $3);
			$$->returns=$3->returns;
		} else {
			ast_add_child($$, a);
			$$->returns=AST_RETURNS_STRUCT;
		}
	}
dereference: %empty {
		$$=NULL;
	}
| dereference TOKEN_SQBOPEN expr TOKEN_SQBCLOSE {
		ast_node_t *a=ast_new_node(AST_TYPE_ARRAYREF, &@$);
		ast_add_child(a, $3);
		if ($1) {
			ast_add_child(ast_find_deepest_ref($1), a);
			$$=$1;
		} else {
			$$=a;
		}
	}
| dereference TOKEN_PERIOD TOKEN_STR {
		ast_node_t *a=ast_new_node(AST_TYPE_STRUCTMEMBER, &@$);
		a->name=strdup($3);
		if ($1) {
			ast_add_child(ast_find_deepest_ref($1), a);
			$$=$1;
		} else {
			$$=a;
		}
	}

//var j=y[2].e[1].member;

array_dereference: %empty{
		$$=NULL;
	}
| array_dereference TOKEN_SQBOPEN expr TOKEN_SQBCLOSE {
		ast_node_t *a=ast_new_node(AST_TYPE_ARRAYREF, &@$);
		ast_add_child(a, $3);
		if ($1) {
			ast_add_child($1, a);
			$$=$1;
		} else {
			$$=a;
		}
	}


return_statement: TOKEN_RETURN {
		$$=ast_new_node(AST_TYPE_RETURN, &@$);
		ast_node_t *a=ast_new_node(AST_TYPE_NUMBER, &@$);
		a->number=0;
		ast_add_child($$, a);
}
| TOKEN_RETURN expr {
		$$=ast_new_node(AST_TYPE_RETURN, &@$);
		ast_add_child($$, $2);
}

expr: compf
| expr TOKEN_EQ compf {   $$=ast_new_node_2chld(AST_TYPE_TEQ, &@$, $1, $3); }
| expr TOKEN_NEQ compf {  $$=ast_new_node_2chld(AST_TYPE_TNEQ, &@$, $1, $3); }
| expr TOKEN_L compf {    $$=ast_new_node_2chld(AST_TYPE_TL, &@$, $1, $3); }
| expr TOKEN_G compf {    $$=ast_new_node_2chld(AST_TYPE_TL, &@$, $3, $1); }
| expr TOKEN_LEQ compf {  $$=ast_new_node_2chld(AST_TYPE_TLEQ, &@$, $1, $3); }
| expr TOKEN_GEQ compf {  $$=ast_new_node_2chld(AST_TYPE_TLEQ, &@$, $3, $1); }

compf: factor
| compf TOKEN_PLUS factor {  $$=ast_new_node_2chld(AST_TYPE_PLUS, &@$, $1, $3); }
| compf TOKEN_MINUS factor { $$=ast_new_node_2chld(AST_TYPE_MINUS, &@$, $1, $3); }

factor: br_term
| factor TOKEN_TIMES br_term { $$=ast_new_node_2chld(AST_TYPE_TIMES, &@$, $1, $3); }
| factor TOKEN_SLASH br_term { $$=ast_new_node_2chld(AST_TYPE_DIVIDE, &@$, $1, $3); }

br_term: term
| TOKEN_LPAREN expr TOKEN_RPAREN {
		$$=$2;
	}


//var_ref returns an *object pointer* to a variable or struct/arraymember
var_ref: TOKEN_STR dereference {
		$$=ast_new_node(AST_TYPE_REF, &@$);
		$$->name=strdup($1);
		if ($2) ast_add_child($$, $2);
		$$->returns=AST_RETURNS_NUMBER; //...we hope
}


//var_deref returns the *value* of a variable or struct/arraymember
var_deref: TOKEN_STR dereference {
		$$=ast_new_node(AST_TYPE_DEREF, &@$);
		$$->name=strdup($1);
		if ($2) ast_add_child($$, $2);
		$$->returns=AST_RETURNS_NUMBER; //...we hope
	}

term: TOKEN_NUMBER { 
		$$=ast_new_node(AST_TYPE_NUMBER, &@$);
		$$->number=$1;
		$$->returns=AST_RETURNS_CONST;
	 }
| var_deref
| var_deref TOKEN_PLUSPLUS {
		$$=ast_new_node(AST_TYPE_POST_ADD, &@$);
		$$->number=(1<<16);
		ast_add_child($$, $1);
		$$->returns=AST_RETURNS_NUMBER;
	}
| var_deref TOKEN_MINUSMINUS {
		$$=ast_new_node(AST_TYPE_POST_ADD, &@$);
		$$->number=-(1<<16);
		ast_add_child($$, $1);
		$$->returns=AST_RETURNS_NUMBER;
	}
| TOKEN_PLUSPLUS var_deref  {
		$$=ast_new_node(AST_TYPE_PRE_ADD, &@$);
		$$->number=(1<<16);
		ast_add_child($$, $2);
		$$->returns=AST_RETURNS_NUMBER;
	}
| TOKEN_MINUSMINUS var_deref  {
		$$=ast_new_node(AST_TYPE_PRE_ADD, &@$);
		$$->number=-(1<<16);
		ast_add_child($$, $2);
		$$->returns=AST_RETURNS_NUMBER;
	}
| func_call


func_call: TOKEN_STR TOKEN_LPAREN funccallargs TOKEN_RPAREN {
		$$=ast_new_node(AST_TYPE_FUNCCALL, &@$);
		$$->name=strdup($1);
		ast_add_child($$, $3);
		$$->returns=AST_RETURNS_NUMBER;
	}

funccallargs: %empty { $$=NULL; }
| expr
| funccallargs TOKEN_COMMA expr {
	ast_add_sibling($1, $3);
	$$=$1;
}






