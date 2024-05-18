
demo: lssl_edit.css lssl_edit.html lssl_edit.js lssl.wasm lssl.wasm.map lssl.js

lssl.js lssl.wasm lssl.wasm.map:
	make -C ../src
	cp ../src/lssl.wasm ../src/lssl.js ../src/lssl.wasm.map .

lssl_edit.css lssl_edit.html lssl_edit.js:
	cp ../web/lssl_edit.css ../web/lssl_edit.html ../web/lssl_edit.js .

clean:
	rm lssl_edit.css lssl_edit.html lssl_edit.js lssl.wasm lssl.wasm.map lssl.js

.PHONY: demo clean
