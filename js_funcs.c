#include <stdio.h>
#include <stdlib.h>
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

