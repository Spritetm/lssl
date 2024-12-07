#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
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
	int ap;
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
	if (ver!=LSSL_VM_VER) {
		printf("Version error! Expected %d got %d\n", LSSL_VM_VER, (int)ver);
		return NULL;
	}
	
	lssl_vm_t *ret=calloc(sizeof(lssl_vm_t), 1);
	if (!ret) goto error;

	ret->progmem=&program[8];
	ret->proglen=prog_len;
	ret->stack_size=stack_size_words;
	ret->stack=calloc(stack_size_words, sizeof(uint32_t));
	ret->sp=glob_sz;
	ret->bp=0;
	return ret;
error:
	if (ret) free(ret->stack);
	free(ret);
	return NULL;
}


/*
Stack layout:
0: global var A
1: global var B
....
N: global var N
N+1..M: global array/struct X
M+1..O: global array/struct Y

fn1 BP
fn1 return PC
<--- BP
local var A
local var B
allocated array/struct C
<--- AP
allocated array/struct D in restricted scope
<--- SP

*/

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

inline static uint32_t make_addr(int pos, int size) {
	return (pos<<16)|size;
}

inline static int pos_from_addr(uint32_t addr) {
	return (addr>>16);
}

inline static int size_from_addr(uint32_t addr) {
	return (addr&0xffff);
}

//Runs until error, or until we return to address -1.
int32_t lssl_vm_run(lssl_vm_t *vm, vm_error_t *error) {
	int32_t ret=0;
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
		} else if (op==INSN_MUL) {
			int32_t a=pop(vm);
			int32_t b=pop(vm);
			int64_t v=(int64_t)a*(int64_t)b;
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
		} else if (op==INSN_LAND) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a&&b)?(1<<16):0);
		} else if (op==INSN_LOR) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, (a||b)?(1<<16):0);
		} else if (op==INSN_BAND) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, a&b);
		} else if (op==INSN_BOR) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, a|b);
		} else if (op==INSN_BXOR) {
			int32_t b=pop(vm);
			int32_t a=pop(vm);
			push(vm, a^b);
		} else if (op==INSN_BNOT) {
			int32_t a=pop(vm);
			push(vm, ~a);
		} else if (op==INSN_LNOT) {
			int32_t a=pop(vm);
			push(vm, (a)?0:(1<<16));
		} else if (op==INSN_JMP) {
			new_pc=arg;
		} else if (op==INSN_JNZ) {
			if (pop(vm)!=0) new_pc=arg;
		} else if (op==INSN_JZ) {
			if (pop(vm)==0) new_pc=arg;
		} else if (op==INSN_ENTER) {
			vm->sp+=arg;
		} else if (op==INSN_RETURN) {
			int32_t v=pop(vm);
			vm->sp=vm->bp; //clear local vars, local objs
			new_pc=pop(vm);
			vm->bp=pop(vm);
			vm->ap=pop(vm);
			vm->sp=vm->sp-arg;
			if (new_pc==-1) { //fake return address we pushed before
				//end of called routine
				ret=v;
				break;
			} else {
				push(vm, v);
			}
		} else if (op==INSN_CALL) {
			push(vm, vm->ap);
			push(vm, vm->bp);
			push(vm, new_pc);
			vm->bp=vm->sp;
			vm->ap=vm->sp;
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
		} else if (op==INSN_POST_ADD) {
			int32_t addr=pop(vm);
			assert(size_from_addr(addr)==1 && "Huh? POST_ADD to addr with size != 1");
			push(vm, vm->stack[pos_from_addr(addr)]);
			vm->stack[pos_from_addr(addr)]+=arg;
		} else if (op==INSN_PRE_ADD) {
			int32_t addr=pop(vm);
			assert(size_from_addr(addr)==1 && "Huh? POST_ADD to addr with size != 1");
			vm->stack[pos_from_addr(addr)]+=arg;
			push(vm, vm->stack[pos_from_addr(addr)]);
		} else if (op==INSN_SYSCALL) {
			int args=(arg>>12);	//argument count
			int no=(arg&0xfff);	//syscall number
			int32_t argv[16];
			for (int i=0; i<args; i++) argv[args-i-1]=pop(vm);
			push(vm, vm_syscall(vm, no, argv));
		} else if (op==INSN_DUP) {
			int32_t v=pop(vm);
			push(vm, v);
			push(vm, v);
		} else if (op==INSN_SCOPE_ENTER) {
			push(vm, vm->ap);
			vm->ap=vm->sp;
		} else if (op==INSN_SCOPE_LEAVE) {
			vm->sp=vm->ap;
			vm->ap=pop(vm);
		} else if (op==INSN_LEA) {
			int addr=vm->bp+arg;
			push(vm, make_addr(addr, 1));
		} else if (op==INSN_LEA_G) {
			int addr=arg;
			push(vm, make_addr(addr, 1));
		} else if (op==INSN_LDA) {
			int addr=vm->bp+arg;
			push(vm, vm->stack[addr]);
		} else if (op==INSN_LDA_G) {
			int addr=arg;
			push(vm, vm->stack[addr]);
		} else if (op==INSN_ARRAYINIT) {
			int count=(pop(vm)>>16);
			uint32_t addr=pop(vm);
			//printf("array_init %d to 0x%x\n",pos_from_addr(addr),make_addr(vm->ap, arg*count));
			vm->stack[pos_from_addr(addr)]=make_addr(vm->sp, arg*count);
			vm->sp+=count*arg;
		} else if (op==INSN_STRUCTINIT) {
			uint32_t addr=pop(vm);
			vm->stack[pos_from_addr(addr)]=make_addr(vm->sp, arg);
			vm->sp+=arg;
		} else if (op==INSN_ARRAY_IDX) {
			int idx=(pop(vm)>>16);
			uint32_t addr=pop(vm);
			//printf("array_idx: addr 0x%X\n", addr);
			if (idx*arg>=size_from_addr(addr) || idx<0) {
				printf("Array out of bounds! Idx is %d, size is %d items of %d. PC=0x%X\n", idx, size_from_addr(addr)/arg, size_from_addr(addr), vm->pc);
				vm->error=LSSL_VM_ERR_ARRAY_OOB;
			}
			addr=make_addr(pos_from_addr(addr)+idx*arg, arg);
			//printf("array_idx: out addr 0x%X\n", addr);
			push(vm, addr);
		} else if (op==INSN_STRUCT_IDX) {
			uint32_t addr=pop(vm);
			int offset=(pop(vm)>>16);
			addr=make_addr(pos_from_addr(addr)+offset, arg);
			//printf("struct_idx: out addr 0x%X\n", addr);
			push(vm, addr);
		} else if (op==INSN_WR_VAR) {
			uint32_t val=pop(vm);
			uint32_t addr=pop(vm);
			//printf("wr_var addr %x @ pc %x\n", addr, vm->pc);
			assert(size_from_addr(addr)==1 && "Huh? WR_VAR to addr with size != 1");
			vm->stack[pos_from_addr(addr)]=val;
		} else if (op==INSN_DEREF) {
			uint32_t addr=pop(vm);
			//printf("deref addr %x @ pc %x\n", addr, vm->pc);
			assert(size_from_addr(addr)==1 && "Huh? DEREF to addr with size != 1");
			push(vm, vm->stack[pos_from_addr(addr)]);
		} else {
			printf("Unknown op %s (%d) @ pc 0x%x\n", lssl_vm_ops[op].op, op, vm->pc);
			vm->error=LSSL_VM_ERR_UNK_OP;
		}
		if (new_pc<0 || new_pc>=vm->proglen) {
			printf("Aiee! Op at pc=0x%X wants to go to 0x%X!\n", vm->pc, new_pc);
			vm->error=LSSL_VM_ERR_INTERNAL;
		}
		if (!vm->error) vm->pc=new_pc;
	}
	error->type=vm->error;
	error->pc=vm->pc;
	return ret;
}

int32_t lssl_vm_run_function(lssl_vm_t *vm, uint32_t fn_handle, int argc, int32_t *argv, vm_error_t *error) {
	int old_sp=vm->sp;
	//push the arguments
	for (int i=0; i<argc; i++) push(vm, argv[i]);
	//fake call
	push(vm, vm->ap);
	push(vm, vm->bp);
	push(vm, -1); //fake return address
	vm->pc=fn_handle;
	vm->bp=vm->sp;
	vm->ap=vm->sp;
	int32_t v=lssl_vm_run(vm, error);
	if (error->type==LSSL_VM_ERR_NONE && old_sp!=vm->sp) {
		printf("Aiee! SP before and after calling fn doesn't match. Before 0x%x after 0x%x\n", old_sp, vm->sp);
		error->type=LSSL_VM_ERR_INTERNAL;
	}
	return v;
}

void lssl_vm_dump_stack(lssl_vm_t *vm) {
	printf("SP %x BP %x AP %x\n", vm->sp, vm->bp, vm->ap);
	printf("Addr\tValue\n");
	for (int i=0; i<vm->sp; i++) {
		printf("%04x\t%08"PRIx32, i, vm->stack[i]);
		if (i==vm->bp) printf(" <-BP");
		if (i==vm->ap) printf(" <-AP");
		printf("\n");
	}
}


int32_t lssl_vm_run_main(lssl_vm_t *vm, vm_error_t *error) {
	return lssl_vm_run_function(vm, 0, 0, NULL, error);
}

void lssl_vm_free(lssl_vm_t *vm) {
	if (vm) free(vm->stack);
	free(vm);
}
