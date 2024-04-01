%{
#include <stdio.h>
#include <math.h>
#include "lexer.h"
#include "lexer_gen.h"
#include "insn_buf.h"
%}

%token TOKEN_EOL
%token TOKEN_PLUS
%token TOKEN_MINUS
%token TOKEN_TIMES
%token TOKEN_SLASH
%token TOKEN_LPAREN
%token TOKEN_RPAREN
%token TOKEN_SEMICOLON
%token TOKEN_COMMA
%token TOKEN_PERIOD
%token TOKEN_ASSIGN
%token TOKEN_EQ
%token TOKEN_NEQ
%token TOKEN_LSS
%token TOKEN_GTR
%token TOKEN_LEQ
%token TOKEN_GEQ
%token TOKEN_NUMBERF
%token TOKEN_NUMBERI
%token TOKEN_VAR
%token TOKEN_STR

%define api.pure full
%parse-param {insn_buf_t *ibuf}
%param {yyscan_t scanner}
%locations

%%

input: %empty
| input statement TOKEN_EOL
| input statement TOKEN_SEMICOLON


statement: vardef
| exp
| assignment


assignment: TOKEN_STR TOKEN_ASSIGN exp {
	int n=insn_buf_find_var(ibuf, $1.str);
	if (n<0) {
		yyerror(&yyloc, ibuf, scanner, "Undefined variable");
		YYERROR;
	}
	insn_buf_add_ins(ibuf, INSN_WR_VAR, n);
}

vardef: TOKEN_VAR TOKEN_STR {
		insn_buf_add_var(ibuf, $2.str, 1);
		free($2.str);
		$2.str=NULL;
	}


exp: factor
| exp TOKEN_PLUS factor { insn_buf_add_ins(ibuf, INSN_ADD, 0); }
| exp TOKEN_MINUS factor { insn_buf_add_ins(ibuf, INSN_SUB, 0); }
;

factor: term
| factor TOKEN_TIMES term { insn_buf_add_ins(ibuf, INSN_MUL, 0); }
| factor TOKEN_SLASH term { insn_buf_add_ins(ibuf, INSN_DIV, 0); }

term: TOKEN_NUMBERF { 
		insn_buf_add_ins(ibuf, INSN_PUSH_I, (int)($$.numberf));
		insn_buf_add_ins(ibuf, INSN_ADD_FR_I, (int)(fmod($$.numberf, 1)*65536.0));
	}
| TOKEN_NUMBERI { insn_buf_add_ins(ibuf, INSN_PUSH_I, $$.numberi); }


