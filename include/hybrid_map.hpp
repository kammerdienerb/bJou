//
//  hybrid_map.hpp
//  bjou
//
//  Created by Brandon Kammerdiener on 2/28/19.
//

#ifndef hybrid_map_hpp
#define hybrid_map_hpp 

#include <iterator>
#include <utility>

#include "hash_table.h"

#define HYBRID_MAP_N (8)

template <typename K_T, typename V_T, typename H_T>
struct hybrid_map;

template <typename K_T, typename V_T, typename H_T>
struct hybrid_map_it {
    typedef typename hash_table_t<K_T, V_T, H_T>::const_iterator hash_it_t;

    const hybrid_map<K_T, V_T, H_T> * the_map;
    bool using_static;
    int static_idx;
    hash_it_t hash_it;
    std::pair<K_T, V_T> the_pair;

    hybrid_map_it(const hybrid_map<K_T, V_T, H_T> * _the_map, int _static_idx)
        : the_map(_the_map), using_static(true), static_idx(_static_idx) { }
    hybrid_map_it(const hybrid_map<K_T, V_T, H_T> * _the_map, hash_it_t _hash_it)
        : the_map(_the_map), using_static(false) {
            hash_it = _hash_it;
        }

    bool operator==(hybrid_map_it<K_T, V_T, H_T>& rhs) {
        if (using_static) {
            return static_idx == rhs.static_idx;
        }
        return hash_it == rhs.hash_it;
    }

    bool operator!=(hybrid_map_it<K_T, V_T, H_T>& rhs) {
        return !(*this == rhs);
    }

    hybrid_map_it<K_T, V_T, H_T>& operator++() {
        if (using_static) {
            static_idx += 1;
        } else {
            ++hash_it;
        }
        return *this;
    }

    std::pair<K_T, V_T>& operator*() {
        return using_static
                ? (the_pair = std::make_pair(
                        the_map->static_key_storage[static_idx], 
                        the_map->static_val_storage[static_idx]))
                : (the_pair = std::make_pair(hash_it->first, hash_it->second));
    }

    std::pair<K_T, V_T> * operator->() {
        return &(using_static
                ? (the_pair = std::make_pair(
                        the_map->static_key_storage[static_idx], 
                        the_map->static_val_storage[static_idx]))
                : (the_pair = std::make_pair(hash_it->first, hash_it->second)));
    }
};

template <typename K_T, typename V_T, typename H_T>
struct hybrid_map {
    K_T                          static_key_storage[HYBRID_MAP_N];
    V_T                          static_val_storage[HYBRID_MAP_N];
    size_t                       n_static;
    bool                         using_static;
    hash_table_t<K_T, V_T, H_T> hash_table;

    hybrid_map()
        : n_static(0),
          using_static(true),
          hash_table(HYBRID_MAP_N * 2) { }

    size_t size() const {
        if (using_static) {
            return n_static;
        }

        return hash_table.size();
    }


    hybrid_map_it<K_T, V_T, H_T> get_static_it(int i) const {
        return hybrid_map_it<K_T, V_T, H_T>(this, i);
    }

    hybrid_map_it<K_T, V_T, H_T> begin() const {
        if (using_static) {
            return get_static_it(0);
        }

        return hybrid_map_it<K_T, V_T, H_T>(this, hash_table.begin());
    }

    hybrid_map_it<K_T, V_T, H_T> end() const {
        if (using_static) {
            return get_static_it(n_static);
        }

        return hybrid_map_it<K_T, V_T, H_T>(this, hash_table.end());
    }

    V_T& operator[](const K_T& key) {
        if (using_static) {
            for (int i = 0; i < n_static; i += 1) {
                if (key == static_key_storage[i]) {
                    return static_val_storage[i];
                }
            }

            static_key_storage[n_static]            = key;
            V_T& ret = static_val_storage[n_static] = {};

            n_static += 1;

            if (n_static == HYBRID_MAP_N) {
                for (int i = 0; i < n_static - 1; i += 1) {
                    hash_table.insert({ static_key_storage[i], static_val_storage[i] });
                }
                auto pair = hash_table.insert({ static_key_storage[n_static - 1], static_val_storage[n_static - 1] });

                using_static = false;
                return pair.first->second;
            }

            return ret;
        }

        return hash_table[key];
    }

    void insert(hybrid_map_it<K_T, V_T, H_T> first, hybrid_map_it<K_T, V_T, H_T> second) {
        auto it = first;
        for (; it != second; ++it) {
            (*this)[it->first] = it->second;
        }
    }

    hybrid_map_it<K_T, V_T, H_T> find(K_T key) {
        if (using_static) {
            for (int i = 0; i < n_static; i += 1) {
                if (key == static_key_storage[i]) {
                    return get_static_it(i);
                }
            }
            return get_static_it(n_static);
        }
       
        return hybrid_map_it<K_T, V_T, H_T>(this, hash_table.find(key));
    }

    size_t count(const K_T& key) {
        if (using_static) {
            for (int i = 0; i < n_static; i += 1) {
                if (key == static_key_storage[i]) {
                    return 1;
                }
            }
            return 0;
        }

        return hash_table.count(key);
    }
};

#endif
