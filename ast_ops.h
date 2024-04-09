#pragma once

void ast_ops_attach_symbol_defs(ast_node_t *node);
int ast_ops_var_place(ast_node_t *node);
void ast_ops_codegen(ast_node_t *node);
void ast_ops_fixup_addrs(ast_node_t *node);

