// fact.c

// #include "bjou_ct.h"

long long factorial(long long n) {
    // BJOU_CT(add_llvm_pass, "tailcallelim");

    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}

void mybenchmark() {
    for (int i = 0; i < 100000000; i += 1)
        factorial(20);
}
