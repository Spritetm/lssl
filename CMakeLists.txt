set(SRC "src/led_syscalls.c" "src/vm.c" "src/vm_defs.c" "src/vm_syscall.c"
		"idf_bindings/lssl_idf_web.c" "${BUILD_DIR}/parser.c" "${BUILD_DIR}/lexer.c")

idf_component_register(SRCS ${SRC}
        INCLUDE_DIRS src/ idf_bindings/ ${BUILD_DIR}/
        EMBED_FILES web/lssl_edit.html web/lssl_edit.css web/lssl_edit.js
			${BUILD_DIR}/lssl.js ${BUILD_DIR}/lssl.wasm ${BUILD_DIR}/lssl.wasm.map
		REQUIRES json esp_http_server nvs_flash)


add_custom_command(OUTPUT ${BUILD_DIR}/parser.c ${BUILD_DIR}/parser_gen.h
     COMMAND bison --header=${BUILD_DIR}/parser_gen.h --output=${BUILD_DIR}/parser.c ${COMPONENT_DIR}/src/parser.y
     DEPENDS ${COMPONENT_DIR}/src/parser.y
     VERBATIM)

add_custom_command(OUTPUT ${BUILD_DIR}/lexer.c ${BUILD_DIR}/lexer_gen.h
     COMMAND flex --header=${BUILD_DIR}/lexer_gen.h -o ${BUILD_DIR}/lexer.c  ${COMPONENT_DIR}/src/lexer.l
     DEPENDS ${COMPONENT_DIR}/src/lexer.l
     VERBATIM)

add_custom_target(parser DEPENDS ${BUILD_DIR}/parser.c ${BUILD_DIR}/parser_gen.h)
add_dependencies(${COMPONENT_LIB} parser)


add_custom_target(lexer DEPENDS ${BUILD_DIR}/lexer.c ${BUILD_DIR}/lexer_gen.h)
add_dependencies(${COMPONENT_LIB} lexer)


set(SRC_WASM "src/vm_defs.c" "src/ast.c" "src/ast_ops.c" "src/compile.c"
	"src/codegen.c" "src/led_syscalls.c" "src/vm_syscall.c" "src/vm.c" "src/js_funcs.c")

set(SRC_WASM_GEN "lexer.c" "parser.c")

list(TRANSFORM SRC_WASM PREPEND ${COMPONENT_DIR}/)
list(TRANSFORM SRC_WASM_GEN PREPEND ${BUILD_DIR}/)

set(EMCC_ARG -O2 -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -I${BUILD_DIR} -I${COMPONENT_DIR}/src/
	-sEXPORTED_FUNCTIONS=_init,_recompile,_get_led,_tokenize_for_syntax_hl,_free,_frame_start 
	-sASSERTIONS=1 -sFILESYSTEM=0 -gsource-map --source-map-base=./)

add_custom_command(OUTPUT ${BUILD_DIR}/lssl.js ${BUILD_DIR}/lssl.wasm ${BUILD_DIR}/lssl.wasm.map
	COMMAND emcc -o ${BUILD_DIR}/lssl.js ${SRC_WASM} ${SRC_WASM_GEN} ${EMCC_ARG} -lm
	DEPENDS ${SRC_WASM} ${SRC_WASM_GEN}
	VERBATIM)

add_custom_target(wasm DEPENDS "${BUILD_DIR}/lssl.js" "${BUILD_DIR}/lssl.wasm" "${BUILD_DIR}/lssl.wasm.map")
add_dependencies(${COMPONENT_LIB} wasm)

set_property(DIRECTORY "${COMPONENT_DIR}" APPEND PROPERTY 
	ADDITIONAL_CLEAN_FILES parser.c parser_gen.h lexer_gen.h lexer.c
							lssl.js lssl.wasm lssl.wasm.map)

