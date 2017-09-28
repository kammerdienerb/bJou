#undef BJOU_DEBUG_BUILD

#include "SwitchArg.h"
#include <assert.h>

//////////////////////////////////////////////////////////////////////
//BEGIN SwitchArg.cpp
//////////////////////////////////////////////////////////////////////

namespace TCLAP {

SwitchArg::SwitchArg(const std::string& flag, 
	 		         const std::string& name, 
     		   		 const std::string& desc, 
	     	    	 bool _default,
					 Visitor* v )
: Arg(flag, name, desc, false, false, v),
  _value( _default )
{ }

SwitchArg::SwitchArg(const std::string& flag, 
					const std::string& name, 
					const std::string& desc, 
					CmdLineInterface& parser,
					bool _default,
					Visitor* v )
: Arg(flag, name, desc, false, false, v),
  _value( _default )
{ 
	parser.add( this );
}

bool SwitchArg::getValue() { return _value; }

bool SwitchArg::combinedSwitchesMatch(std::string& combinedSwitches )
{
	// make sure this is actually a combined switch
	if ( combinedSwitches[0] != Arg::flagStartString()[0] )
		return false;

	// make sure it isn't a long name 
	if ( combinedSwitches.substr( 0, Arg::nameStartString().length() ) == 
		 Arg::nameStartString() )
		return false;

	// ok, we're not specifying a ValueArg, so we know that we have
	// a combined switch list.  
	for ( unsigned int i = 1; i < combinedSwitches.length(); i++ )
		if ( combinedSwitches[i] == _flag[0] ) 
		{
			// update the combined switches so this one is no longer present
			// this is necessary so that no unlabeled args are matched
			// later in the processing.
			//combinedSwitches.erase(i,1);
			combinedSwitches[i] = Arg::blankChar(); 
			return true;
		}

	// none of the switches passed in the list match. 
	return false;	
}

bool SwitchArg::processArg(int *i, std::vector<std::string>& args)
{
	if ( _ignoreable && Arg::ignoreRest() )
		return false;

	if ( argMatches( args[*i] ) || combinedSwitchesMatch( args[*i] ) )
	{
		// If we match on a combined switch, then we want to return false
		// so that other switches in the combination will also have a
		// chance to match.
		bool ret = false;
		if ( argMatches( args[*i] ) )
			ret = true;

		if ( _alreadySet || ( !ret && combinedSwitchesMatch( args[*i] ) ) )
#ifdef BJOU_DEBUG_BUILD
			throw(CmdLineParseException("Argument already set!", toString()));
#else
        assert(false && "Argument already set!");
#endif

		_alreadySet = true;

		if ( _value == true )
			_value = false;
		else
			_value = true;

		_checkWithVisitor();

		return ret;
	}
	else
		return false;
}

//////////////////////////////////////////////////////////////////////
//End SwitchArg.cpp
//////////////////////////////////////////////////////////////////////

} //namespace TCLAP
