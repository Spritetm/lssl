#pragma once
#include "insn_buf.h"

#define YYSTYPE token_t

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

void yyerror (const YYLTYPE *loc, insn_buf_t *ibuf, yyscan_t yyscanner, char const *msg);



