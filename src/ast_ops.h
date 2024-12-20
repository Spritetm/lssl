#pragma once

//ToDo: do these all need to be public? Are they still in sync with the c file?
void ast_ops_collate_consts(ast_node_t *node);
void ast_ops_attach_symbol_defs(ast_node_t *node);
void ast_ops_add_trailing_return(ast_node_t *node);
void ast_ops_var_place(ast_node_t *node);
void ast_ops_codegen(ast_node_t *node);
void ast_ops_fixup_addrs(ast_node_t *node);
void ast_ops_fixup_enter_leave_return(ast_node_t *node);
void ast_ops_assign_addr_to_fndef_node(ast_node_t *node);
void ast_ops_remove_useless_ops(ast_node_t *node);
void ast_ops_position_insns(ast_node_t *node);
void ast_ops_fix_parents(ast_node_t *node);

void ast_ops_do_compile(ast_node_t *prognode);

uint8_t *ast_ops_gen_binary(ast_node_t *node, int *len);


