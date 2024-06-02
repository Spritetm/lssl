# LedStrip Shader Language

The intent of this project is to allow you to program effects for your LED strips
in a dynamic way. Generally used LED-strip firmware has the downside that if you want
to define a (dynamic) effect, you need to open the source code, add the effect to 
the firmware, recompile and flash the microcntroller with new firmware. This project,
on the other hand, allows you to write effects like that 'live' in a webpage where
you can see the changes immediately.

In other words: you can take a project that uses LSSL, flash it to e.g. an ESP32 chip
attached to a strip of WS2812-like LEDs, then point your browser at the IP of the 
ESP32 and modify the source code of the running effect. You will immediately see 
any compilation errors, and if there are none, you will see the changed effect run 
both in your browser as well as on the LED-strip.

This project came into being because I have several LED-strips and spotlight strings
using WS2812-like LEDs around my home. Some of those need very specific lighting 
scenarios (e.g. make the lights for the plants green but color-cycle the rest) and I 
didn't want to integrate these effects in the (universal) firmware.

This project is inspired by OpenGL shaders, which are small programs that assign a color
value to each pixel in a rendered triangle; the 'shader language' in the name stems
from that. It's also inspired by <a href="https://electromage.com/pixelblaze">Pixelblaze</a>,
a commercial product that uses a Javascript dialect to implement the same idea. (If
you don't want to mess around with firmware, I suggest you buy their modules; they
seem like a great off-the-shelf solution.) The downside of Pixelblaze is that the software
is not open-source, though, and I needed the capability to add e.g. my own output routines
and my own control protocols.

LedStrip Shader Language (LSSL) itself is a language that derives bits from C and 
Javascript. It compiles into a binary that can be executed on a small virtual machine
which is called to calculate the LED values.

## Example

```C
function set_led(pos, time) {
	var a=pos*0.2+time*2;
	var r=sin(a)*127+128;
	var g=sin(a+2.1)*127+128;
	var b=sin(a+4.2)*127+128;
	led_set_rgb(r, g, b);
}

function main() {
	register_led_cb(set_led);
}
```

![led sin example](doc/led_sin.gif "led sin example")

## General architecture

The LSSL codebase consists of three main components: the compiler, the virtual machine 
and the editor.

The compiler is written in C and uses Flex and Bison as the lexer and parser, 
respectively. It takes in LSSL source code and compiles it into a binary that can
be ran on the virtual machine. It does error reporting (e.g. when there's a syntax
error) and also has some hooks for the editor in order to help with syntax 
highlighting. The compiler can be compiled into a standalone commandline program,
but normally the compiler is a WebAssembly file that gets called from the editor.
In other words: compilation normally happens in your web browser, meaning the 
microcontroller that runs the LED strip doesn't have to dedicate any resources to
that task.

The virtual machine is the code that interprets the bytecode. It's not very large: at
the moment of writing, the code clocks in at less than 10K of flash usage. RAM usage
depends on the program running in the VM, but you could get away with as low as tens
of bytes with a trivial program. The virtual machine code gets compiled both into
WebAssembly as for the target microcontroller; the WebAssembly file allows the
editor to show a simulation of the output to the LED strip, while the microcontroller
uses the VM to control the actual LED-strip.

The editor is a bit of HTML, CSS and Javascript that allow you to edit the current
program. Because the compiler is integrated into this as a WebAssembly binary, the
editor can do on-the-fly syntax highlighting and simulation: a second or two after you
stop typing, the editor will compile your program and either show any compilation errors
or a simulation of how a LED-strip running the current program would look.

## Integration into your own project

LSSL is not intended as a standalone project itself; its purpose is to easily be
integrated into other projects.

You can integrate LSSL in your projects in multiple ways:

* Use the VM, compiler and editor as standalone units.

In this case, the src/ directory contains all you need for the compiler and VM,
and the demo/ directory should give you an idea of what to serve using a webserver.
(You still need to build the backend that stores the programs yourself.) 
The Makefile in both directories should show you the process of building both
the compiler/VM and the webassembly files. You can use ``lssl.c`` as an example on how
to call everything. This is useful e.g. to compile programs on a server and then
send them to a separate microcontroller that runs the actual effects.

* Use the VM in ESP-IDF. This entire project is an ESP-IDF component
and can be integrated in both 









