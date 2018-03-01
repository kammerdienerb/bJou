#include <cstdio>

int main() {
    int i = 12345;
    int j = 54321;
    int& r = i;
    r = 67890;
    r = j;
    printf("i: %d, r: %d\n", i, r);
}
