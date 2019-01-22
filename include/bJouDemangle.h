/*
 * bJouDemangle.h
 * Symbol demangling
 * Brandon Kammerdiener
 * January, 2019
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t bJouDemangle_hash(const char * s);
void bJouDemangle_u642b32(uint64_t u, char * dst);

#ifdef __cplusplus
}
#endif
