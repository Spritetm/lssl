These are tests that can be used to smoketest if the compiler still works.

All tests must either compile, run, and have main() return the number 42,
or they need to result in a compile or runtime error in a specific location.
The specific location where the error is expected is indicated using an 
error marker. Specifically, it's the comment "ERROR HERE" followed by a 
comment with a v indicating the position in the next line where the 
error is expected. For instance:

//ERROR HERE
//  v
a=a+";

Note that the test code doesn't parse tabs, so it's best to align the code
producing the error and the v itself using spaces. Also note that the test
code doesn't care what error the faulty code generates, just that the 'v'
points somewhere inside the token that generates the error.
