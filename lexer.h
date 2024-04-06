#pragma once
#include "insn_buf.h"
#include "ast.h"


//#define YYSTYPE ast_node_t*

typedef struct {
	int numberi;
	float numberf;
	char *str;
} token_t;


typedef void *yyscan_t;


typedef struct {
  int first_line;
  int first_column;
  int last_line;
  int last_column;
  int pos_start;
  int pos_end;
} file_loc_t;

#include "parser.h"

void yyerror (const YYLTYPE *loc, ast_node_t **program, yyscan_t yyscanner, char const *msg);



