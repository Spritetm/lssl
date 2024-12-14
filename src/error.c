#include <stdio.h>
#include "error.h"
#include "parser.h"
#include "ast.h"
#include <stdarg.h>

void panic_error(ast_node_t *node, const char *fmt, ...) {
	ast_node_t *p=node;
	node->has_error=true;
	while (p->parent) p=p->parent;
	p->has_error=true;
//	ast_dump(p);
	printf("Error on line %d col %d: ", node->loc.first_line+1, node->loc.first_column);
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

void yyerror (const YYLTYPE *loc, ast_node_t **program, yyscan_t yyscanner, const char *fmt, ...) {
	printf("Error on line %d col %d: ", loc->first_line+1, loc->first_column);
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}
