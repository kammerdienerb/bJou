// fact.c

long long factorial(long long n) {
    n + 1 * 2;
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}

void mybenchmark() {
    for (int i = 0; i < 100000000; i += 1)
        factorial(20);
}
