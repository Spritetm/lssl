#include "ast.h"
#include "parser.h"

void panic_error(ast_node_t *node, const char *fmt, ...);
void yyerror (const YYLTYPE *loc, ast_node_t **program, yyscan_t yyscanner, const char *fmt, ...);

