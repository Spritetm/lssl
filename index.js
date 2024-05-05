"use strict"
var led_time=0;
var compile_timeout=null;
var led_timer=null;

window.addEventListener("load", (event) => {
	const code = document.querySelector("#code_text");
	code.addEventListener("keydown", code_keypress);
	code.addEventListener("keyup", code_keyrelease);
	var t=localStorage.getItem("code");
	if (t) code.textContent=t;
});


function get_pos_for(node, s_node, s_off) {
	if (s_node.nodeType==Node.ELEMENT_NODE) {
		//Note: In this case s_off is the nth child element of
		//s_node, not the text position within s_node.
		//Figure out that child and call the function with an offset
		//of 0 there to get the correct result.
		return get_pos_for(node, s_node.childNodes[s_off], 0);
	}

	if (node==s_node) {
		return [true, s_off];
	}
	if (node.nodeType==Node.TEXT_NODE) { //text node
		return [false, node.textContent.length];
	} else {
		var off=0;
		for (const n of node.childNodes) {
			var [nf, no]=get_pos_for(n, s_node, s_off);
			if (nf) {
				return [true, no+off];
			} else {
				off+=no;
			}
		}
		return [false, off];
	}
}

function get_sel_pos(container) {
	if (document.getSelection().rangeCount<1) return [0, 0];
	var sel=document.getSelection().getRangeAt(0);
	var [f, soff]=get_pos_for(container, sel.startContainer, sel.startOffset);
	var [f, eoff]=get_pos_for(container, sel.endContainer, sel.endOffset);
	return [soff, eoff];
}

//Returns a [node, offset in the node] for a certain offset into a [grand]parent node
function sel_pos_to_node_off(node, off) {
	var len=0;
	var t=0;
	for (const n of node.childNodes) {
		if (n.nodeType==Node.TEXT_NODE) { //text node
			if (off==0) {
				// A browser-specific workaround, in 2024?
				// Firefox acts weird if we put the cursor on a boundary of a textnode
				// if the node has a newline. We use the alternative notation (cursor
				// is at the n'th subelement of the container) instead; that does seem
				// to work OK.
				return [node, t];
			}
			if (off<n.textContent.length) {
				//Found the node.
				return [n, off];
			} else {
				len+=n.textContent.length;
			}
		} else if (n.nodeType==Node.ELEMENT_NODE) {
			var [fn, fo]=sel_pos_to_node_off(n, off-len);
			if (fn!==null) return [fn, fo]; //that call found the node
			len+=fo;
		}
		t++;
	}
	return [null, len];
}


function set_sel_to(node, off) {
	var sel=new Range();
	var [snode, soff]=sel_pos_to_node_off(node, off[0]);
	var [enode, eoff]=sel_pos_to_node_off(node, off[1]);
	if (snode===null || enode===null) return;
	sel.setStart(snode, soff);
	sel.setEnd(enode, eoff);
	document.getSelection().removeAllRanges();
	document.getSelection().addRange(sel);
}

function code_keypress(e) {
	const code = document.querySelector("#code_text");
	if (e.keyCode==9) {
		//tab
		e.preventDefault();
		var sel=document.getSelection().getRangeAt(0);
		sel.deleteContents();
		var txt=new Text("\t")
		sel.insertNode(txt);
		sel.setStart(txt, 1);
		sel.setEnd(txt, 1);
		document.getSelection().removeAllRanges();
		document.getSelection().addRange(sel);
	}
}

function code_keyrelease(e) {
	//Movement keys won't change the text, and re-highlighting during e.g.
	//selecting text with shift-arrowkeys will mess with the selection, so we'll
	//just ignore any keys that can't change the actual text.
	const ignore=["ArrowUp","ArrowDown", "ArrowLeft", "ArrowRight", "Meta", "PageUp", "PageDown"];
	if (ignore.includes(e.key)) return;

	const code = document.querySelector("#code_text");
	//remember cursor pos
	var off=get_sel_pos(code);
	compile_errors=new Array(); //clear errors

	syntax_highlight();
	set_sel_to(code, off);
	if (compile_timeout===null) clearTimeout(compile_timeout)
	compile_timeout=setTimeout(recompile_typed, 1000);

	localStorage.setItem("code", code.textContent);
}

var compile_errors=new Array();

function recompile_typed() {
	compile_errors=new Array(); //clear errors
	const code = document.querySelector("#code_text")
	const code_text=code.textContent;
	Module.ccall('recompile', 'Uint8Array', ['string'], [code_text]);

	if (compile_errors.length==0) {
		//Start running VM
		if (led_timer===null) {
			console.log("Compile succesful. Restarting VM");
			led_timer=setInterval(led_tick, 20);
		}
	} else {
		//Stop running VM
		if (led_timer!==null) {
			console.log("Compile has errors. Stopping VM");
			clearInterval(led_timer);
			led_timer=null;
			led_time=0;
		}
	}

	//Show errors (or hide previous ones)
	var off=get_sel_pos(code);
	syntax_highlight();
	set_sel_to(code, off);
}


function report_error(pos_start, pos_end, msg) {
	var er=[];
	er["start"]=pos_start;
	er["end"]=pos_end;
	er["msg"]=msg;
	compile_errors.push(er);
}


function insert_txtnode_or_error(hltext, code_text, start, end, err, cl) {
	var word=code_text.substring(start, end);
	var span=document.createElement('span');
	span.classList.add(cl);
	var txtnode=new Text(word);
	span.appendChild(txtnode);

	//This assumes errors do not overlap, and more-or-less happen on a token
	//boundary (and/or tokens are pretty small).
	for (const e of err) {
		if (e["start"]>=start && e["start"]<end && !("inserted" in e)) {
			//Insert error div here.
			hltext.push(e["span"]);
			e["span"].appendChild(span);
			e["inserted"]=true;
			return;
		} else if ((start>e["start"] && start<=e["end"]) ||
					(end>e["end"] && end<=e["end"])) {
			//Need to insert the node in the error span instead.
			e["span"].appendChild(span);
			return;
		}
	}
	//Not part of error. Just push back in main array.
	hltext.push(span);
}

function syntax_highlight() {
	const code = document.querySelector("#code_text");
	const code_text = code.textContent;
	//Syntax highlight
	const classtxt=[
		"", //unknown, never returned by code
		"oper",
		"langfunc",
		"number",
		"punctuation",
		"comment",
	];
	
	//Generate span for error messages, if any
	var err=compile_errors.slice(); //copy errors as we're gonna mess with it
	for (let e of err) {
		var span=document.createElement('span');
		span.classList.add("compile_error");
		span.setAttribute("data-error", e["msg"]);
		e["span"]=span;
	}
	
	var ptr=Module.ccall('tokenize_for_syntax_hl', 'number', ['string'], [code_text]);
	var txtpos=0;
	var hltext=new Array();
	while(1) {
		var r=Module.HEAPU32.subarray(ptr/4, ptr/4+16);
		if (r[0]>r[1]) break; //end sentinel; end highlighting.
		if (r[0]>txtpos) {
			//insert the bit between the end of the prev token and the start of this node
			insert_txtnode_or_error(hltext, code_text, txtpos, r[0], err, "plain")
			var txtnode=new Text(code_text.substring(txtpos, r[0]));
		}
		insert_txtnode_or_error(hltext, code_text, r[0], r[1], err, classtxt[r[2]]);
		txtpos=r[1];
		ptr+=16;
	}
	//add any remaining text
	insert_txtnode_or_error(hltext, code_text, txtpos, code.lenght, err, "plain");

	document.querySelector("#code_text").replaceChildren(...hltext);
}


function led_tick() {
	const canvas = document.querySelector("#leds");
	const ctx = canvas.getContext("2d");

	Module.ccall('frame_start', '', [], []);
	for (var i=0; i<100; i++) {
		var leds_ptr=Module.ccall("get_led", "number", ["int", "float"], [i, led_time]);
		if (leds_ptr==0) {
			//vm exec error, stop running
			console.log("VM ran into a runtime error. Stopping.");
			clearInterval(led_timer);
			led_timer=null;
			break;
		}
		var r=Module.HEAPU8[leds_ptr+0];
		var g=Module.HEAPU8[leds_ptr+1];
		var b=Module.HEAPU8[leds_ptr+2];
		ctx.fillStyle = "rgb("+r+" "+g+" "+b+")";
		ctx.fillRect(i, 0, i+1, 1);
	}
	led_time+=0.02;
}
