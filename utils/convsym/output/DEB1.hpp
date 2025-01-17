
/* ------------------------------------------------------------ *
 * ConvSym utility version 2.7.2								*
 * Output wrapper for the Debug Information format version 1.0	*
 * ------------------------------------------------------------	*/

#include <map>
#include <cstdint>
#include <string>

#include "../../core/OptsParser.hpp"
#include "../../core/Huffman.hpp"
#include "../../core/BitStream.hpp"
#include "../../core/IO.hpp"
#include "../../core/utils.hpp"

#include "OutputWrapper.hpp"


struct Output__Deb1 : public OutputWrapper {

public:

	Output__Deb1() {	// Constructor

	};

	~Output__Deb1() {	// Destructor

	};

	/**
	 * Main function that generates the output
	 */
	void
	parse(	std::multimap<uint32_t, std::string>& SymbolList,
			const char * fileName,
			uint32_t appendOffset = 0,
			uint32_t pointerOffset = 0,
			const char * opts = "" ) {

		IO::FileOutput output = *OutputWrapper::setupOutput( fileName, appendOffset, pointerOffset );

		/* Parse options from "-inopt" agrument's value */
		bool optFavorLastLabels = false;

		const std::map<std::string, OptsParser::record>
			OptsList {
				{ "favorLastLabels",	{ .type = OptsParser::record::p_bool,	.target =&optFavorLastLabels } }
			};
			
		OptsParser::parse( opts, OptsList );

		/* Write format version token */
		output.writeBEWord( 0xDEB1 );

		/* Allocate space for blocks offsets table */
		auto lastSymbolPtr = SymbolList.rbegin();
		uint16_t lastBlock = (lastSymbolPtr->first) >> 16;

		if (lastBlock > 63) {		// blocks index table is limited to $100 entries (which is enough to cover all the 24-bit addressable space)
			IO::Log( IO::error, "Too many memory blocks to allocate (%02X), truncating to $40 blocks. Some symbols will be lost.", lastBlock+1 );
			lastBlock = 0x3F;
		}

		uint16_t blockOffsets [ 0x40 ] = { 0 };
		uint16_t dataOffsets [ 0x40 ] = { 0 };

		const uint32_t loc_BlockOffsets = output.getCurrentOffset();	// remember the offset where blocks offset table should start
		output.setOffset( sizeof(blockOffsets) + sizeof(dataOffsets), IO::current );	// reserve space to write down offsets tables later

		/* ------------------------------------------------ */
		/* Generate Huffman-codes and create decoding table */
		/* ------------------------------------------------ */

		IO::Log( IO::debug, "Building an encoding table...");

		/* Generate table of character frequencies based on symbol names */
		uint32_t freqTable[0x100] = { 0 };
		for ( auto& Symbol : SymbolList ) {
			for ( auto& Character : Symbol.second ) {
				freqTable[ (int)Character ]++;
			}
			freqTable[ 0x00 ]++;
		}

		/* Generate table of Huffman codes (sorted by code) */
		Huffman::RecordSet codesTable = Huffman::encode( freqTable );

		/* Write down the decoding table */
		const Huffman::Record* characterToRecord[0x100] = { nullptr };	// LUT that links each character to its Huffman-coding record
		for ( auto& entry : codesTable ) {
			characterToRecord[ entry.data ] = &entry;	// assign character this Huffman::Record entity
			output.writeBEWord( entry.code );			// write Huffman-code
			output.writeByte( entry.codeLength );		// write Huffman-code length (in bits)
			output.writeByte( entry.data );				// write down original character
		}
		output.writeWord( -1 );			// write 0xFFFF at the end of Huffman-table to stop searching and cause error

		/* ------------------------------------- */
		/* Generate per block symbol information */
		/* ------------------------------------- */

		{
			IO::Log( IO::debug, "Generating symbol data blocks...");

			auto SymbolPtr = SymbolList.begin();

			/* For 64kb block within symbols range */
			for ( uint16_t block = 0x00; block <= lastBlock; block++ ) {

				/* Align block on even address */
				if ( output.getCurrentOffset() & 1 ) {
					output.writeByte( 0x00 );
				}

				IO::Log( IO::debug, "Block %02X:", block);
				uint32_t loc_Block = output.getCurrentOffset();	// remember offset, where this block starts ...

				std::vector<uint16_t> offsetsData;
				std::vector<uint8_t> symbolsData;

				/* For every symbol within the block ... */
				for ( ; block == (SymbolPtr->first>>16) && (SymbolPtr != SymbolList.cend()); ++SymbolPtr ) {

					IO::Log( IO::debug, "\t%08X\t%s", SymbolPtr->first, SymbolPtr->second.c_str() );

					/* 
					 * For records with the same offsets, fetch only the last or the first processed symbol,
					 * depending "favor last labels" option ...
					 */
					if ( (optFavorLastLabels && std::next(SymbolPtr) != SymbolList.end()
								&& std::next(SymbolPtr)->first == SymbolPtr->first) ||
						 (!optFavorLastLabels && SymbolPtr != SymbolList.begin()
								&& std::prev(SymbolPtr)->first == SymbolPtr->first) ) {
						continue;
					}

					BitStream SymbolsHeap;

					/* Add offset to the offsets block */
					offsetsData.push_back( swap16( (uint16_t) SymbolPtr->first & 0xFFFF) );

					/* Encode each symbol character with the generated Huffman-codes and store it in the bitsteam */
					for ( auto& Character : SymbolPtr->second ) {
						auto *record = characterToRecord[ (int)Character ];
						SymbolsHeap.pushCode( record->code, record->codeLength );
					}

					/* Finally, add null-terminator */
					{
						auto *record = characterToRecord[ 0x00 ];
						SymbolsHeap.pushCode( record->code, record->codeLength );
					}

					/* Push this symbol to the data buffer */
					symbolsData.push_back( SymbolsHeap.size()+1 );	// write down symbols size
					for ( auto t = SymbolsHeap.begin(); t != SymbolsHeap.end(); t++ ) {
						symbolsData.push_back( *t );
					}

				}

				/* Write offsets block and their corresponding encoded symbols heap */
				if ( offsetsData.size() > 0 ) {

					/* Add zero offset to finalize the block */
					offsetsData.push_back( 0x0000 );
					
					/* Check for pointer capacity limits */
					if ( (loc_Block - loc_BlockOffsets)>>1 > 0xFFFF
						|| (loc_Block+offsetsData.size()*2 - loc_BlockOffsets)>>1 > 0xFFFF ) {
						IO::Log( IO::error, "Block %02X is either too large, or symbol file has exceeded its size limits; unable to write the block", block );
						continue;
					}

					/* Setup pointers to the blocks */
					blockOffsets[block] = swap16( (loc_Block - loc_BlockOffsets)>>1 );
					dataOffsets[block] = swap16( (loc_Block+offsetsData.size()*2 - loc_BlockOffsets)>>1 );

					/* Write down block offsets and symbol data */
					output.writeData( &offsetsData[0], offsetsData.size()*sizeof(uint16_t) );
					output.writeData( &symbolsData[0], symbolsData.size() );

				}
			}
		}
		
		/* Finally, write down block offsets table in the header */
		output.setOffset( loc_BlockOffsets, IO::start );
		output.writeData( blockOffsets, sizeof(blockOffsets) );
		output.writeData( dataOffsets, sizeof(dataOffsets) );

	};

};
