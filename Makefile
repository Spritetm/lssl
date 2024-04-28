SRC_BASE = lexer.c parser.c vm_defs.c ast.c error.c ast_ops.c codegen.c
SRC_BASE += led_syscalls.c vm_syscall.c vm.c
SRC_TEST = test.c
SRC_JS = js_funcs.c

SRC_ALL = $(SRCS_BASE) $(SRCS_TEST) $(SRCS_JS)
DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
-include $(DEPDIR)/($SRC_ALL:.c=.d)

CFLAGS=-ggdb -Wall $(DEPFLAGS)

default: test vm lssl.js

lexer.c lexer_gen.h: lexer.l
	flex --header-file=lexer_gen.h -o lexer.c  $^

parser.c parser_gen.h: parser.y
	bison --header=parser_gen.h --output=parser.c $^

test: $(SRCS_BASE:.c=.o) $(SRCS_TEST:.c=.o)
	$(CC) $(CFLAGS) -o $@  $^ -lm

#vm: vm_defs.o vm.o vm_syscall.o vm_runner.o led_syscalls.o
#	$(CC) $(CFLAGS) -o $@  $^ -lm

EMSCR_ARGS = -O2 -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -sEXPORTED_FUNCTIONS=_init,_recompile,_get_led,_tokenize_for_syntax_hl,_free
EMSCR_ARGS +=-sASSERTIONS=1 -sFILESYSTEM=0 -gsource-map --source-map-base=./

lssl.js: $(SRCS_BASE) $(SRCS_JS)
	emcc -o $@ $^ $(EMSCR_ARGS) -lm

clean:
	rm -f $(SRCS_BASE:.c=.o) 
	rm -f $(SRCS_JS:.c=.o)
	rm -f $(SRCS_TEST:.c=.o)
	rm -f parser.c parser_gen.h lexer.c lexer_gen.h
