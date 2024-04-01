#pragma once
#include "insn_buf.h"

#define YYSTYPE token_t

typedef struct {
	int numberi;
	float numberf;
	char *str;
} token_t;


typedef void *yyscan_t;

#include "parser.h"

void yyerror (const YYLTYPE *loc, insn_buf_t *ibuf, yyscan_t yyscanner, char const *msg);


