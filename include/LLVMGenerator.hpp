//
//  LLVMGenerator.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 1/24/17.
//  Copyright © 2017 me. All rights reserved.
//

#ifndef LLVMGenerator_hpp
#define LLVMGenerator_hpp

namespace bjou {
struct LLVMBackEnd;

struct LLVMGenerator {
    LLVMBackEnd & backEnd;

    LLVMGenerator(LLVMBackEnd & _backEnd);

    void generate();
};
} // namespace bjou
#endif /* LLVMGenerator_hpp */
