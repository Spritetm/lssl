#pragma once

#include <stdbool.h>
#include "vm_defs.h"



typedef struct insn_buf_t insn_buf_t;
typedef struct insn_t insn_t;



insn_buf_t *insn_buf_create();
void insn_buf_start_block(insn_buf_t *buf);
void insn_buf_end_block(insn_buf_t *buf);
bool insn_buf_add_function(insn_buf_t *buf, insn_t *insn, const char *name);
bool insn_buf_add_arg(insn_buf_t *buf, const char *argname);
void insn_buf_add_ins(insn_buf_t *buf, int type, int val);
void insn_buf_add_ins_with_sym(insn_buf_t *buf, int type, const char *varname);
void insn_buf_add_return_if_needed(insn_buf_t *buf);
bool insn_buf_add_var(insn_buf_t *buf, const char *name, int size);
void insn_buf_dump(insn_buf_t *buf, const char *srctxt);
int insn_buf_fixup(insn_buf_t *buf);
void insn_buf_push_last_insn_pos(insn_buf_t *buf);
void insn_buf_push_cur_insn_pos(insn_buf_t *buf);
insn_t *insn_buf_pop_insn(insn_buf_t *buf);
void insn_buf_add_ins_with_tgt(insn_buf_t *buf, int type, insn_t *tgt);
void insn_buf_change_insn_tgt(insn_buf_t *buf, insn_t *insn, insn_t *tgt);
insn_t *insn_buf_next_insn(insn_buf_t *buf);
void insn_buf_end_src_pos(insn_buf_t *buf, int start, int end);
void insn_buf_new_src_pos(insn_buf_t *buf);
