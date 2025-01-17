
/* ------------------------------------------------------------ *
 * Debugging Modules Utilities Core								*
 * Argument values parser helper 								*
 * (c) 2017-2018, Vladikcomper									*
 * ------------------------------------------------------------	*/

#pragma once

#include <map>
#include <string>

#include "IO.hpp"


namespace ArgvParser {

	/* Structure that handles parameter definitions */
	struct record {
		enum { flag, hexNumber, hexRange, string } type;
		void * target;
		void * target2;
	};

	/**
	 * Function to parse command line arguments (argv, argc) according to parameter data structures
	 */
	inline void parse(const char ** argv, int argc, std::map<std::string,record> ParametersList ) {

		for (int i=0; i<argc; ++i) {
			
			auto parameter = ParametersList.find( argv[i] );
			#define _GET_NEXT_ARGUMENT \
				if (++i==argc) { \
					IO::Log(IO::fatal, "Expected value for parameter \"%s\"", parameter->first.c_str()); \
					break; \
				}

			if ( parameter != ParametersList.end() ) {
				switch ( parameter->second.type ) {
					case record::flag:
						*((bool*)parameter->second.target) = true;
						break;

					case record::hexNumber:
						_GET_NEXT_ARGUMENT
						sscanf( argv[i], "%x", (unsigned int*)parameter->second.target );
						break;

					case record::hexRange:
						_GET_NEXT_ARGUMENT
						sscanf( argv[i], "%x", (unsigned int*)parameter->second.target );
						_GET_NEXT_ARGUMENT
						sscanf( argv[i], "%x", (unsigned int*)parameter->second.target2 );
						break;

					case record::string:
						_GET_NEXT_ARGUMENT
						*((std::string*)parameter->second.target) = argv[i];
						break;
						
					default:
						throw "Incorrect or broken parameters data";
				}
			}
			else {
				IO::Log( IO::warning, "Unknown parameter \"%s\" passed. Parameter is ignored", argv[i] );
			}

			#undef _GET_NEXT_ARGUMENT
		}
	}
}