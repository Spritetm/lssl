# Language reference

The LSSL language looks like a mix between C and Javascript. It's a domain specific
language for controlling LEDs, with things not useful for that purpose stripped out.
For instance: there's no support for strings and only one basic numerical
data type.

## Comments

C-style single line comments starting with ``//`` are allowed: everything from the
``//`` to the end of the line is ignored. Multiline comments are not supported.

## Variables

Variables in LSSL have a static datatype. LSSL has three basic data 
types: the plain old scalar, the array, and the struct. There is a fourth
datatype (the function pointer) but its use is limited to passing to
syscalls.

### Scalar type

A scalar type is defined by the keyword 'var' and can optionally assigned a value:

```c
var x; //x will be zero-initialized
var y=1;
var z=y+2; //initializers can be dynamic
```

Scalars are implemented as 16.16 fixed point numbers. This means that they go from
slightly more than -32769 to slightly less than 32768, and have a precision of
1/65536'th. Math using scalars will saturate: if a number overflows or underflows, it
will be clamped to the maximum or minimum variable the scalar supports, respectively.


### Arrays

LSSL supports arrays without a limit to dimensions. Arrays in LSSL are base-zero, so
array[0] will reference the first element, array[1] the second etc. Note that LSSL keeps
track of the size of an array; referencing a member that is outside the range specified 
when declaring the array will lead to a runtime error. Arrays are also declared with the
'var' keyword, but at the moment cannot be initialized at declaration time.

```c
var a[2]; //declare an array with 2 members
a[0]=1;
a[1]=2;
a[3]=3; //this causes a runtime error
var twodimensional[2][2] //two-dimensional array with in total 4 members
twodimensional[0][1]=1;

var members=2+2;
var b[members];			//size of array can be defined at runtime
```

### Structs

Structs are collections of the other two datatypes. They're statically declared,
like C structs, but the syntax is a bit different: they behave like they're typedef'fed
but there's no need for typedef in the declaration:

```c
//This declares the type. (Note that this does *not* create a my_struct variable.)
struct my_struct {
	var a;			//scalar member
	var b[10];		//array member
}

struct second_struct {
	my_struct c;		//struct as struct member
	my_struct d[10];	//array of structs as struct member
}

//this *does* define a variable of type my_struct
second_struct myvar;
//use a dot to access struct members
myvar.d[2].b[2]=3;

//note you can have arrays of structs
my_struct myarray[2];
myarray[1].a=2;
```

### Function pointers

Function pointers are not fully supported: they cannot be kept in variables, arrays 
or structs, and normal functions cannot use them as an argument or return value.
They only are supported in syscalls, that is, external functions written in C that
are part of the virtual machine the code runs on.

```c

function my_function(var a, var b) {
	return 42;
}

function does_not_compile(funcarg) {
	funcarg(); //gives a syntax error, function pointers not supported here
}


does_not_compile(my_function); //syntax error - function pointers not supported here
//note register_led_cb is a syscall
register_led_cb(my_function); //compiles - syscalls can take function pointers
```

## Statements

LSSL statements can end in either a semicolon or a newline, like in Javascript. 
Statements can be grouped in blocks: a block creates a new variable scope. For instance:

```c
var a=1;
{
	var a=2;
	//a will be 2 here
}
//a will be 1 here
```

### Functions

Code in LSSL is grouped in functions. Functions start with the keyword 'function' and have
their arguments in brackets, with the body either being a single statement or, more often,
a block delimited by curly brackets. Function arguments can be any combination of 
scalars, arrays and structs. Functions always have a scalar as a return value, either 
explicitly (through a 'return' statement) or implicitly (at the end of the function, it
will always return with a value of 0). Returning arrays or structs is not supported.

```c
struct my_struct {
	var member;
	[...]
}

//An example of a function taking various args
function my_fn(scalar, array[], my_struct struct, my_struct struct_array[]) {
	var ret=scalar;
	ret+=array[0];
	ret+=struct.member;
	ret+=struct_array[0].member;
	return ret;
}

var a;
var b[2];
my_struct c;
my_struct d[2];

var r=my_fn(a, b, c, d);
//You can also ignore the return value.
my_fn(a, b, c, d);
```

Note that unlike in C, functions do not need to be declared before they are used.


### Syscalls




