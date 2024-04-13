CFLAGS=-ggdb -Wall

default: test vm lssl.js

lexer.c lexer_gen.h: lexer.l
	flex --header-file=lexer_gen.h -o lexer.c  $^

parser.c parser_gen.h: parser.y
	bison --header=parser_gen.h --output=parser.c $^

test.o: lexer_gen.h parser_gen.h

test: test.o lexer.o parser.o vm_defs.o ast.o error.o ast_ops.o codegen.o led_syscalls.o vm_syscall.o
	$(CC) $(CFLAGS) -o $@  $^ -lm

vm: vm_defs.o vm.o vm_syscall.o vm_runner.o led_syscalls.o
	$(CC) $(CFLAGS) -o $@  $^ -lm

EMSCR_ARGS=-O2 -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -sEXPORTED_FUNCTIONS=_compile,_run -sASSERTIONS=1 -sFILESYSTEM=0


lssl.js: lexer.c parser.c vm_defs.c ast.c error.c ast_ops.c codegen.c vm.c vm_syscall.c js_funcs.c led_syscalls.c
	emcc -o $@ $^ $(EMSCR_ARGS) -lm

clean:
	rm -f test.o lexer.o parser.o insn_buf.o vm_defs.o
	rm -f error.o ast_ops.o codegen.o ast.o vm.o
	rm -f parser.c parser_gen.h lexer.c lexer_gen.h
