//
//  Defaults.hpp
//  bjou++
//
//  Created by Brandon Kammerdiener on 5/23/18.
//  Copyright © 2018 me. All rights reserved.
//

#include "Defaults.hpp"

namespace bjou {
void StartDefaultCompilation(ArgSet & args) {
    /* args.print(); */

    bjou::DEFAULT_FE frontEnd;
    bjou::DEFAULT_BE backEnd(frontEnd);

    Compilation * save = compilation;

    compilation = new bjou::Compilation(frontEnd, backEnd, args);
    compilation->go();

    compilation = save;
}
} // namespace bjou
