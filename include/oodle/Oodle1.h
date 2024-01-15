// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <https://unlicense.org>
#ifndef LIBOODLE_OODLE1_H
#define LIBOODLE_OODLE1_H

#include <array>
#include <cstdint>
#include <vector>

namespace Oodle {

class Oodle1Bitstream {
public:
	explicit Oodle1Bitstream(const uint8_t *input_) : input(input_) {
		sr = *input >> 1;
		lsb = *input & 0x01;
		srModulus = 0x80;
		input++;
	}

	void Ingest() {
		while (srModulus <= 0x800000) {
			sr = (sr << 1) | lsb;
			const auto b = *input;
			sr = (sr << 7) | (b >> 1);
			lsb = b & 0x01;
			srModulus <<= 8;
			input++;
		}
	}

	uint32_t Peek(uint32_t one) {
		Ingest();
		const auto scale = (srModulus / one);
		const auto z = std::min((sr / scale), one - 1);
		return z;
	}

	void Consume(uint32_t minZ, uint32_t spanZ, uint32_t one) {
		const auto scale = (srModulus / one);
		const auto scaledZ = (minZ * scale);
		sr -= scaledZ;
		if (minZ < (one - spanZ)) {
			srModulus = spanZ * scale;
		} else {
			srModulus -= scaledZ;
		}
	}

	uint32_t Get(uint32_t one) {
		Ingest();
		const auto scale = (srModulus / one);
		const auto z = std::min((sr / scale), one - 1);
		const auto scaledZ = (z * scale);
		sr -= scaledZ;
		if (z < (one - 1)) {
			srModulus = scale;
		} else {
			srModulus -= scaledZ;
		}
		return z;
	}

private:
	const uint8_t *input = nullptr;
	uint32_t sr = 0;
	uint32_t srModulus = 0;
	uint8_t lsb = 0;
};

class Oodle1Decoder {
public:
	static constexpr auto One = 0x4000u;

	Oodle1Decoder() = default;

	void Initialize(uint32_t alphabetSize, uint32_t uniqueSymbols);
	void Decay();
	void Renormalize();
	uint32_t Decode(Oodle1Bitstream& bs, uint32_t alphabetSize);

private:
	uint32_t usedSymbolCount = 0u;
	std::vector<uint8_t> symbols;
	std::vector<uint16_t> symbolWeights;		// SW
	std::vector<uint16_t> symbolOccurrences;	// LSW
	uint32_t totalOccurrence = 0u;				// TLW
	uint32_t highestLearnedSymbol = 0u;			// HLS
	uint32_t highestNormalizedSymbol = 0u;		// HLSN
	uint32_t nextRenormWeight = 0u;				// NRW
	uint32_t decayThreshold = 0u;				// DT
	uint32_t rapidRenormInterval = 0u;			// RRI
	uint32_t renormInterval = 0u;				// RI
};

class Oodle1Decompressor {
public:
	explicit Oodle1Decompressor(Oodle1Bitstream& bs) : bs(bs) { }

	void Initialize(const uint32_t *header);
	uint32_t Decompress(uint8_t *output);

private:
	static constexpr uint32_t RepeatLengthTable[65] = {
		 0,  2,  3,  4,  5,   6,  7,   8,
		 9, 10, 11, 12, 13,  14,  15,  16,
		17, 18, 19, 20, 21,  22,  23,  24,
		25, 26, 27, 28, 29,  30,  31,  32,
		33, 34, 35, 36, 37,  38,  39,  40,
		41, 42, 43, 44, 45,  46,  47,  48,
		49, 50, 51, 52, 53,  54,  55,  56,
		57, 58, 59, 60, 61, 128, 192, 256, 512
	};

	Oodle1Bitstream& bs;
	std::array<Oodle1Decoder,4> litDecoders;
	std::array<Oodle1Decoder,65> lenDecoders;
	Oodle1Decoder off1Decoder;
	std::array<Oodle1Decoder,256> off4Decoders;
	Oodle1Decoder off1024Decoder;

	uint32_t windowSize = 0x7fffffu;
	uint32_t litAlphabetSize = 256u;
	uint32_t offset1AlphabetSize = 0u;
	uint32_t bytesOutput = 0u;
	uint32_t lastRepeatCode = 0u;
};

}

#endif
