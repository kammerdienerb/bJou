
/******************************************************************************
 *
 *  file:  CmdLine.h
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

#ifndef TCLAP_CMDLINE_H
#define TCLAP_CMDLINE_H

#include "SwitchArg.h"
#include "MultiSwitchArg.h"
#include "UnlabeledValueArg.h"
#include "UnlabeledMultiArg.h"

#include "XorHandler.h"
#include "HelpVisitor.h"
#include "VersionVisitor.h"
#include "IgnoreRestVisitor.h"

#include "CmdLineOutput.h"
#include "StdOutput.h"

#include "Constraint.h"
#include "ValuesConstraint.h"

#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace TCLAP {

/**
 * The base class that manages the command line definition and passes
 * along the parsing to the appropriate Arg classes.
 */
class CmdLine : public CmdLineInterface
{
	protected:

		/**
		 * The list of arguments that will be tested against the
		 * command line.
		 */
		std::list<Arg*> _argList;

		/**
		 * The name of the program.  Set to argv[0].
		 */
		std::string _progName;

		/**
		 * A message used to describe the program.  Used in the usage output.
		 */
		std::string _message;

		/**
		 * The version to be displayed with the --version switch.
		 */
		std::string _version;

		/**
		 * The number of arguments that are required to be present on
		 * the command line. This is set dynamically, based on the
		 * Args added to the CmdLine object.
		 */
		int _numRequired;

		/**
		 * The character that is used to separate the argument flag/name
		 * from the value.  Defaults to ' ' (space).
		 */
		char _delimiter;

		/**
		 * The handler that manages xoring lists of args.
		 */
		XorHandler _xorHandler;

		/**
		 * A list of Args to be explicitly deleted when the destructor
		 * is called.  At the moment, this only includes the three default
		 * Args.
		 */
		std::list<Arg*> _argDeleteOnExitList;

		/**
		 * A list of Visitors to be explicitly deleted when the destructor
		 * is called.  At the moment, these are the Vistors created for the
		 * default Args.
		 */
		std::list<Visitor*> _visitorDeleteOnExitList;

		/**
		 * Object that handles all output for the CmdLine.
		 */
		CmdLineOutput* _output;

		/**
		 * Checks whether a name/flag string matches entirely matches
		 * the Arg::blankChar.  Used when multiple switches are combined
		 * into a single argument.
		 * \param s - The message to be used in the usage.
		 */
		bool _emptyCombined(const std::string& s);

		/**
		 * Perform a delete ptr; operation on ptr when this object is deleted.
		 */
		void deleteOnExit(Arg* ptr);

		/**
		 * Perform a delete ptr; operation on ptr when this object is deleted.
		 */
		void deleteOnExit(Visitor* ptr);

	private:

		/**
		 * Encapsulates the code common to the constructors (which is all
		 * of it).
		 */
		void _constructor();

		/**
		 * Is set to true when a user sets the output object. We use this so
		 * that we don't delete objects that are created outside of this lib.
		 */
		bool _userSetOutput;

		/**
		 * Whether or not to automatically create help and version switches.
		 */
		bool _helpAndVersion;

	public:

		/**
		 * Command line constructor. Defines how the arguments will be
		 * parsed.
		 * \param message - The message to be used in the usage
		 * output.
		 * \param delimiter - The character that is used to separate
		 * the argument flag/name from the value.  Defaults to ' ' (space).
		 * \param version - The version number to be used in the
		 * --version switch.
		 * \param helpAndVersion - Whether or not to create the Help and
		 * Version switches. Defaults to true.
		 */
		CmdLine(const std::string& message,
				const char delimiter = ' ',
				const std::string& version = "none",
				bool helpAndVersion = true);

		/**
		 * Deletes any resources allocated by a CmdLine object.
		 */
		virtual ~CmdLine();

		/**
		 * Adds an argument to the list of arguments to be parsed.
		 * \param a - Argument to be added.
		 */
		void add( Arg& a );

		/**
		 * An alternative add.  Functionally identical.
		 * \param a - Argument to be added.
		 */
		void add( Arg* a );

		/**
		 * Add two Args that will be xor'd.  If this method is used, add does
		 * not need to be called.
		 * \param a - Argument to be added and xor'd.
		 * \param b - Argument to be added and xor'd.
		 */
		void xorAdd( Arg& a, Arg& b );

		/**
		 * Add a list of Args that will be xor'd.  If this method is used,
		 * add does not need to be called.
		 * \param xors - List of Args to be added and xor'd.
		 */
		void xorAdd( std::vector<Arg*>& xors );

		/**
		 * Parses the command line.
		 * \param argc - Number of arguments.
		 * \param argv - Array of arguments.
		 */
		void parse(int argc, char** argv);

		/**
		 *
		 */
		CmdLineOutput* getOutput();

		/**
		 *
		 */
		void setOutput(CmdLineOutput* co);

		/**
		 *
		 */
		std::string& getVersion();

		/**
		 *
		 */
		std::string& getProgramName();

		/**
		 *
		 */
		std::list<Arg*>& getArgList();

		/**
		 *
		 */
		XorHandler& getXorHandler();

		/**
		 *
		 */
		char getDelimiter();

		/**
		 *
		 */
		std::string& getMessage();

		/**
		 *
		 */
		bool hasHelpAndVersion();
};

}

#endif
