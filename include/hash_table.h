/*
 * hash_table.h
 */

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#define USE_SKA_UNORDERED
#define USE_POW_2_HASHER
/* #define USE_MY_STRING_HASHER */
/* #define USE_HYBRID */

#include "flat_hash_map/flat_hash_map.hpp"

#if   defined USE_SKA_FLAT
    #define hash_table_t ska::flat_hash_map
#elif defined USE_SKA_BYTELL
    #include "flat_hash_map/bytell_hash_map.hpp"
    #define hash_table_t ska::bytell_hash_map
#elif defined USE_SKA_UNORDERED
    #include "flat_hash_map/unordered_map.hpp"
    #define hash_table_t ska::unordered_map
#else
    #include <unordered_map>
    #define hash_table_t std::unordered_map
#endif

#ifdef USE_HYBRID
    #include "hybrid_map.hpp"
    #define hash_table_t hybrid_map
#endif

#ifdef        USE_POW_2_HASHER
    #define STRING_HASHER ska::power_of_two_std_hash<std::string>
#elif defined USE_MY_STRING_HASHER
    #include "std_string_hasher.hpp"
    #define STRING_HASHER std_string_hasher
#else
    #define STRING_HASHER std::hash<std::string>
#endif

#endif
