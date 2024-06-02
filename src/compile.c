#include "lexer.h"
#include "lexer_gen.h"
#include "parser.h"
#include "ast_ops.h"
#include "codegen.h"
#include "vm_syscall.h"
#include "vm.h"


static ast_node_t *last_node(ast_node_t *n) {
	while (n->sibling) n=n->sibling;
	return n;
}

ast_node_t *lssl_compile(const char *code) {
	ast_node_t *prognode=ast_gen_program_start_node();
	yyscan_t myscanner=NULL;
	yylex_init(&myscanner);
	prognode=ast_gen_program_start_node();
	ast_node_t *p=prognode;
	int i=0;
	const char *name;
	const char *header;
	while (vm_syscall_get_info(i, &name, &header)) {
		if (header) {
			yy_scan_string(header, myscanner);
			yyparse(&p->sibling, myscanner);
			p=last_node(p);
		}
		i++;
	}
	yy_scan_string(code, myscanner);
	yyparse(&p->sibling, myscanner);
	p=last_node(p);
	//add a newline in case program doesn't end in one
	yy_scan_string("\n", myscanner);
	yyparse(&p->sibling, myscanner);
	if (!prognode->sibling) {
		free(prognode);
		return NULL;
	}
	ast_ops_do_compile(prognode);
	yylex_destroy(myscanner);
	return prognode;
}
