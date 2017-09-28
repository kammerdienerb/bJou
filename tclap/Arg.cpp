#undef BJOU_DEBUG_BUILD

//////////////////////////////////////////////////////////////////////
//BEGIN Arg.cpp
//////////////////////////////////////////////////////////////////////

#include "Arg.h"

#include <assert.h>

namespace TCLAP {

Arg::Arg(const std::string& flag, 
         const std::string& name, 
         const std::string& desc, 
         bool req, 
         bool valreq,
         Visitor* v) :
  _flag(flag),
  _name(name),
  _description(desc),
  _required(req),
  _requireLabel("required"),
  _valueRequired(valreq),
  _alreadySet(false),
  _visitor( v ),
  _ignoreable(true),
  _xorSet(false),
  _acceptsMultipleValues(false)
{
	if ( _flag.length() > 1 )
#ifdef BJOU_DEBUG_BUILD
		throw(SpecificationException(
				"Argument flag can only be one character long", toString() ) );
#else
        assert(false && "Argument flag can only be one character long");
#endif

	if ( _name != ignoreNameString() &&
		 ( _flag == Arg::flagStartString() || 
		   _flag == Arg::nameStartString() || 
		   _flag == " " ) )
#ifdef BJOU_DEBUG_BUILD
		throw(SpecificationException("Argument flag cannot be either '" +
							Arg::flagStartString() + "' or '" + 
							Arg::nameStartString() + "' or a space.",
							toString() ) );
#else
        assert(false && "Invalid argument flag");
#endif

	if ( ( _name.find( Arg::flagStartString(), 0 ) != std::string::npos ) ||
		 ( _name.find( Arg::nameStartString(), 0 ) != std::string::npos ) ||
		 ( _name.find( " ", 0 ) != std::string::npos ) )
#ifdef BJOU_DEBUG_BUILD
		throw(SpecificationException("Argument name cannot contain either '" + 
							Arg::flagStartString() + "' or '" + 
							Arg::nameStartString() + "' or space.",
							toString() ) );
#else
    assert(false && "Invalid argument flag");
#endif

}

Arg::~Arg() { }

std::string Arg::shortID( const std::string& valueId ) const
{
	std::string id = "";

	if ( _flag != "" )
		id = Arg::flagStartString() + _flag;
	else
		id = Arg::nameStartString() + _name;

	std::string delim = " "; 
	delim[0] = Arg::delimiter(); // ugly!!!
	
	if ( _valueRequired )
		id += delim + "<" + valueId  + ">";

	if ( !_required )
		id = "[" + id + "]";

	return id;
}

std::string Arg::longID( const std::string& valueId ) const
{
	std::string id = "";

	if ( _flag != "" )
	{
		id += Arg::flagStartString() + _flag;

		if ( _valueRequired )
			id += " <" + valueId + ">";
		
		id += ",  ";
	}

	id += Arg::nameStartString() + _name;

	if ( _valueRequired )
		id += " <" + valueId + ">";
			
	return id;

}

bool Arg::operator==(const Arg& a) const
{
	if ( ( _flag != "" && _flag == a._flag ) || _name == a._name)
		return true;
	else
		return false;
}

std::string Arg::getDescription() const 
{
	std::string desc = "";
	if ( _required )
		desc = "(" + _requireLabel + ")  ";

//	if ( _valueRequired )
//		desc += "(value required)  ";

	desc += _description;
	return desc; 
}

const std::string& Arg::getFlag() const { return _flag; }

const std::string& Arg::getName() const { return _name; } 

bool Arg::isRequired() const { return _required; }

bool Arg::isValueRequired() const { return _valueRequired; }

bool Arg::isSet() const 
{ 
	if ( _alreadySet && !_xorSet )
		return true;
	else
		return false;
}

bool Arg::isIgnoreable() const { return _ignoreable; }

void Arg::setRequireLabel( const std::string& s) 
{ 
	_requireLabel = s;
}

bool Arg::argMatches( const std::string& argFlag ) const
{
	if ( ( argFlag == Arg::flagStartString() + _flag && _flag != "" ) ||
		 argFlag == Arg::nameStartString() + _name )
		return true;
	else
		return false;
}

std::string Arg::toString() const
{
	std::string s = "";

	if ( _flag != "" )
		s += Arg::flagStartString() + _flag + " ";

	s += "(" + Arg::nameStartString() + _name + ")";

	return s;
}

void Arg::_checkWithVisitor() const
{
	if ( _visitor != NULL )
		_visitor->visit();
}

/**
 * Implementation of trimFlag.
 */
void Arg::trimFlag(std::string& flag, std::string& value) const
{
	int stop = 0;
	for ( int i = 0; static_cast<unsigned int>(i) < flag.length(); i++ )
		if ( flag[i] == Arg::delimiter() )
		{
			stop = i;
			break;
		}

	if ( stop > 1 )
	{
		value = flag.substr(stop+1);
		flag = flag.substr(0,stop);
	}

}

/**
 * Implementation of _hasBlanks.
 */
bool Arg::_hasBlanks( const std::string& s ) const
{
	for ( int i = 1; static_cast<unsigned int>(i) < s.length(); i++ )
		if ( s[i] == Arg::blankChar() )
			return true;

	return false;
}

void Arg::forceRequired()
{
	_required = true;
}

void Arg::xorSet()
{
	_alreadySet = true;
	_xorSet = true;
}

/**
 * Overridden by Args that need to added to the end of the list.
 */
void Arg::addToList( std::list<Arg*>& argList ) const
{
	argList.push_front( const_cast<Arg*>(this) );
}

bool Arg::allowMore()
{
	return false;
}

bool Arg::acceptsMultipleValues()
{
	return _acceptsMultipleValues;
}

//////////////////////////////////////////////////////////////////////
//END Arg.cpp
//////////////////////////////////////////////////////////////////////

} //namespace TCLAP
