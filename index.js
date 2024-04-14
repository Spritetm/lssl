
window.addEventListener("load", (event) => {
	const btn = document.querySelector("#compile_btn");
	btn.disabled=true;
	btn.addEventListener("click", recompile);
	const btntst = document.querySelector("#test_btn");
	btntst.addEventListener("click", test);
});



compile_errors=new Array();

function report_error(pos_start, pos_end, msg) {
	var er=[];
	er["start"]=pos_start;
	er["end"]=pos_end;
	er["msg"]=msg;
	compile_errors.push(er);
}


function insert_txtnode_or_error(hltext, code, start, end, err, cl) {
	var span=document.createElement('span');
	span.classList.add(cl);
	var txtnode=new Text(code.substring(start, end));
	span.appendChild(txtnode);
	//This assumes errors do not overlap, and more-or-less happen on a token
	//boundary (and/or tokens are pretty small).
	for (const e of err) {
		if (e["start"]>=start && e["start"]<end) {
			//Insert error div here.
			hltext.push(e["span"]);
		}
		if (e["start"]>=start && e["end"]<=end) {
			//Need to insert the node in the error span instead.
			e["span"].appendChild(span);
			return;
		}
	}
	//Not part of error. Just push back in main array.
	hltext.push(span);
}

function test() {
	compile_errors=new Array(); //clear errors
	const code = document.querySelector("#code_text").textContent;
	Module.ccall('recompile', 'Uint8Array', ['string'], [code]);
	
	//Syntax highlight
	classtxt=[
		"", //unknown, never returned by code
		"oper",
		"langfunc",
		"number",
		"punctuation",
		"comment",
	];
	var ptr=Module.ccall('tokenize_for_syntax_hl', 'number', ['string'], [code]);
	var txtpos=0;
	var hltext=new Array();
	
	var err=compile_errors.slice();
	for (let e of err) {
		var span=document.createElement('span');
		span.classList.add("compile_error");
		span.setAttribute("data-error", e["msg"]);
		e["span"]=span;
	}
	
	while(1) {
		var r=Module.HEAPU32.subarray(ptr/4, ptr/4+16);
		if (r[0]>r[1]) break; //end sentinel
		if (r[0]>txtpos) {
			insert_txtnode_or_error(hltext, code, txtpos, r[0], err, "plain")
			var txtnode=new Text(code.substring(txtpos, r[0]));
		}
		insert_txtnode_or_error(hltext, code, r[0], r[1], err, classtxt[r[2]]);
		txtpos=r[1];
		ptr+=16;
	}
	//add any remaining text
	insert_txtnode_or_error(hltext, code, txtpos, code.lenght, err, "plain");

	document.querySelector("#code_text").replaceChildren(...hltext);

	if (led_time==0) led_tick(); //kick off program run
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
