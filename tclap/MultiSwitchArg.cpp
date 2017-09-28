#undef BJOU_DEBUG_BUILD

#include "MultiSwitchArg.h"

//////////////////////////////////////////////////////////////////////
//BEGIN MultiSwitchArg.cpp
//////////////////////////////////////////////////////////////////////

namespace TCLAP {

inline MultiSwitchArg::MultiSwitchArg(const std::string& flag,
					const std::string& name,
					const std::string& desc,
					int init,
					Visitor* v )
: SwitchArg(flag, name, desc, false, v),
_value( init )
{ }

inline MultiSwitchArg::MultiSwitchArg(const std::string& flag,
					const std::string& name, 
					const std::string& desc, 
					CmdLineInterface& parser,
					int init,
					Visitor* v )
: SwitchArg(flag, name, desc, false, v),
_value( init )
{ 
	parser.add( this );
}

inline int MultiSwitchArg::getValue() { return _value; }

inline bool MultiSwitchArg::processArg(int *i, std::vector<std::string>& args)
{
	if ( _ignoreable && Arg::ignoreRest() )
		return false;

	if ( argMatches( args[*i] ))
	{
		// so the isSet() method will work
		_alreadySet = true;

		// Matched argument: increment value.
		++_value;

		_checkWithVisitor();

		return true;
	}
	else if ( combinedSwitchesMatch( args[*i] ) )
	{
		// so the isSet() method will work
		_alreadySet = true;

		// Matched argument: increment value.
		++_value;

		// Check for more in argument and increment value.
		while ( combinedSwitchesMatch( args[*i] ) ) 
			++_value;

		_checkWithVisitor();

		return false;
	}
	else
		return false;
}

std::string MultiSwitchArg::shortID(const std::string& val) const
{
	std::string id = Arg::shortID() + " ... ";

	return id;
}

std::string MultiSwitchArg::longID(const std::string& val) const
{
	std::string id = Arg::longID() + "  (accepted multiple times)";

	return id;
}

//////////////////////////////////////////////////////////////////////
//END MultiSwitchArg.cpp
//////////////////////////////////////////////////////////////////////

} //namespace TCLAP
