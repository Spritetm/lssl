#pragma once

void ast_ops_attach_symbol_defs(ast_node_t *node);
void ast_ops_add_trailing_return(ast_node_t *node);
int ast_ops_var_place(ast_node_t *node);
void ast_ops_codegen(ast_node_t *node);
void ast_ops_fixup_addrs(ast_node_t *node);
void ast_ops_fixup_enter_leave_return(ast_node_t *node);
void ast_ops_assign_addr_to_fndef_node(ast_node_t *node);
void ast_ops_remove_useless_ops(ast_node_t *node);
void ast_ops_position_insns(ast_node_t *node);


uint8_t *ast_ops_gen_binary(ast_node_t *node, int *len);


