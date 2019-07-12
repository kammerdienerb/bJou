//
//  Misc.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 4/29/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "Misc.hpp"
#include <string>


/* credit:
 * https://stackoverflow.com/questions/3898840/converting-a-number-of-bytes-into-a-file-size-in-c
 */
#define DIM(x) (sizeof(x)/sizeof(*(x)))

static const char     *sizes[]   = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
static const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL *
                                   1024ULL * 1024ULL * 1024ULL;

char * calculateSize(size_t size) {   
    char     *result = (char *) malloc(sizeof(char) * 20);
    uint64_t  multiplier = exbibytes;
    int i;

    for (i = 0; i < DIM(sizes); i++, multiplier /= 1024) {   
        if (size < multiplier) {
            continue;
        }
        if (size % multiplier == 0) {
            sprintf(result, "%llu %s", size / multiplier, sizes[i]);
        } else {
            sprintf(result, "%.1f %s", (float) size / multiplier, sizes[i]);
        }
        return result;
    }
    strcpy(result, "0");
    return result;
}

std::string de_quote(std::string & str) {
    if (str == "\"\"")
        return "";
    if (str.size() > 2 && str[0] == '\"' && str[str.size() - 1] == '\"')
        return str.substr(1, str.size() - 2);
    return str;
}

bool has_suffix(const std::string & s, std::string suffix) {
    return (s.size() > suffix.size()) &&
           (s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
}

std::string str_escape(std::string & str) {
    size_t pos = 0;
    std::string r = str;

    while (pos < r.size()) {
        if (r[pos] == '\\') {
            if (r[pos + 1] == '\"')
                r.replace(pos, 2, "\"");
            else if (r[pos + 1] == 'n')
                r.replace(pos, 2, "\n");
            else if (r[pos + 1] == 't')
                r.replace(pos, 2, "\t");
            else if (r[pos + 1] == '\\')
                r.replace(pos, 2, "\\");
            else if (r[pos + 1] == 'r')
                r.replace(pos, 2, "\r");
            else if (r[pos + 1] == 'e')
                r.replace(pos, 2, "\033");
            // @incomplete
        }
        pos++;
    }
    return r;
}

char get_ch_value(std::string & str) {
    char ch;
    if (str.size() == 3)
        ch = str[1];
    else if (str == "'\\0'")
        ch = '\0';
    else if (str == "'\\n'")
        ch = '\n';
    else if (str == "'\\r'")
        ch = '\r';
    else if (str == "'\\e'")
        ch = '\e';
    else if (str == "'\\t'")
        ch = '\t';
    else if (str == "'\\\\'")
        ch = '\\';
    else if (str == "'\\''")
        ch = '\'';
    else
        return -1; // @incomplete

    return ch;
}
