


#define ARG_NONE 0
#define ARG_INT 1
#define ARG_FR 2
#define ARG_VAR 3
#define ARG_LABEL 4
#define ARG_TARGET 5
#define ARG_FUNCTION 6

//We use some Deeper C Preprocessor Magic so we can keep the instruction defs in one
//place, and generate enums/structs from that. This keeps them from going out of
//sync.

#define LSSL_INSTRUCTIONS \
	LSSL_INS_ENTRY(NOP, ARG_NONE) \
	LSSL_INS_ENTRY(PUSH_I, ARG_INT) \
	LSSL_INS_ENTRY(ADD_FR_I, ARG_FR) \
	LSSL_INS_ENTRY(RD_VAR, ARG_VAR) \
	LSSL_INS_ENTRY(WR_VAR, ARG_VAR) \
	LSSL_INS_ENTRY(RD_ARR, ARG_VAR) \
	LSSL_INS_ENTRY(WR_ARR, ARG_VAR) \
	LSSL_INS_ENTRY(MUL, ARG_NONE) \
	LSSL_INS_ENTRY(DIV, ARG_NONE) \
	LSSL_INS_ENTRY(ADD, ARG_NONE) \
	LSSL_INS_ENTRY(SUB, ARG_NONE) \
	LSSL_INS_ENTRY(JMP, ARG_TARGET) \
	LSSL_INS_ENTRY(JNZ, ARG_TARGET) \
	LSSL_INS_ENTRY(JZ, ARG_TARGET) \
	LSSL_INS_ENTRY(VAR, ARG_INT) \
	LSSL_INS_ENTRY(VARI, ARG_NONE) \
	LSSL_INS_ENTRY(ENTER, ARG_INT) \
	LSSL_INS_ENTRY(LEAVE, ARG_INT) \
	LSSL_INS_ENTRY(RETURN, ARG_INT) \
	LSSL_INS_ENTRY(CALL, ARG_FUNCTION) \
	LSSL_INS_ENTRY(POP, ARG_NONE) \
	LSSL_INS_ENTRY(TEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TNEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TL, ARG_NONE) \
	LSSL_INS_ENTRY(TG, ARG_NONE) \
	LSSL_INS_ENTRY(TLEQ, ARG_NONE) \
	LSSL_INS_ENTRY(TGEQ, ARG_NONE)



#define LSSL_INS_ENTRY(ins, argtype) INSN_##ins,
enum lssl_instr_enum {
	LSSL_INSTRUCTIONS
};
#undef LSSL_INS_ENTRY

typedef struct {
	const char *op;
	int argtype;
} lssl_vm_op_t;

//defined in vm_defs.c
extern const lssl_vm_op_t lssl_vm_ops[];
