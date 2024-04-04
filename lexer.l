%{
    #include <stdlib.h>
    #include <string.h>
	#include "lexer.h"
	#include "parser.h"
	#include "insn_buf.h"

    void yyerror (const YYLTYPE *loc, insn_buf_t *ibuf, yyscan_t yyscanner, char const *msg) {
		printf("Error: %s on line %d col %d\n", msg, loc->first_line, loc->first_column);
	}

%}

%option warn nodefault
%option reentrant bison-bridge
%option bison-locations
%option yylineno
%option noinput
%option nounput


digit		[0-9]
hexdigit	[0-9A-Fa-f]

%%

"//".*"\n"				{ return TOKEN_EOL;			}
"\n"					{ return TOKEN_EOL;			}
"+"						{ return TOKEN_PLUS;		}
"-"						{ return TOKEN_MINUS;		}
"*"						{ return TOKEN_TIMES;		}
"/"						{ return TOKEN_SLASH;		}
"("						{ return TOKEN_LPAREN;		}
")"						{ return TOKEN_RPAREN;		}
";"						{ return TOKEN_SEMICOLON;	}
","						{ return TOKEN_COMMA;	}
"."						{ return TOKEN_PERIOD;		}
"="						{ return TOKEN_ASSIGN;		}
"=="					{ return TOKEN_EQ;	}
"<>"					{ return TOKEN_NEQ;		}
"<"						{ return TOKEN_LSS;		}
">"						{ return TOKEN_GTR;		}
"<="					{ return TOKEN_LEQ;		}
">="					{ return TOKEN_GEQ;		}
"{"						{ return TOKEN_CURLOPEN;	}
"}"						{ return TOKEN_CURLCLOSE;	}
{digit}+"."{digit}+		{ yylval->numberf=strtof(yytext, NULL); return TOKEN_NUMBERF; }
"0x"{hexdigit}+			{ yylval->numberi=strtol(yytext, NULL, 16); return TOKEN_NUMBERI; }
{digit}+			 	{ yylval->numberi=strtol(yytext, NULL, 10); return TOKEN_NUMBERI; }
"var"				 	{ return TOKEN_VAR;		}
"for"					{ return TOKEN_FOR;		}
"while"					{ return TOKEN_WHILE;	}
"function"				{ return TOKEN_FUNCTION;}
[a-zA-Z0-9_]+			{ yylval->str=strdup(yytext); return TOKEN_STR; }

" "
\t
.						{ printf("bad input character '%s' at line %d\n", yytext, yylineno); }

%%

int yywrap(yyscan_t yyscanner) { return 1; }

