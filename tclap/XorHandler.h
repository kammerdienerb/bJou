
/****************************************************************************** 
 * 
 *  file:  XorHandler.h
 * 
 *  Copyright (c) 2003, Michael E. Smoot .
 *  Copyright (c) 2004, Michael E. Smoot, Daniel Aarno.
 *  All rights reverved.
 * 
 *  See the file COPYING in the top directory of this distribution for
 *  more information.
 *  
 *  THE SOFTWARE IS PROVIDED _AS IS_, WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 *  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 *  DEALINGS IN THE SOFTWARE.  
 *  
 *****************************************************************************/ 

#ifndef TCLAP_XORHANDLER_H
#define TCLAP_XORHANDLER_H

#include "Arg.h"
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

namespace TCLAP {

/**
 * This class handles lists of Arg's that are to be XOR'd on the command
 * line.  This is used by CmdLine and you shouldn't ever use it.
 */
class XorHandler
{
	protected:

		/**
		 * The list of of lists of Arg's to be or'd together.
		 */
		std::vector< std::vector<Arg*> > _orList;

	public:

		/**
		 * Constructor.  Does nothing.
		 */
		XorHandler( ) {}

		/**
		 * Add a list of Arg*'s that will be orred together.
		 * \param ors - list of Arg* that will be xor'd.
		 */
		void add( std::vector<Arg*>& ors );
			
		/**
		 * Checks whether the specified Arg is in one of the xor lists and
		 * if it does match one, returns the size of the xor list that the
		 * Arg matched.  If the Arg matches, then it also sets the rest of
		 * the Arg's in the list. You shouldn't use this.  
		 * \param a - The Arg to be checked.
		 */
		int check( const Arg* a );

		/**
		 * Returns the XOR specific short usage.
		 */
		std::string shortUsage();

		/**
		 * Prints the XOR specific long usage.
		 * \param os - Stream to print to.
		 */
		void printLongUsage(std::ostream& os);

		/**
		 * Simply checks whether the Arg is contained in one of the arg
		 * lists.
		 * \param a - The Arg to be checked.
		 */
		bool contains( const Arg* a );

		std::vector< std::vector<Arg*> >& getXorList(); 

};
}
#endif