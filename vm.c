#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "vm_defs.h"
#include "vm_syscall.h"
#include "vm.h"

struct lssl_vm_t {
	uint8_t *progmem;
	int proglen;
	int32_t *stack;
	int stack_size;
	int bp;
	int sp;
	int pc;
	int error;
};

static int8_t get_i8(uint8_t *p) {
	return p[0];
}

static int16_t get_i16(uint8_t *p) {
	return p[0]|(p[1]<<8);
}

static int32_t get_i32(uint8_t *p) {
	return p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24);
}

lssl_vm_t *lssl_vm_init(uint8_t *program, int prog_len, int stack_size_words) {
	uint32_t ver=get_i32(&program[0]);
	uint32_t glob_sz=get_i32(&program[4]);

	//version needs to match
	if (ver!=LSSL_VM_VER) return NULL;
	
	lssl_vm_t *ret=calloc(sizeof(lssl_vm_t), 1);
	if (!ret) goto error;

	ret->progmem=&program[8];
	ret->stack_size=stack_size_words;
	ret->stack=calloc(stack_size_words, sizeof(uint32_t));
	ret->sp=glob_sz;
	ret->bp=glob_sz;
	return ret;
error:
	if (ret) free(ret->stack);
	free(ret);
	return NULL;
}


//static void dump_stack(lssl_vm_t *vm) {
//	for (int i=0; i<vm->sp; i++) printf("%02X (% 3d) %x\n", i, i-vm->sp, vm->stack[i]);
//}


static void push(lssl_vm_t *vm, int32_t val) {
	if (vm->sp>=vm->stack_size) {
		printf("Stack overflow at PC=0x%X (stack size %d)\n", vm->pc, vm->stack_size);
		vm->error=LSSL_VM_ERR_STACK_OVF;
		return;
	}
	vm->stack[vm->sp++]=val;
}

static int32_t pop(lssl_vm_t *vm) {
	if (vm->sp<0) {
		printf("Stack underflow at PC=0x%X\n", vm->pc);
		vm->error=LSSL_VM_ERR_STACK_UDF;
		return 0;
	}
	return vm->stack[--vm->sp];
}

int32_t lssl_vm_run_function(lssl_vm_t *vm, uint32_t fn_handle, int argc, int32_t *argv, vm_error_en *error) {
	int32_t ret=0;
	//push the arguments
	for (int i=0; i<argc; i++) push(vm, argv[i]);
	//fake call
	push(vm, vm->bp);
	push(vm, -1); //fake return address
	vm->pc=fn_handle;
	vm->bp=vm->sp;
	vm->error=0;
	while(vm->error==LSSL_VM_ERR_NONE) {
		int new_pc=vm->pc;
		int op=vm->progmem[new_pc++];
		//todo: bounds check
		int bytes=lssl_vm_argtypes[lssl_vm_ops[op].argtype].byte_size;
		int arg=0;
		if (bytes==2) {
			arg=get_i8(&vm->progmem[new_pc]);
			new_pc+=1;
		} else if (bytes==3) {
			arg=get_i16(&vm->progmem[new_pc]);
			new_pc+=2;
		} else if (bytes==5) {
			arg=get_i32(&vm->progmem[new_pc]);
			new_pc+=4;
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
			new_pc=arg;
		} else if (op==INSN_JNZ) {
			if (pop(vm)!=0) new_pc=arg;
		} else if (op==INSN_JZ) {
			if (pop(vm)==0) new_pc=arg;
		} else if (op==INSN_ENTER) {
			vm->sp+=arg;
		} else if (op==INSN_LEAVE) {
			int32_t v=pop(vm);
			vm->sp-=arg;
			push(vm, v);
		} else if (op==INSN_RETURN) {
			int32_t v=pop(vm);
			new_pc=pop(vm);
			vm->bp=pop(vm);
			vm->sp=vm->sp-arg;
			if (new_pc==-1) { //fake return address we pushed before
				//end of called routine
				ret=v;
				break;
			} else {
				push(vm, v);
			}
		} else if (op==INSN_CALL) {
			push(vm, vm->bp);
			push(vm, new_pc);
			vm->bp=vm->sp;
			new_pc=arg;
		} else if (op==INSN_POP) {
			pop(vm);
		} else if (op==INSN_TEQ) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a==b)?(1<<16):0);
		} else if (op==INSN_TNEQ) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a!=b)?(1<<16):0);
		} else if (op==INSN_TL) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a<b)?(1<<16):0);
		} else if (op==INSN_TG) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a>b)?(1<<16):0);
		} else if (op==INSN_TLEQ) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a<=b)?(1<<16):0);
		} else if (op==INSN_TGEQ) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a>=b)?(1<<16):0);
		} else if (op==INSN_SYSCALL) {
			int args=vm_syscall_arg_count(arg);
			assert(args<=8);
			int32_t argv[8];
			for (int i=0; i<args; i++) argv[args-i-1]=pop(vm);
			push(vm, vm_syscall(arg, argv, args));
		} else if (op==INSN_DUP) {
			int32_t v=pop(vm);
			push(vm, v);
			push(vm, v);
		} else if (op==INSN_ARRAYINIT || op==INSN_ARRAYINIT_G) {
			int size=pop(vm)>>16;
			int addr=(op==INSN_ARRAYINIT)?vm->bp+arg:arg;
			vm->stack[addr]=addr+1;
			vm->stack[addr+1]=size;
			for (int i=0; i<size; i++) vm->stack[addr+2+i]=0;
		} else if (op==INSN_RD_ARR || op==INSN_RD_G_ARR) {
			int pos=pop(vm)>>16;
			int addr=(op==INSN_RD_ARR)?vm->bp+arg:arg;
			int data_addr=vm->stack[addr];
			int size=vm->stack[data_addr];
			if (pos<0 || pos>size) {
				printf("Array out of bounds: %d @ pc 0x%x\n", pos, vm->pc);
				vm->error=LSSL_VM_ERR_ARRAY_OOB;
			} else {
				push(vm, vm->stack[data_addr+1+pos]);
			}
		} else if (op==INSN_WR_ARR || op==INSN_WR_G_ARR) {
			int32_t val=pop(vm);
			int pos=pop(vm)>>16;
			int addr=(op==INSN_WR_ARR)?vm->bp+arg:arg;
			int data_addr=vm->stack[addr];
			int size=vm->stack[data_addr];
			if (pos<0 || pos>size) {
				printf("Array out of bounds: %d @ pc 0x%x\n", pos, vm->pc);
				vm->error=LSSL_VM_ERR_ARRAY_OOB;
			} else {
				vm->stack[data_addr+1+pos]=val;
			}
		} else {
			printf("Unknown op %d @ pc 0x%x\n", op, vm->pc);
			vm->error=LSSL_VM_ERR_UNK_OP;
		}
		vm->pc=new_pc;
	}
	*error=vm->error;
	return ret;
}


void lssl_vm_run_main(lssl_vm_t *vm) {
	vm_error_en error=0;
	//Initialization and main start from pos 0.
	lssl_vm_run_function(vm, 0, 0, NULL, &error);
}

void lssl_vm_free(lssl_vm_t *vm) {
	if (vm) free(vm->stack);
	free(vm);
}
