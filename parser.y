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
%token TOKEN_NUMBERF TOKEN_NUMBERI
%token TOKEN_VAR
%token TOKEN_STR
%token TOKEN_CURLOPEN TOKEN_CURLCLOSE
%token TOKEN_FOR TOKEN_WHILE TOKEN_IF
%token TOKEN_FUNCTION
%token TOKEN_EQ TOKEN_NEQ TOKEN_L TOKEN_G TOKEN_LEQ TOKEN_GEQ
%token TOKEN_RETURN

%define api.pure full
%parse-param {ast_node_t **program}
%param {yyscan_t scanner}
%locations
%define api.location.type {file_loc_t}

%union{
	int numberi;
	float numberf;
	char *str;
	ast_node_t *ast;
}

%type<numberi> TOKEN_NUMBERI
%type<numberf> TOKEN_NUMBERF
%type<str> TOKEN_STR
%type<ast> input input_line statement block funcdef stdaloneexpr
%type<ast> funcdefargs while_statement if_statement for_statement assignment
%type<ast> vardef expr compf factor br_term term func_call funccallargs
%type<ast> funcdefarg return_statement

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

input_line: statement TOKEN_EOL
| statement TOKEN_SEMICOLON
| block

block: TOKEN_CURLOPEN input TOKEN_CURLCLOSE {
		$$=ast_new_node(AST_TYPE_BLOCK, &@$);
		$$->children=$2;
		$2->parent=$$;
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


stdaloneexpr: expr {
		//eval the expr but ignore the result
		$$=ast_new_node(AST_TYPE_DROP, &@$);
		$$->children=$1;
	}

funcdef: TOKEN_FUNCTION TOKEN_STR TOKEN_LPAREN funcdefargs TOKEN_RPAREN TOKEN_CURLOPEN input TOKEN_CURLCLOSE {
		$$=ast_new_node(AST_TYPE_FUNCDEF, &@$);
		$$->name=strdup($2);
		$$->returns=AST_RETURNS_NUMBER; //functions can only return a number for now
		ast_add_child($$, $4);
		ast_add_child($$, $7);
	}

funcdefargs: %empty { $$=NULL; }
| funcdefarg
| funcdefargs TOKEN_COMMA funcdefarg {
	ast_add_sibling($1, $3);
	$$=$1;
}

funcdefarg: TOKEN_STR {
		$$=ast_new_node(AST_TYPE_FUNCDEFARG, &@$);
		$$->name=strdup($1);
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

assignment: TOKEN_STR TOKEN_ASSIGN expr {
		$$=ast_new_node(AST_TYPE_ASSIGN, &@$);
		$$->name=strdup($1);
		ast_add_child($$, $3);
	}

vardef: TOKEN_VAR TOKEN_STR {
		$$=ast_new_node(AST_TYPE_DECLARE, &@$);
		$$->name=strdup($2);
	}
| TOKEN_VAR TOKEN_STR TOKEN_ASSIGN expr {
		$$=ast_new_node(AST_TYPE_DECLARE, &@$);
		$$->name=strdup($2);

		ast_node_t *a=ast_new_node(AST_TYPE_ASSIGN, &@$);
		a->name=strdup($2);
		ast_add_sibling($$, a);
		ast_add_child(a, $4);
	}

return_statement: TOKEN_RETURN {
		$$=ast_new_node(AST_TYPE_RETURN, &@$);
		ast_node_t *a=ast_new_node(AST_TYPE_INT, &@$);
		a->numberi=0;
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

term: TOKEN_NUMBERI { 
		$$=ast_new_node(AST_TYPE_INT, &@$);
		$$->numberi=$1;
		$$->returns=AST_RETURNS_CONST;
	 }
| TOKEN_NUMBERF { 
		$$=ast_new_node(AST_TYPE_FLOAT, &@$);
		$$->numberf=$1;
		$$->returns=AST_RETURNS_CONST;
	}
| TOKEN_STR { 
		$$=ast_new_node(AST_TYPE_VAR, &@$);
		$$->name=strdup($1);
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






