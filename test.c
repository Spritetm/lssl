#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "lexer_gen.h"
#include "parser.h"
#include "ast_ops.h"
#include "codegen.h"
#include "led_syscalls.h"

int main(int argc, char **argv) {
	char buf[1024*1024]={};
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

	led_syscalls_init();
	yyscan_t myscanner;
	yylex_init(&myscanner);
    yy_scan_string(buf, myscanner);

	ast_node_t *prognode=NULL;

	yyparse(&prognode, myscanner);
	if (!prognode) {
		printf("Parser found error.\n");
		exit(1);
	}

	ast_ops_do_compile(prognode);

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