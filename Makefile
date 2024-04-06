CFLAGS=-ggdb -Wall

default: test

lexer.c lexer_gen.h: lexer.l
	flex --header-file=lexer_gen.h -o lexer.c  $^

parser.c parser.h: parser.y
	bison --header=parser.h --output=parser.c $^

test.o: lexer_gen.h parser.h

test: test.o lexer.o parser.o vm_defs.o ast.o
	$(CC) $(CFLAGS) -o $@  $^ -lm

clean:
	rm -f test.o lexer.o parser.o insn_buf.o vm_defs.o
	rm -f parser.c parser.h lexer.c lexer_gen.h
