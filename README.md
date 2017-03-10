# Check instruction dependence on memory


### Build the pass:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ cd ..

### compile the test
    $ clang test.c -o test

### to run test
    $ ./test

### build IR
    $ clang -O3 -emit-llvm test.c -c -o test.bc

### look at assembly code
    $ llvm-dis < test.bc | less

### run the pass
    $ opt -load build/ApproxCheck/libApproxCheck.so -ApproxCheck -disable-output test.bc
