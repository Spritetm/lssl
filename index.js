
window.addEventListener("load", (event) => {
	const btn = document.querySelector("#compile_btn");
	btn.addEventListener("click", recompile);
});

function recompile() {
	const code = document.querySelector("#code_text").textContent;
	var len=0;
//	var code_ptr=Module.stringToNewUTF8(code);
	var prog=Module.ccall('compile', 'Uint8Array', ['string'], [code]);
//	Module._free(code_ptr);
}
