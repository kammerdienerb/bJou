/*
 * bJouDemangle.c
 * Symbol demangling
 * Brandon Kammerdiener
 * November, 2018
 */

#include <stdint.h>

uint64_t bJouDemangle_strlen(const char * s) {
    uint64_t len;

    len = 0;

    while (*s++)
        len += 1;

    return len;
}

void bJouDemangle_strcat(char * s1, const char * s2) {
    uint64_t l2,
             i;

    s1 += bJouDemangle_strlen(s1);
    l2  = bJouDemangle_strlen(s2);

    for (i = 0; i < l2; i += 1)
        *s1++ = s2[i];
    *s1 = 0;
}

void bJouDemangle_strrev(char * s) {
    char * end;

    end = s + bJouDemangle_strlen(s) - 1;

#define XOR_SWAP(a,b) do {\
    a ^= b;               \
    b ^= a;               \
    a ^= b;               \
} while (0)

    while (s < end) {
        XOR_SWAP(*s, *end);
        s   += 1;
        end -= 1;
    }
#undef XOR_SWAP
}

uint64_t bJouDemange_hash(char * s) {
    uint64_t h1;
    char     c;

    h1 = 5381ULL;

    while ((c = *s++))
        h1 = h1 * 33 ^ c;

    return h1;
}

void bJouDemangle_u642b32(uint64_t u, char * dst) {
    uint64_t _0,
             _a,
             d;
    char     c[2];

    _0 = '0';
    _a = 'a';

    *dst = c[1] = 0;

    do {
        d    = u % 32;
        c[0] = (d < 10)
                 ? (char)(_0 + d)
                 : (char)(_a + d - 10);
    
        bJouDemangle_strcat(dst, c);

        u /= 32;
    } while (u > 0);

    bJouDemangle_strrev(dst);
}
