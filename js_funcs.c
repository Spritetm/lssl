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

	ast_ops_fix_parents(prognode);
	ast_ops_attach_symbol_defs(prognode);
	ast_ops_fix_parents(prognode);
	ast_ops_add_trailing_return(prognode);
	ast_ops_fix_parents(prognode);
	ast_ops_var_place(prognode);
	codegen(prognode);
	ast_ops_position_insns(prognode);
	ast_ops_fixup_enter_leave_return(prognode);
	ast_ops_remove_useless_ops(prognode);
	ast_ops_assign_addr_to_fndef_node(prognode);
	ast_ops_fixup_addrs(prognode);
	ast_dump(prognode);

	int bin_len;
	program=ast_ops_gen_binary(prognode, &bin_len);
//	yy_delete_buffer(YY_CURRENT_BUFFER, myscanner);
	yylex_destroy(myscanner);

	vm=lssl_vm_init(program, bin_len, 1024);
	lssl_vm_run_main(vm);
}


void get_led(int pos, float t, uint8_t *rgb) {
	led_syscalls_calculate_led(vm, pos, t);
	led_syscalls_get_rgb(&rgb[0], &rgb[1], &rgb[2]);
}

