#include "ast.h"
#include "parser.h"

void yyerror (const YYLTYPE *loc, ast_node_t **program, yyscan_t yyscanner, char const *msg);

