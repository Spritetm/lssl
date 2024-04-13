#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "vm_syscall.h"
#include "vm_defs.h"

#define SYSCALL_FUNCTION(name) int32_t syscall_##name(int32_t *arg, int argct)

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

SYSCALL_FUNCTION(mod) {
	return arg[0]%arg[1];
}

SYSCALL_FUNCTION(rand) {
	return (rand()%(arg[2]-arg[1]))+arg[1];
}

typedef int32_t (syscall_fn_t)(int32_t *args, int argct);

#define LSSL_SYSCALL_ENTRY(name, paramcount) syscall_##name,
static syscall_fn_t *syscall_list[]={
	LSSL_SYSCALLS
};
#undef LSSL_SYSCALL_ENTRY

int32_t vm_syscall(int syscall, int32_t *arg, int argct) {
	//check if implemented
	if (syscall<0 || syscall>(sizeof(syscall_list)/sizeof(syscall_list[0]))) return 0;
	
	return syscall_list[syscall](arg, argct);
}





