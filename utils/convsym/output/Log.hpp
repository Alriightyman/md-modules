
/* ------------------------------------------------------------ *
 * ConvSym utility version 2.0									*
 * Output wrapper for simple symbol logging						*
 * ------------------------------------------------------------	*/


struct Output__Log : public OutputWrapper {

public:

	Output__Log() {	// Constructor

	};

	~Output__Log() {	// Destructor

	};

	/**
	 * Main function that generates the output
	 */
	void
	parse(	map<uint32_t, string>& SymbolList,
			const char * fileName,
			uint32_t appendOffset = 0,
			uint32_t pointerOffset = 0,
			const char * opts = "" ) {
	
		const char * lineFormat = *opts ? opts : "%X: %s";
		IO::FileOutput output = IO::FileOutput( fileName );

		for ( auto symbol : SymbolList ) {
			output.writeLine( lineFormat, symbol.first, symbol.second.c_str() );
		}

	}

};
