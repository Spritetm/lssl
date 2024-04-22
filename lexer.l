%{
    #include <stdlib.h>
    #include <string.h>
	#include "lexer.h"
	#include "parser.h"

	//from https://stackoverflow.com/questions/656703/how-does-flex-support-bison-location-exactly
	#define YY_USER_ACTION \
		yylloc->pos_start = yylloc->pos_end; \
		yylloc->first_line = yylloc->last_line; \
		yylloc->first_column = yylloc->last_column; \
		for(int i = 0; yytext[i] != '\0'; i++) { \
			if(yytext[i] == '\n') { \
				yylloc->last_line++; \
				yylloc->last_column = 0; \
			} \
			else { \
				yylloc->last_column++; \
			} \
			yylloc->pos_end++; \
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
"++"					{ return TOKEN_PLUSPLUS;	}
"--"					{ return TOKEN_MINUSMINUS;	}
"*"						{ return TOKEN_TIMES;		}
"/"						{ return TOKEN_SLASH;		}
"("						{ return TOKEN_LPAREN;		}
")"						{ return TOKEN_RPAREN;		}
";"						{ return TOKEN_SEMICOLON;	}
","						{ return TOKEN_COMMA;	}
"."						{ return TOKEN_PERIOD;		}
"="						{ return TOKEN_ASSIGN;		}
"=="					{ return TOKEN_EQ;	}
"!>"					{ return TOKEN_NEQ;		}
"<"						{ return TOKEN_L;		}
">"						{ return TOKEN_G;		}
"<="					{ return TOKEN_LEQ;		}
">="					{ return TOKEN_GEQ;		}
"{"						{ return TOKEN_CURLOPEN;	}
"}"						{ return TOKEN_CURLCLOSE;	}
"["						{ return TOKEN_SQBOPEN;		}
"]"						{ return TOKEN_SQBCLOSE;	}
{digit}+"."{digit}+		{ (yylval)->number=strtof(yytext, NULL)*65536.0; return TOKEN_NUMBER; }
"0x"{hexdigit}+			{ (yylval)->number=strtol(yytext, NULL, 16)<<16; return TOKEN_NUMBER; }
{digit}+			 	{ (yylval)->number=strtol(yytext, NULL, 10)<<16; return TOKEN_NUMBER; }
"var"				 	{ return TOKEN_VAR;		}
"for"					{ return TOKEN_FOR;		}
"while"					{ return TOKEN_WHILE;	}
"if"					{ return TOKEN_IF;		}
"function"				{ return TOKEN_FUNCTION;}
"return"				{ return TOKEN_RETURN;	}
[a-zA-Z0-9_]+			{ (yylval)->str=strdup(yytext); return TOKEN_STR; }

" "
\t
.						{ printf("bad input character '%s' at line %d\n", yytext, yylineno); }

%%

int yywrap(yyscan_t yyscanner) { return 1; }

