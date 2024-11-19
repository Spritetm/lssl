SRC_BASE = lexer.c parser.c vm_defs.c ast.c ast_ops.c codegen.c
SRC_BASE += led_syscalls.c vm_syscall.c vm.c compile.c
SRC_LSSL = lssl.c error.c
SRC_TEST = test.c
SRC_JS = js_funcs.c

SRC_ALL = $(SRC_BASE) $(SRC_TEST) $(SRC_JS) $(SRC_LSSL)
DEPFLAGS = -MT $@ -MMD -MP

CFLAGS=-g3 -O1 -Wall $(DEPFLAGS)

default: test lssl lssl.js

lexer.c lexer_gen.h: lexer.l parser_gen.h
	flex --header-file=lexer_gen.h -o lexer.c  $^

parser.c parser_gen.h: parser.y
	bison --header=parser_gen.h --output=parser.c $^

lssl: $(SRC_BASE:.c=.o) $(SRC_LSSL:.c=.o)
	$(CC) $(CFLAGS) -o $@  $^ -lm

test: $(SRC_BASE:.c=.o) $(SRC_TEST:.c=.o)
	$(CC) $(CFLAGS) -o $@  $^ -lm


EMSCR_ARGS = -O2 -sEXPORTED_RUNTIME_METHODS=ccall,cwrap 
EMSCR_ARGS += -sEXPORTED_FUNCTIONS=_init,_recompile,_get_led,_tokenize_for_syntax_hl,_free,_frame_start
EMSCR_ARGS +=-sASSERTIONS=1 -sFILESYSTEM=0 -gsource-map --source-map-base=./

lssl.js: $(SRC_BASE) $(SRC_JS)
	emcc -o $@ $^ $(EMSCR_ARGS) -lm

clean:
	rm -f $(SRC_ALL:.c=.o) 
	rm -f $(SRC_ALL:.c=.d) 
	rm -f parser.c parser_gen.h lexer.c lexer_gen.h
	rm -f lssl test lssl.wasm lssl.wasm.map lssl.js

-include $(SRC_ALL:.c=.d)
