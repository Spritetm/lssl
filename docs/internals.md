
LSSL VM internals
=================

The VM has two memory spaces: the program and the stack. The program is loaded
from the compiled code and cannot be changed (='ROM'). The stack is an array of
32-bit R/W memory that works as, well, a stack. It grows upward, that is, the
first byte saved is at position 0, the 2nd at position 1 etc.

Generally, the VM behaves as you'd expect from a stack-based VM: instructions
pop their operands from the stack, do stuff with it, and push the result back 
to the stack. The VM only has one 'basic' data type, namely an 16.16 fixed point
integer. (The rest of the text refers to these as 'the basic data type'). Any 
other datatype is a composite made up of structs/arrays of this basic datatype
(refered to as an 'object' in the rest of the text). Note that the 'basic' data
type is also used to store pointers to composite types, as well as PC addresses
(for e.g. function pointers).

Specifically, there are three stack pointers:
- SP, the Stack Pointer. Always points to the top of the stack; any RAM above it
  can be seen as having undefined data.
- BP, the Base Pointer. This is the SP when a function call starts. It provides
  a handy way to get to local basic datatype variables and the pointers to local objects: 
  these are stored directly above the base pointer. It can also be used to access
  function arguments: these are at a fixed position under the BP (so BP-4, BP-5, ...)
- AP, the Allocation Pointer. At the entry of each function block, this gets
  set to SP. Any local allocation of objects then happens by increasing SP
  by the needed amount and saving the address in the local variable pointer
  as referenced by BP. At the end of the function block, SP is restored to AP,
  immediately clearing all space allocated to the local objects.

A program starts with two words of 'meta-info'. The first one is the program version.
This document describes version 1. The second one is the length of globals. The SP
will initially be set to this, so the first byte to be pushed to the stack will be
pushed after the end of the global section. BP and AP are initialized to 0, as they
will be set to a proper value when a function is entered.

Within a function, the first instruction is 'ENTER'; this establishes a new
stack frame by adding the argument to SP. Now, the difference between BP and
SP contains the space for local 'basic' variables.

To initialize global or local objects, the 'STRUCTINIT' and 'ARRAYINIT' instructions
are run. Regardless of placement of the associated struct or array declarations,
these instructions are placed at the start of the function block their lifetime
is associated with if the objects are local to a function block, or at the 
start of the program when the objects are global. These increase SP with a given number
to allocate space for the object and save the pointer to this space into the 'basic'
data type spot created with the 'ENTER' instruction. These pointers can then be used
to e.g. pass the object to functions.

Note that these pointers encode both the address of the object data in memory 
(in the top 16 bit of the pointer), as well as the size of the object (in the bottom
16 bits). This allows for detecting array-out-of-bounds issues as well as accepting
unknown-sized arrays in functions.

ToDo:
- What does a multidimensional array look like in RAM?
- What does a struct that includes a struct look like?




