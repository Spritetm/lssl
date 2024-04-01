#include <stdio.h>
#include "insn_buf.h"
#include "lexer.h"
#include "lexer_gen.h"
#include "parser.h"

int main(int argc, char **argv) {
	char buf[1024*1024];
	fread(buf, sizeof(buf), 1, stdin);

	yyscan_t myscanner;
	yylex_init(&myscanner);
//    struct yyguts_t * yyg = (struct yyguts_t*)myscanner;
    yy_scan_string(buf, myscanner);

	insn_buf_t *ibuf=insn_buf_create();

	yyparse(ibuf, myscanner);

	insn_buf_dump(ibuf);

//    yy_delete_buffer(YY_CURRENT_BUFFER, myscanner);
    yylex_destroy(myscanner);


}