#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "vm_syscall.h"
#include "vm_defs.h"
#include "vm.h"


LSSL_SYSCALL_FUNCTION(syscall_abs) {
	if (arg[0]<0) return -arg[0]; else return arg[0];
}

LSSL_SYSCALL_FUNCTION(syscall_floor) {
	return arg[0]&0xffff0000;
}

LSSL_SYSCALL_FUNCTION(syscall_ceil) {
	arg[0]+=0xffff;
	arg[0]=arg[0]&0xffff0000;
	return arg[0];
}

LSSL_SYSCALL_FUNCTION(syscall_clamp) {
	int r=arg[0];
	if (r<arg[1]) r=arg[1];
	if (r>arg[2]) r=arg[2];
	return r;
}

LSSL_SYSCALL_FUNCTION(syscall_sin) {
	float f=arg[0]/65536.0;
	return sin(f)*65536;
}

LSSL_SYSCALL_FUNCTION(syscall_cos) {
	float f=arg[0]/65536.0;
	return cos(f)*65536;
}

LSSL_SYSCALL_FUNCTION(syscall_tan) {
	float f=arg[0]/65536.0;
	return tan(f)*65536;
}

LSSL_SYSCALL_FUNCTION(syscall_rand) {
	//note this returns a real number
	return (rand()%(arg[1]-arg[0]))+arg[0];
}

LSSL_SYSCALL_FUNCTION(syscall_dumpstack) {
	lssl_vm_dump_stack(vm);
	return 0;
}


static const char header[]=
	"syscalldef abs(x);\n"
	"syscalldef floor(x);\n"
	"syscalldef ceil(x);\n"
	"syscalldef clamp(x, min, max);\n"
	"syscalldef sin(x);\n"
	"syscalldef cos(x);\n"
	"syscalldef tan(x);\n"
	"syscalldef rand(x);\n"
	"syscalldef dump_stack();\n";

static const vm_syscall_list_entry_t builtin_syscalls[]={
	{"abs", syscall_abs}, 
	{"floor", syscall_floor}, 
	{"ceil", syscall_ceil}, 
	{"clamp", syscall_clamp},
	{"sin", syscall_sin}, 
	{"cos", syscall_cos}, 
	{"tan", syscall_tan}, 
	{"rand", syscall_rand},
	{"dump_stack", syscall_dumpstack}
};

typedef struct syscall_list_t syscall_list_t;

struct syscall_list_t {
	char *name;
	int start;
	int count;
	const char *header;
	const vm_syscall_list_entry_t *ent;
	syscall_list_t *next;
};

static syscall_list_t syscall_list_builtin={
	.name="builtin",
	.start=0,
	.count=(sizeof(builtin_syscalls)/sizeof(builtin_syscalls[0])),
	.ent=builtin_syscalls,
	.header=header,
	.next=NULL
};

static syscall_list_t *syscall_list_last=&syscall_list_builtin;

void vm_syscall_add_local_syscalls(const char *name, const vm_syscall_list_entry_t *syscalls, 
									int count, const char *header) {
	syscall_list_t *l=calloc(sizeof(syscall_list_t), 1);
	l->name=strdup(name);
	l->start=syscall_list_last->start+syscall_list_last->count;
	l->count=count;
	l->next=syscall_list_last;
	l->ent=syscalls;
	l->header=header;
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

int vm_syscall_get_info(int idx, const char **name, const char **header) {
	syscall_list_t *l=syscall_list_last;
	for (int i=0; i<idx; i++) {
		if (!l->next) return 0;
		l=l->next;
	}
	*name=l->name;
	*header=l->header;
	return 1;
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

int32_t vm_syscall(lssl_vm_t *vm, int syscall, int32_t *arg) {
	const vm_syscall_list_entry_t *ent=ent_for_handle(syscall);
	assert(ent && "Invalid syscall entry!");
	return ent->fn(vm, arg);
}

void vm_syscall_free() {
	syscall_list_t *l=syscall_list_last;
	while(l) {
		syscall_list_t *next=l->next;
		if (l!=&syscall_list_builtin) {
			free(l->name);
			free(l);
		}
		l=next;
	}
	syscall_list_last->next=NULL;
}
