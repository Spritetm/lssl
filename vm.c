#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "vm_defs.h"
#include "vm_syscall.h"

#define LSSL_VM_ERR_NONE 0
#define LSSL_VM_ERR_STACK_OVF 1
#define LSSL_VM_ERR_STACK_UDF 2
#define LSSL_VM_ERR_UNK_OP 3

typedef struct {
	uint8_t *progmem;
	int proglen;
	uint32_t *stack;
	int stack_size;
	int main_fn;
	int bp;
	int sp;
	int pc;
	int error;
} lssl_vm_t;


static uint16_t get_i16(uint8_t *p) {
	return p[0]|(p[1]<<8);
}

static uint32_t get_i32(uint8_t *p) {
	return p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24);
}

lssl_vm_t *lssl_vm_init(uint8_t *program, int prog_len, int stack_size_words) {
	uint32_t ver=get_i32(&program[0]);
	uint32_t glob_sz=get_i32(&program[4]);
	uint32_t initial_pc=get_i32(&program[8]);

	//version needs to match
	if (ver!=LSSL_VM_VER) return NULL;
	
	lssl_vm_t *ret=calloc(sizeof(lssl_vm_t), 1);
	if (!ret) goto error;

	ret->stack_size=stack_size_words;
	ret->stack=calloc(stack_size_words, sizeof(uint32_t));
	ret->sp=glob_sz;
	ret->bp=glob_sz;
	ret->main_fn=initial_pc;
	return ret;
error:
	if (ret) free(ret->stack);
	free(ret);
	return NULL;
}


static void push(lssl_vm_t *vm, int32_t val) {
	if (vm->sp>=vm->stack_size) {
		vm->error=LSSL_VM_ERR_STACK_OVF;
		return;
	}
	vm->stack[vm->sp++]=val;
}

static int32_t pop(lssl_vm_t *vm) {
	if (vm->sp<0) {
		vm->error=LSSL_VM_ERR_STACK_UDF;
		return 0;
	}
	return vm->stack[--vm->sp];
}

int32_t lssl_vm_run_function(lssl_vm_t *vm, uint32_t fn_handle, int argc, int32_t *argv, int *error) {
	int32_t ret=0;
	//push the arguments
	for (int i=0; i<argc; i++) push(vm, argv[i]);
	//fake call
	push(vm, vm->bp);
	push(vm, -1); //fake return address
	vm->pc=fn_handle;
	while(vm->error==LSSL_VM_ERR_NONE) {
		printf("PC %x\n", vm->pc);
		int op=vm->progmem[vm->pc++];
		//todo: bounds check
		int bytes=lssl_vm_argtypes[lssl_vm_ops[op].argtype].byte_size;
		int arg=0;
		if (bytes==2) {
			arg=vm->progmem[vm->pc++];
		} else if (bytes==3) {
			arg=get_i16(&vm->progmem[vm->pc]);
			vm->pc+=2;
		} else if (bytes==5) {
			arg=get_i32(&vm->progmem[vm->pc]);
			vm->pc+=4;
		}
		if (op==INSN_PUSH_I) {
			push(vm, arg<<16);
		} else if (op==INSN_PUSH_R) {
			push(vm, arg);
		} else if (op==INSN_RD_VAR) {
			push(vm, vm->stack[vm->bp+arg]);
		} else if (op==INSN_WR_VAR) {
			vm->stack[vm->bp+arg]=pop(vm);
		} else if (op==INSN_RD_G_VAR) {
			push(vm, vm->stack[arg]);
		} else if (op==INSN_WR_G_VAR) {
			vm->stack[arg]=pop(vm);
		} else if (op==INSN_RD_ARR) {
		} else if (op==INSN_WR_ARR) {
		} else if (op==INSN_RD_G_ARR) {
		} else if (op==INSN_WR_G_ARR) {
		} else if (op==INSN_MUL) {
			int32_t a=pop(vm);
			int32_t b=pop(vm);
			int64_t v=(int64_t)a*b;
			push(vm, v>>16);
		} else if (op==INSN_DIV) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			//todo: is this correct?
			int64_t v=(((int64_t)a)<<16)/b;
			push(vm, v);
		} else if (op==INSN_ADD) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, a+b);
		} else if (op==INSN_SUB) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, a-b);
		} else if (op==INSN_JMP) {
			vm->pc=arg;
		} else if (op==INSN_JNZ) {
			if (pop(vm)!=0) vm->pc=arg;
		} else if (op==INSN_JNZ) {
			if (pop(vm)==0) vm->pc=arg;
		} else if (op==INSN_ENTER) {
			vm->sp+=arg;
		} else if (op==INSN_LEAVE) {
			int32_t v=pop(vm);
			vm->sp-=arg;
			push(vm, v);
		} else if (op==INSN_RETURN) {
			int32_t v=pop(vm);
			vm->pc=pop(vm);
			vm->bp=pop(vm);
			vm->sp=vm->sp-arg;
			if (vm->pc==-1) { //fake return address we pushed before
				//end of called routine
				ret=v;
				break;
			} else {
				push(vm, v);
			}
		} else if (op==INSN_CALL) {
			push(vm, vm->bp);
			push(vm, vm->pc);
			vm->bp=vm->sp;
			vm->pc=arg;
		} else if (op==INSN_POP) {
			pop(vm);
		} else if (op==INSN_TEQ) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a==b));
		} else if (op==INSN_TNEQ) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a!=b));
		} else if (op==INSN_TL) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a<b));
		} else if (op==INSN_TG) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a>b));
		} else if (op==INSN_TLEQ) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a<=b));
		} else if (op==INSN_TGEQ) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a>=b));
		} else if (op==INSN_SYSCALL) {
			int args=vm_syscall_arg_count(arg);
			assert(args<=8);
			int32_t argv[8];
			for (int i=0; i<args; i++) argv[args-i-1]=pop(vm);
			push(vm, vm_syscall(arg, argv, args));
		} else {
			printf("Unknown opcode!\n");
			vm->error=LSSL_VM_ERR_UNK_OP;
		}
	}
	*error=vm->error;
	return ret;
}


void lssl_vm_run_main(lssl_vm_t *vm) {
	int error=0;
	lssl_vm_run_function(vm, vm->main_fn, 0, NULL, &error);
}

void lssl_vm_free(lssl_vm_t *vm) {
	if (vm) free(vm->stack);
	free(vm);
}
