#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "vm_syscall.h"
#include "vm_defs.h"
#include "vm.h"

#define SYSCALL_FUNCTION(name) static int32_t syscall_##name(lssl_vm_t *vm, int32_t *arg, int argct)

SYSCALL_FUNCTION(abs) {
	if (arg[0]<0) return -arg[0]; else return arg[0];
}

SYSCALL_FUNCTION(floor) {
	return arg[0]&0xffff0000;
}

SYSCALL_FUNCTION(ceil) {
	arg[0]+=0xffff;
	arg[0]=arg[0]&0xffff0000;
	return arg[0];
}

SYSCALL_FUNCTION(clamp) {
	int r=arg[0];
	if (r<arg[1]) r=arg[1];
	if (r>arg[2]) r=arg[2];
	return r;
}

SYSCALL_FUNCTION(sin) {
	float f=arg[0]/65536.0;
	return sin(f)*65536;
}

SYSCALL_FUNCTION(cos) {
	float f=arg[0]/65536.0;
	return cos(f)*65536;
}

SYSCALL_FUNCTION(tan) {
	float f=arg[0]/65536.0;
	return tan(f)*65536;
}

SYSCALL_FUNCTION(rand) {
	return (rand()%(arg[2]-arg[1]))+arg[1];
}

SYSCALL_FUNCTION(dumpstack) {
	lssl_vm_dump_stack(vm);
	return 0;
}

static const vm_syscall_list_entry_t builtin_syscalls[]={
	{"abs", syscall_abs, 1}, 
	{"floor", syscall_floor, 1}, 
	{"ceil", syscall_ceil, 1}, 
	{"clamp", syscall_clamp, 2}, 
	{"sin", syscall_sin, 1}, 
	{"cos", syscall_cos, 1}, 
	{"tan", syscall_tan, 1}, 
	{"rand", syscall_rand, 3},
	{"dump_stack", syscall_dumpstack, 0}
};

typedef struct syscall_list_t syscall_list_t;

struct syscall_list_t {
	int start;
	int count;
	const vm_syscall_list_entry_t *ent;
	syscall_list_t *next;
};

static syscall_list_t syscall_list_builtin={
	.start=0,
	.count=(sizeof(builtin_syscalls)/sizeof(builtin_syscalls[0])),
	.ent=builtin_syscalls,
	.next=NULL
};

static syscall_list_t *syscall_list_last=&syscall_list_builtin;

void vm_syscall_add_local_syscalls(const vm_syscall_list_entry_t *syscalls, int count) {
	syscall_list_t *l=calloc(sizeof(syscall_list_t), 1);
	l->start=syscall_list_last->start+syscall_list_last->count;
	l->count=count;
	l->next=syscall_list_last;
	l->ent=syscalls;
	syscall_list_last=l;
}

int vm_syscall_handle_for_name(const char *name) {
	syscall_list_t *l=syscall_list_last;
	while (l) {
		for (int i=0; i<l->count; i++) {
			if (strcmp(l->ent[i].name, name)==0) {
				return l->start+i;
			}
		}
		l=l->next;
	}
	return -1;
}


static vm_syscall_list_entry_t const *ent_for_handle(int handle) {
	syscall_list_t *l=syscall_list_last;
	while (l) {
		if (handle>=l->start && handle<(l->start+l->count)) {
			return &l->ent[handle - l->start];
		}
		l=l->next;
	}
	return NULL;
}

const char *vm_syscall_name(int handle) {
	const vm_syscall_list_entry_t *ent=ent_for_handle(handle);
	assert(ent && "Invalid syscall entry!");
	return ent->name;
}

int vm_syscall_arg_count(int handle) {
	const vm_syscall_list_entry_t *ent=ent_for_handle(handle);
	assert(ent && "Invalid syscall entry!");
	return ent->argct;
}

int32_t vm_syscall(lssl_vm_t *vm, int syscall, int32_t *arg, int argct) {
	const vm_syscall_list_entry_t *ent=ent_for_handle(syscall);
	assert(ent && "Invalid syscall entry!");
	assert(ent->argct==argct && "Invalid arg count for syscall!");
	return ent->fn(vm, arg, argct);
}

void vm_syscall_free() {
	syscall_list_t *l=syscall_list_last;
	while(l) {
		syscall_list_t *next=l->next;
		if (l!=&syscall_list_builtin) free(l);
		l=next;
	}
}
