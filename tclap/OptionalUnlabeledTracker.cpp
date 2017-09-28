#undef BJOU_DEBUG_BUILD

#include "OptionalUnlabeledTracker.h"
#include <assert.h>

namespace TCLAP {

void OptionalUnlabeledTracker::check( bool req, const std::string& argName )
{
    if ( OptionalUnlabeledTracker::alreadyOptional() )
#ifdef BJOU_DEBUG_BUILD
        throw( SpecificationException(
	"You can't specify ANY Unlabeled Arg following an optional Unlabeled Arg",
	                argName ) );
#else
    assert(false && "You can't specify ANY Unlabeled Arg following an optional Unlabeled Arg");
#endif

    if ( !req )
        OptionalUnlabeledTracker::gotOptional();
}
    
}
