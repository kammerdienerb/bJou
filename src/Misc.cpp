//
//  Misc.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 4/29/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Misc.hpp"
#include <string>

std::string de_quote(std::string& str) {
    if (str.size() > 2 && str[0] == '\"' && str[str.size() - 1] == '\"')
        return str.substr(1, str.size() - 2);
    return str;
}

std::string str_escape(std::string& str) {
    size_t pos = 0;
    std::string r = str;
    
    while (pos < r.size()) {
        if (r[pos] == '\\') {
            if (r[pos + 1] == 'n')
                r.replace(pos, 2, "\n");
            else if (r[pos + 1] == 't')
                r.replace(pos, 2, "\t");
            else if (r[pos + 1] == '\\')
                r.replace(pos, 2, "\\");
            // @incomplete
        }
        pos++;
    }
    return r;
}

char get_ch_value(std::string& str) {
    char ch;
    if (str.size() == 3)
        ch = str[1];
    else if (str == "'\\0'")
        ch = '\0';
    else if (str == "'\\n'")
        ch = '\n';
    else if (str == "'\\t'")
        ch = '\t';
    else return -1; // @incomplete
    
    return ch;
}
