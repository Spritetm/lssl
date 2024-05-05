#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdarg.h>
#include "lexer.h"
#include "lexer_gen.h"
#include "parser.h"
#include "ast_ops.h"
#include "codegen.h"
#include "led_syscalls.h"
#include "vm_syscall.h"
#include "vm.h"

#define TEST_DIR "tests"

#define ERR_OK 0
#define ERR_TEST_ERR 1
#define ERR_NO_EXPECTED_ERR 2
#define ERR_UNEXPECTED_ERR 3
#define ERR_RUNTIME 4
#define ERR_RESULT 5

int expected_error_line;
int expected_error_col;
int expected_error_got;
int unexpected_error_got;

int check_expected(const YYLTYPE *loc) {
	//hacky way to compare
	int pos_start=8000*loc->first_line+loc->first_column;
	int pos_end=8000*loc->last_line+loc->last_column;
	int pos=8000*expected_error_line+expected_error_col;
	if (pos>=pos_start && pos<=pos_end) {
		expected_error_got=1;
		return 1;
	} else {
		return 0;
	}
}

void panic_error(ast_node_t *node, const char *fmt, ...) {
	if (check_expected(&node->loc)) return;
	unexpected_error_got=1;
	printf("Error on line %d col %d: ", node->loc.first_line+1, node->loc.first_column);
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

void yyerror(const YYLTYPE *loc, ast_node_t **program, yyscan_t yyscanner, const char *fmt, ...) {
	if (check_expected(loc)) return;
	unexpected_error_got=1;
	printf("Error on line %d col %d: ", loc->first_line+1, loc->first_column);
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}


int run_test(char *code) {
	uint8_t *bin=NULL;
	int bin_len;
	lssl_vm_t *vm=NULL;
	ast_node_t *prognode=NULL;
	int ret=ERR_OK;

	char *err_pos=strstr(code, "//ERROR HERE");
	if (err_pos) {
		while (err_pos && *err_pos!='\n') err_pos++;
		if (!err_pos) {
			printf("Invalid ERROR HERE statement\n");
			ret=ERR_TEST_ERR;
			goto cleanup;
		}
		err_pos++; //skip past newline
		int col_pos=0;
		while (err_pos[col_pos]!='v' && err_pos[col_pos]!='\n') col_pos++;
		if (err_pos[col_pos]!='v') {
			printf("Invalid ERROR HERE statement\n");
			ret=ERR_TEST_ERR;
			goto cleanup;
		}
		int line_pos=0;
		char *p=code;
		while (p!=err_pos) {
			if (*p=='\n') line_pos++;
			p++;
		}
		line_pos++; //as the actual error is *under* the 'v'
		expected_error_col=col_pos;
		expected_error_line=line_pos;
		expected_error_got=0;
		printf("Expecting error on/around line %d col %d\n", line_pos+1, col_pos);
	}

	unexpected_error_got=0;
	yyscan_t myscanner=NULL;
	yylex_init(&myscanner);
	yy_scan_string(code, myscanner);
	prognode=ast_gen_program_start_node();
	yyparse(&prognode->sibling, myscanner);
	ast_ops_do_compile(prognode);
	yylex_destroy(myscanner);

	if (err_pos) {
		if (expected_error_got) {
			ret=ERR_OK;
			goto cleanup;
		}
	}
	if (unexpected_error_got) {
		ret=ERR_UNEXPECTED_ERR;
		goto cleanup;
	}

	bin=ast_ops_gen_binary(prognode, &bin_len);

	led_syscalls_clear();
	vm=lssl_vm_init(bin, bin_len, 65536);
	vm_error_t vm_err;
	int32_t vmret=lssl_vm_run_main(vm, &vm_err);
	if (vm_err.type) {
		const file_loc_t *loc=ast_lookup_loc_for_pc(prognode, vm_err.pc);
		if (!loc) {
			printf("Running main() resulted in an error at pc 0x%X, but file loc didn't resolve!\n", vm_err.pc);
			ret=ERR_RUNTIME;
			goto cleanup;
		} else {
			yyerror(loc, &prognode, NULL, "Runtime error %d", vm_err.type);
			if (err_pos && check_expected(loc)) {
				//We expected this error.
				ret=ERR_OK;
				goto cleanup;
			} else {
				printf("Running main() returned error %d @ PC=0x%X\n", vm_err.type, vm_err.pc);
				ret=ERR_RUNTIME;
				goto cleanup;
			}
		}
	} else if (vmret==65536*42) {
		printf("Ran main() succesfully, returned %f\n", vmret/65536.0);
	} else {
		printf("Ran main() succesfully but it returned %f\n", vmret/65536.0);
		ret=ERR_RESULT;
		goto cleanup;
	}


cleanup:
	lssl_vm_free(vm);
	ast_free_all(prognode);
	free(bin);
	return ret;
}

typedef struct {
	char *file;
	int result;
} result_t;

int main(int argc, char **argv) {
	result_t errors[1024];
	DIR *dir=opendir(TEST_DIR);
	if (!dir) {
		perror(TEST_DIR);
		exit(1);
	}
	led_syscalls_init();
	char code[1024*1024]={};
	int res=0;
	struct dirent *de;
	while ((de=readdir(dir))) {
		if (strlen(de->d_name)>5 && strcmp(&de->d_name[strlen(de->d_name)-5], ".lssl")==0) {
			char filename[512];
			snprintf(filename, sizeof(filename), "%s/%s", TEST_DIR, de->d_name);
			printf("Testing %s ...\n", filename);
			FILE *f=fopen(filename, "r");
			if (!f) {
				perror(filename);
				exit(1);
			}
			int len=fread(code, 1, sizeof(code), f);
			code[len]=0;
			fclose(f);
			int result=run_test(code);
			errors[res].file=strdup(de->d_name);
			errors[res].result=result;
			res++;
		}
	}
	const char *result_str[]={
		"OK", "Test condition malformed", "Expected error did not happen", 
		"Unexpected compilation error", "Runtime error", "Result did not match expectation"
	};
	int ok=0, fail=0;
	for (int i=0; i<res; i++) {
		if (errors[i].result==ERR_OK) ok++; else fail++;
		printf("%s: %s\n", errors[i].file, result_str[errors[i].result]);
	}
	printf(" *** Success: %d Fail %d *** \n", ok, fail);
}