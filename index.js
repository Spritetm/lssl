
window.addEventListener("load", (event) => {
	const btn = document.querySelector("#compile_btn");
	btn.addEventListener("click", recompile);
	//todo: do this when runtime is ready
	setTimeout(led_tick, 500); 
});

function recompile() {
	const code = document.querySelector("#code_text").value;
	var len=0;
	Module.ccall('recompile', 'Uint8Array', ['string'], [code]);
}

var led_time=0;

function led_tick() {
	const canvas = document.querySelector("#leds");
	const ctx = canvas.getContext("2d");
	leds=new Uint8Array(3);
	for (var i=0; i<100; i++) {
		Module.ccall("get_led", null, ["int", "float", "array"], [i, led_time, leds]);
		ctx.fillStyle = "rgb("+leds[0]+" "+leds[1]+" "+leds[2]+")";
		ctx.fillRect(i*20, 0, i*20+20, 20);
	}
	led_time+=0.1;
	setTimeout(led_tick, 100); 
}