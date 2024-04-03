%{
#include <stdio.h>
#include <math.h>
#include "lexer.h"
#include "lexer_gen.h"
#include "insn_buf.h"
%}

%token TOKEN_EOL
%token TOKEN_PLUS TOKEN_MINUS TOKEN_TIMES TOKEN_SLASH
%token TOKEN_LPAREN TOKEN_RPAREN
%token TOKEN_SEMICOLON
%token TOKEN_COMMA
%token TOKEN_PERIOD
%token TOKEN_ASSIGN
%token TOKEN_EQ TOKEN_NEQ TOKEN_LSS TOKEN_GTR TOKEN_LEQ TOKEN_GEQ
%token TOKEN_NUMBERF TOKEN_NUMBERI
%token TOKEN_VAR
%token TOKEN_STR
%token TOKEN_CURLOPEN TOKEN_CURLCLOSE
%token TOKEN_FOR TOKEN_WHILE
%token TOKEN_FUNCTION

%define api.pure full
%parse-param {insn_buf_t *ibuf}
%param {yyscan_t scanner}
%locations

%%

input: %empty
| input statement TOKEN_EOL
| input statement TOKEN_SEMICOLON
| input block


block: block_open input block_close

block_open: TOKEN_CURLOPEN {
		insn_buf_start_block(ibuf);
	}

block_close: TOKEN_CURLCLOSE {
		insn_buf_end_block(ibuf);
	}


statement: %empty
| vardef
| expr
| assignment
| funcdef
//| while_statement


funcdef: TOKEN_FUNCTION funcname TOKEN_LPAREN funcargs TOKEN_RPAREN TOKEN_CURLOPEN input func_end

funcname: TOKEN_STR {
		insn_buf_start_block(ibuf);
		insn_buf_name_block(ibuf, $$.str);
		insn_buf_add_ins(ibuf, INSN_ENTER, 0);
	}

funcargs: %empty
| funcarg
| funcargs TOKEN_COMMA funcarg

funcarg: TOKEN_STR {
	insn_buf_add_arg(ibuf, $1.str);
}

func_end: TOKEN_CURLCLOSE {
		insn_buf_add_return_if_needed(ibuf);
		insn_buf_end_block(ibuf);
	}



//while_statement: TOKEN_WHILE TOKEN_LPAREN expr TOKEN_RPAREN input {
//		int start_ip=insn_buf_get_cur_ip(ibuf);
		
//	}



//while_block_end: %empty {
//	insn_buf_add(ibuf, INSN_JMP
//}

assignment: TOKEN_STR TOKEN_ASSIGN expr {
		if (!insn_buf_add_ins_with_var(ibuf, INSN_WR_VAR, $1.str)) {
			yyerror(&yyloc, ibuf, scanner, "Undefined variable");
			YYERROR;
		}
	}

vardef: TOKEN_VAR TOKEN_STR {
		bool r=insn_buf_add_var(ibuf, $2.str, 1);
		if (!r) {
			yyerror(&yyloc, ibuf, scanner, "Variable already defined");
			YYERROR;
		}
		free($2.str);
		$2.str=NULL;
	}


expr: factor
| expr TOKEN_PLUS factor { insn_buf_add_ins(ibuf, INSN_ADD, 0); }
| expr TOKEN_MINUS factor { insn_buf_add_ins(ibuf, INSN_SUB, 0); }


factor: br_term
| factor TOKEN_TIMES br_term { insn_buf_add_ins(ibuf, INSN_MUL, 0); }
| factor TOKEN_SLASH br_term { insn_buf_add_ins(ibuf, INSN_DIV, 0); }

br_term: term
| TOKEN_LPAREN expr TOKEN_RPAREN

term: TOKEN_NUMBERF { 
		insn_buf_add_ins(ibuf, INSN_PUSH_I, (int)($$.numberf));
		insn_buf_add_ins(ibuf, INSN_ADD_FR_I, (int)(fmod($$.numberf, 1)*65536.0));
	}
| TOKEN_NUMBERI { insn_buf_add_ins(ibuf, INSN_PUSH_I, $$.numberi); }
| TOKEN_STR { 
		if (!insn_buf_add_ins_with_var(ibuf, INSN_RD_VAR, $1.str)) {
			yyerror(&yyloc, ibuf, scanner, "Undefined variable");
			YYERROR;
		}
	}
| func_call


func_call: TOKEN_STR TOKEN_LPAREN funcargs TOKEN_RPAREN {
		insn_buf_add_ins_with_label(ibuf, INSN_CALL, $1.str);
	}

funcargs: %empty
| expr
| funcargs TOKEN_COMMA expr






