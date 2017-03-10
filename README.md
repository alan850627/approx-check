# Check instruction dependence on memory


### Build:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ cd ..

### compile
    $ clang test.c -o test

### to run function
    $ ./test

### build IR
    $ clang -O3 -emit-llvm test.c -c -o test.bc

### look at assembly code
    $ llvm-dis < test.bc | less

### run the simple pass
    $ opt -load build/ApproxCheck/libApproxCheck.so -ApproxCheck -disable-output test.bc
