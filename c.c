#include <string.h>
#include <stdio.h>

typedef struct {
    char * s;
} S;

int main() {
    char array[256] = {0};
    S s = { "hello, world!" };
    memcpy(array, s.s, 12);
    printf("%s\n", array);
    return 0;
}
