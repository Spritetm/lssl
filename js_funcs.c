#include <stdio.h>
#include <stdlib.h>
#include <emscripten.h>
#include <stdarg.h>
#include "lexer.h"
#include "lexer_gen.h"
#include "parser.h"
#include "ast_ops.h"
#include "codegen.h"
#include "led_syscalls.h"
#include "vm.h"

static lssl_vm_t *vm=NULL;
static uint8_t *program;

void init() {
	led_syscalls_init();
}

void recompile(char *code) {
	if (vm) {
		lssl_vm_free(vm);
		vm=NULL;
	}

	yyscan_t myscanner;
	yylex_init(&myscanner);
	yy_scan_string(code, myscanner);

	ast_node_t *prognode;

	yyparse(&prognode, myscanner);

	ast_ops_do_compile(prognode);

	int bin_len;
	program=ast_ops_gen_binary(prognode, &bin_len);
//	yy_delete_buffer(YY_CURRENT_BUFFER, myscanner);
	yylex_destroy(myscanner);

	vm=lssl_vm_init(program, bin_len, 1024);
	lssl_vm_run_main(vm);
}

uint8_t rgb[3];

uint8_t *get_led(int pos, float t) {
	if (!vm) {
		rgb[0]=0; rgb[1]=0; rgb[2]=128;
		return rgb;
	}
	led_syscalls_calculate_led(vm, pos, t);
	led_syscalls_get_rgb(&rgb[0], &rgb[1], &rgb[2]);
	return rgb;
}


#define CLASS_UNK 0
#define CLASS_OPER 1
#define CLASS_LANGFUNC 2
#define CLASS_NUMBER 3
#define CLASS_PUNCTUATION 4
#define CLASS_COMMENT 5

//Mapping between classes and tokens for syntax highlighting
int token_class[]={
	[TOKEN_EOL]=CLASS_COMMENT, //should technically get its own class...
	[TOKEN_PLUS]=CLASS_OPER, [TOKEN_MINUS]=CLASS_OPER, [TOKEN_SLASH]=CLASS_OPER, [TOKEN_TIMES]=CLASS_OPER, 
	[TOKEN_LPAREN]=CLASS_PUNCTUATION, [TOKEN_RPAREN]=CLASS_PUNCTUATION,
	[TOKEN_SEMICOLON]=CLASS_PUNCTUATION, [TOKEN_COMMA]=CLASS_PUNCTUATION,
	[TOKEN_PERIOD]=CLASS_PUNCTUATION, [TOKEN_ASSIGN]=CLASS_PUNCTUATION,
	[TOKEN_EQ]=CLASS_OPER, [TOKEN_NEQ]=CLASS_OPER, [TOKEN_L]=CLASS_OPER, [TOKEN_G]=CLASS_OPER, 
	[TOKEN_LEQ]=CLASS_OPER, [TOKEN_GEQ]=CLASS_OPER,
	[TOKEN_CURLOPEN]=CLASS_PUNCTUATION, [TOKEN_CURLCLOSE]=CLASS_PUNCTUATION,
	[TOKEN_NUMBERF]=CLASS_NUMBER, [TOKEN_NUMBERI]=CLASS_NUMBER, 
	[TOKEN_VAR]=CLASS_LANGFUNC, [TOKEN_FOR]=CLASS_LANGFUNC, 
	[TOKEN_WHILE]=CLASS_LANGFUNC, [TOKEN_IF]=CLASS_LANGFUNC, 
	[TOKEN_FUNCTION]=CLASS_LANGFUNC, [TOKEN_RETURN]=CLASS_LANGFUNC, 
};

typedef struct {
	uint32_t start;
	uint32_t end;
	uint32_t class;
	uint32_t pad;
} token_t;

token_t *tokenize_for_syntax_hl(char *code) {
	yyscan_t myscanner;
	yylex_init(&myscanner);
	yy_scan_string(code, myscanner);

	int tsz=1024;
	int tlen=0;
	token_t *tokens=calloc(tsz, sizeof(token_t));

	int token;
	union YYSTYPE tokeninfo={};
	YYLTYPE loc={};
	while ((token=yylex(&tokeninfo, &loc, myscanner))) {
		if (token<(sizeof(token_class)/sizeof(token_class[0]))) {
			int class=token_class[token];
			if (class!=CLASS_UNK) {
				if (tsz==tlen+1) {
					tsz+=1024;
					tokens=realloc(tokens, tsz*sizeof(token_t));
				}
				tokens[tlen].start=loc.pos_start;
				tokens[tlen].end=loc.pos_end;
				tokens[tlen].class=class;
				tokens[tlen].pad=token_class[token];
				tlen++;
			}
		}
	}
	//end marker
	tokens[tlen].start=1;
	tokens[tlen].end=0;

//	yy_delete_buffer(YY_CURRENT_BUFFER, myscanner);
	yylex_destroy(myscanner);

	return tokens;
}


/*
Note: the following functions replace those in error.c (which is not compiled in for
the emscripten build) to report errors back to the webpage.
*/

EM_JS(void, call_report_error, (int pos_start, int pos_end, char* msg), {
	var umsg=UTF8ToString(msg);
	report_error(pos_start, pos_end, umsg);
});

void panic_error(ast_node_t *node, const char *fmt, ...) {
//	ast_node_t *p=node;
//	while (p->parent) p=p->parent;
//	ast_dump(p);
	char *msg;
	va_list ap;
	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);
	call_report_error(node->loc.pos_start, node->loc.pos_end, msg);
	free(msg);
}

void yyerror (const YYLTYPE *loc, ast_node_t **program, yyscan_t yyscanner, const char *fmt, ...) {
	char *msg;
	va_list ap;
	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);
	call_report_error(loc->pos_start, loc->pos_end, msg);
	free(msg);
}



