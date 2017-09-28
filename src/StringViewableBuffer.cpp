//
//  StringViewableBuffer.cpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/14/17.
//  Copyright © 2017 me. All rights reserved.
//

#include "StringViewableBuffer.hpp"
#include "Misc.hpp"
#include "Global.hpp"

#include <assert.h>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <mutex>

namespace bjou {
    StringViewableBuffer::StringViewableBuffer() {
        _buff = nullptr;
        _begin = nullptr;
        _end = nullptr;
        _nbytes = 0;
    }
    
    StringViewableBuffer::StringViewableBuffer(const char * c_str) {
        _nbytes = std::strlen(c_str);
        _nbytes += 1;
        _buff = new char[_nbytes];
        assert(_buff && "Couldn't allocate _buff.");
        _buff[_nbytes - 1] = '\0';
        _begin = _buff;
        _end = _buff + _nbytes - 1;
        std::memcpy(_buff, c_str, _nbytes);
    }
    
    StringViewableBuffer::StringViewableBuffer(std::string& str) : StringViewableBuffer(str.c_str()) {  }
    
    StringViewableBuffer::StringViewableBuffer(std::ifstream& file) {
        assert(file && "Bad file.");
        auto save_pos = file.tellg();
        file.seekg(0, file.end);
        _nbytes = file.tellg();
        _nbytes += 1;
        _buff = new char[_nbytes];
        assert(_buff && "Couldn't allocate _buff.");
        _buff[_nbytes - 1] = '\0';
        _begin = _buff;
        _end = _buff + _nbytes - 1;
        file.seekg(0, file.beg);
        file.read(_buff, _nbytes - 1);
        assert(file.gcount() == _nbytes - 1);
        file.seekg(save_pos); // reset
    }
    
    const char * StringViewableBuffer::begin() const { return _begin; }
    const char * StringViewableBuffer::end() const { return _end; }
    
    void StringViewableBuffer::advance(long long n) {
        if (_begin + n >= _end)
            _begin = _end;
        else _begin += n;
    }
    
    void StringViewableBuffer::pullBack(long long n) {
        if (_end - n <= _begin)
            _end = _begin;
        else _end -= n;
    }
    
    unsigned long long StringViewableBuffer::viewSize() const { return _end - _begin; }
    
    std::string StringViewableBuffer::substr(long long i, long long n) const {
        assert(_begin + i + n <= _end && "Out of bounds.");
        return std::string(_begin + i, n);
    }
    
    StringViewableBuffer::operator std::string() const {
        return std::string(_begin, _end - _begin);
    }
    
    StringViewableBuffer::~StringViewableBuffer() {
        BJOU_DEBUG_ASSERT(_buff);
        delete[] _buff;
        _begin = nullptr;
        _end = nullptr;
    }
    
    char StringViewableBuffer::operator[] (long long i) const {
        assert(_begin + i <= _end && "Out of bounds.");
        return _begin[i];
    }
}
