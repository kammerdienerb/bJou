//
//  StringViewableBuffer.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/14/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef StringViewableBuffer_hpp
#define StringViewableBuffer_hpp

#include <string>
#include <fstream>

namespace bjou {    
    class StringViewableBuffer {
        char * _buff;
        char * _begin;
        char * _end;
        long long _nbytes;
        
    public:
        StringViewableBuffer();
        StringViewableBuffer(const char * c_str);
        StringViewableBuffer(std::string& str);
        StringViewableBuffer(std::ifstream& file);
        
        ~StringViewableBuffer();
        
        const char * begin() const;
        const char * end() const;
        
        void advance(long long n);
        void pullBack(long long n);
        
        unsigned long long viewSize() const;
        
        std::string substr(long long i, long long n) const;
        operator std::string() const;
        
        char operator[] (long long i) const;
    };
}

#endif /* StringViewableBuffer_hpp */
