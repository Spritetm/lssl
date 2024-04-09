CFLAGS=-ggdb -Wall

default: test

lexer.c lexer_gen.h: lexer.l
	flex --header-file=lexer_gen.h -o lexer.c  $^

parser.c parser_gen.h: parser.y
	bison --header=parser_gen.h --output=parser.c $^

test.o: lexer_gen.h parser_gen.h

test: test.o lexer.o parser.o vm_defs.o ast.o error.o ast_ops.o codegen.o
	$(CC) $(CFLAGS) -o $@  $^ -lm

clean:
	rm -f test.o lexer.o parser.o insn_buf.o vm_defs.o
	rm -f error.o ast_ops.o codegen.o ast.o
	rm -f parser.c parser_gen.h lexer.c lexer_gen.h
