#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "lexer_gen.h"
#include "parser.h"
#include "ast_ops.h"
#include "codegen.h"
#include "vm.h"


uint8_t *compile(char *code) {
	yyscan_t myscanner;
	yylex_init(&myscanner);
	yy_scan_string(code, myscanner);

	ast_node_t *prognode;

	yyparse(&prognode, myscanner);

	ast_ops_attach_symbol_defs(prognode);
	ast_ops_add_trailing_return(prognode);
	ast_ops_var_place(prognode);
	codegen(prognode);
	ast_ops_position_insns(prognode);
	ast_ops_fixup_enter_leave_return(prognode);
	ast_ops_remove_useless_ops(prognode);
	ast_ops_assign_addr_to_fndef_node(prognode);
	ast_ops_fixup_addrs(prognode);
	ast_dump(prognode);

	int bin_len;
	uint8_t *bin=ast_ops_gen_binary(prognode, &bin_len);
//    yy_delete_buffer(YY_CURRENT_BUFFER, myscanner);
    yylex_destroy(myscanner);
	return bin;
}

void run(uint8_t *program) {

}


