#undef BJOU_DEBUG_BUILD

#include "CmdLine.h"


///////////////////////////////////////////////////////////////////////////////
//Begin CmdLine.cpp
///////////////////////////////////////////////////////////////////////////////

namespace TCLAP {

CmdLine::CmdLine(const std::string& m,
				        char delim,
						const std::string& v,
						bool help )
: _progName("not_set_yet"),
  _message(m),
  _version(v),
  _numRequired(0),
  _delimiter(delim),
  _userSetOutput(false),
  _helpAndVersion(help)
{
	_constructor();
}

CmdLine::~CmdLine()
{
	ArgListIterator argIter;
	VisitorListIterator visIter;

	for( argIter = _argDeleteOnExitList.begin();
		 argIter != _argDeleteOnExitList.end();
		 ++argIter)
		delete *argIter;

	for( visIter = _visitorDeleteOnExitList.begin();
		 visIter != _visitorDeleteOnExitList.end();
		 ++visIter)
		delete *visIter;

	if ( !_userSetOutput )
		delete _output;
}

void CmdLine::_constructor()
{
	_output = new StdOutput;

	Arg::setDelimiter( _delimiter );

	Visitor* v;

	if ( _helpAndVersion )
	{
		v = new HelpVisitor( this, &_output );
		SwitchArg* help = new SwitchArg("h","help",
						"Displays usage information and exits.",
						false, v);
		add( help );
		deleteOnExit(help);
		deleteOnExit(v);

		v = new VersionVisitor( this, &_output );
		SwitchArg* vers = new SwitchArg("","version",
					"Displays version information and exits.",
					false, v);
		add( vers );
		deleteOnExit(vers);
		deleteOnExit(v);
	}

	v = new IgnoreRestVisitor();
	SwitchArg* ignore  = new SwitchArg(Arg::flagStartString(),
					   Arg::ignoreNameString(),
			   "Ignores the rest of the labeled arguments following this flag.",
					   false, v);
	add( ignore );
	deleteOnExit(ignore);
	deleteOnExit(v);
}

void CmdLine::xorAdd( std::vector<Arg*>& ors )
{
	_xorHandler.add( ors );

	for (ArgVectorIterator it = ors.begin(); it != ors.end(); it++)
	{
		(*it)->forceRequired();
		(*it)->setRequireLabel( "OR required" );

		add( *it );
	}
}

void CmdLine::xorAdd( Arg& a, Arg& b )
{
    std::vector<Arg*> ors;
    ors.push_back( &a );
    ors.push_back( &b );
	xorAdd( ors );
}

void CmdLine::add( Arg& a )
{
	add( &a );
}

void CmdLine::add( Arg* a )
{
	for( ArgListIterator it = _argList.begin(); it != _argList.end(); it++ )
		if ( *a == *(*it) )
#ifdef BJOU_DEBUG_BUILD
			throw( SpecificationException(
			       	"Argument with same flag/name already exists!",
					a->longID() ) );
#else
        assert(false && "Argument with same flag/name already exists!");
#endif

	a->addToList( _argList );

	if ( a->isRequired() )
		_numRequired++;
}

void CmdLine::parse(int argc, char** argv)
{
#ifdef BJOU_DEBUG_BUILD
	try {
#endif

	_progName = argv[0];

	// this step is necessary so that we have easy access to mutable strings.
	std::vector<std::string> args;
  	for (int i = 1; i < argc; i++)
		args.push_back(argv[i]);

	int requiredCount = 0;

  	for (int i = 0; static_cast<unsigned int>(i) < args.size(); i++)
	{
		bool matched = false;
		for (ArgListIterator it = _argList.begin(); it != _argList.end(); it++)
        {
			if ( (*it)->processArg( &i, args ) )
			{
				requiredCount += _xorHandler.check( *it );
				matched = true;
				break;
			}
        }

		// checks to see if the argument is an empty combined switch ...
		// and if so, then we've actually matched it
		if ( !matched && _emptyCombined( args[i] ) )
			matched = true;

		if ( !matched && !Arg::ignoreRest() )
#ifdef BJOU_DEBUG_BUILD
			throw(CmdLineParseException("Couldn't find match for argument",
			                             args[i]));
#else
        assert(false && "Couldn't find match for argument");
#endif
    }

	if ( requiredCount < _numRequired )
#ifdef BJOU_DEBUG_BUILD
		throw(CmdLineParseException("One or more required arguments missing!"));
#else
        assert(false && "One or more required arguments missing!");
#endif

	if ( requiredCount > _numRequired )
#ifdef BJOU_DEBUG_BUILD
		throw(CmdLineParseException("Too many arguments!"));
#else
        assert(false && "Too many arguments!");
#endif

#ifdef BJOU_DEBUG_BUILD
	} catch ( ArgException e ) { _output->failure(*this,e); exit(1); }
#endif
}

bool CmdLine::_emptyCombined(const std::string& s)
{
	if ( s[0] != Arg::flagStartChar() )
		return false;

	for ( int i = 1; static_cast<unsigned int>(i) < s.length(); i++ )
		if ( s[i] != Arg::blankChar() )
			return false;

	return true;
}

void CmdLine::deleteOnExit(Arg* ptr)
{
	_argDeleteOnExitList.push_back(ptr);
}

void CmdLine::deleteOnExit(Visitor* ptr)
{
	_visitorDeleteOnExitList.push_back(ptr);
}

CmdLineOutput* CmdLine::getOutput()
{
	return _output;
}

void CmdLine::setOutput(CmdLineOutput* co)
{
	_userSetOutput = true;
	_output = co;
}

std::string& CmdLine::getVersion()
{
	return _version;
}

std::string& CmdLine::getProgramName()
{
	return _progName;
}

std::list<Arg*>& CmdLine::getArgList()
{
	return _argList;
}

XorHandler& CmdLine::getXorHandler()
{
	return _xorHandler;
}

char CmdLine::getDelimiter()
{
	return _delimiter;
}

std::string& CmdLine::getMessage()
{
	return _message;
}

bool CmdLine::hasHelpAndVersion()
{
	return _helpAndVersion;
}

///////////////////////////////////////////////////////////////////////////////
//End CmdLine.cpp
///////////////////////////////////////////////////////////////////////////////



} //namespace TCLAP
