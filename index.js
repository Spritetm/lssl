
window.addEventListener("load", (event) => {
	const btn = document.querySelector("#compile_btn");
	btn.disabled=true;
	btn.addEventListener("click", recompile);
});

var led_time=0;

function recompile() {
	const code = document.querySelector("#code_text").value;
	var len=0;
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