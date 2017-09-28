#undef BJOU_DEBUG_BUILD

#include "XorHandler.h"

//////////////////////////////////////////////////////////////////////
//BEGIN XOR.cpp
//////////////////////////////////////////////////////////////////////

namespace TCLAP {

void XorHandler::add( std::vector<Arg*>& ors )
{ 
	_orList.push_back( ors );
}

int XorHandler::check( const Arg* a ) 
{
	// iterate over each XOR list
	for ( int i = 0; static_cast<unsigned int>(i) < _orList.size(); i++ )
	{
		// if the XOR list contains the arg..
		ArgVectorIterator ait = std::find( _orList[i].begin(), 
		                                   _orList[i].end(), a );
		if ( ait != _orList[i].end() )
		{
			// go through and set each arg that is not a
			for ( ArgVectorIterator it = _orList[i].begin(); 
				  it != _orList[i].end(); 
				  it++ )	
				if ( a != (*it) )
					(*it)->xorSet();

			// return the number of required args that have now been set
			if ( (*ait)->allowMore() )
				return 0;
			else
				return static_cast<int>(_orList[i].size());
		}
	}

	if ( a->isRequired() )
		return 1;
	else
		return 0;
}

bool XorHandler::contains( const Arg* a )
{
	for ( int i = 0; static_cast<unsigned int>(i) < _orList.size(); i++ )
		for ( ArgVectorIterator it = _orList[i].begin(); 
			  it != _orList[i].end(); 
			  it++ )	
			if ( a == (*it) )
				return true;

	return false;
}

std::vector< std::vector<Arg*> >& XorHandler::getXorList() 
{
	return _orList;
}



//////////////////////////////////////////////////////////////////////
//END XOR.cpp
//////////////////////////////////////////////////////////////////////

} //namespace TCLAP
