#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "lexer_gen.h"
#include "parser.h"
#include "ast_ops.h"
#include "codegen.h"

int main(int argc, char **argv) {
	char buf[1024*1024];
	if (argc==1) {
		fread(buf, sizeof(buf), 1, stdin);
	} else {
		FILE *f=fopen(argv[1], "r");
		if (!f) {
			perror(argv[1]);
			exit(1);
		}
		fread(buf, sizeof(buf), 1, f);
		fclose(f);
	}

	yyscan_t myscanner;
	yylex_init(&myscanner);
//    struct yyguts_t * yyg = (struct yyguts_t*)myscanner;
    yy_scan_string(buf, myscanner);

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

	if (argc>=3) {
		int len;
		uint8_t *bin=ast_ops_gen_binary(prognode, &len);
		FILE *f=fopen(argv[2], "wb");
		if (!f) {
			perror(argv[2]);
			exit(1);
		}
		fwrite(bin, len, 1, f);
		fclose(f);
	}

//	insn_buf_fixup(ibuf);
//	insn_buf_dump(ibuf, buf);

//    yy_delete_buffer(YY_CURRENT_BUFFER, myscanner);
    yylex_destroy(myscanner);
}