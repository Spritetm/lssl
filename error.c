#include <stdio.h>
#include "error.h"
#include "parser.h"

void yyerror (const YYLTYPE *loc, ast_node_t **program, yyscan_t yyscanner, char const *msg) {
	printf("Error: %s on line %d col %d\n", msg, loc->first_line, loc->first_column);
}
