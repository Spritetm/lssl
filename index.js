
window.addEventListener("load", (event) => {
	const btn = document.querySelector("#compile_btn");
	btn.disabled=true;
	btn.addEventListener("click", recompile);
	const btntst = document.querySelector("#test_btn");
	btntst.addEventListener("click", test);
});


function test() {
	classtxt=[
		"", //unknown, never returned by code
		"oper",
		"langfunc",
		"number",
		"punctuation",
		"comment",
	];
	const code = document.querySelector("#code_text").textContent;
	var ptr=Module.ccall('tokenize_for_syntax_hl', 'number', ['string'], [code]);
	var txtpos=0;
	var hltext=new Array();
	while(1) {
		var r=Module.HEAPU32.subarray(ptr/4, ptr/4+16);
		if (r[0]>r[1]) break; //end sentinel
		if (r[0]>txtpos) {
			var txtnode=new Text(code.substring(txtpos, r[0]));
			hltext.push(txtnode);
		}
		var span=document.createElement('span');
		span.classList.add(classtxt[r[2]]);
		var txtnode=new Text(code.substring(r[0], r[1]));
		span.appendChild(txtnode);
		hltext.push(span);
		txtpos=r[1];
		ptr+=16;
	}
	//add any remaining text
	var txtnode=new Text(code.substring(txtpos));
	hltext.push(txtnode);
	document.querySelector("#code_text").replaceChildren(...hltext);
}


var led_time=0;

function recompile() {
	const code = document.querySelector("#code_text").textContent;
	Module.ccall('recompile', 'Uint8Array', ['string'], [code]);
	if (led_time==0) led_tick(); //kick off program run
}


function led_tick() {
	const canvas = document.querySelector("#leds");
	const ctx = canvas.getContext("2d");
	for (var i=0; i<100; i++) {
		leds_ptr=Module.ccall("get_led", "number", ["int", "float"], [i, led_time]);
		var r=Module.HEAPU8[leds_ptr+0];
		var g=Module.HEAPU8[leds_ptr+1];
		var b=Module.HEAPU8[leds_ptr+2];
		ctx.fillStyle = "rgb("+r+" "+g+" "+b+")";
		ctx.fillRect(i, 0, i+1, 1);
	}
	led_time+=0.02;
	setTimeout(led_tick, 20); 
}