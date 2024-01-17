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
#include <iostream>
#include <oodle/Oodle1.h>

namespace Oodle {

void Oodle1Decoder::Initialize(uint32_t alphabetSize, uint32_t uniqueSymbols) {
	usedSymbolCount = uniqueSymbols;
	symbols.resize(alphabetSize + 2);
	symbolWeights.resize(alphabetSize + 2);
	symbolOccurrences.resize(alphabetSize + 2);
	std::fill(symbolWeights.begin(), symbolWeights.end(), One);
	symbolWeights[0] = 0;
	symbolOccurrences[0] = 4;
	totalOccurrence = symbolOccurrences[0];
	nextRenormWeight = 8;
	decayThreshold = std::max(256u, std::min((alphabetSize - 1) * 32, 15160u));
	rapidRenormInterval = 4;
	renormInterval = std::max(128u, std::min((alphabetSize - 1) * 2, (decayThreshold / 2) - 32));
}

void Oodle1Decoder::Decay() {
	symbolOccurrences[0] /= 2;
	totalOccurrence = symbolOccurrences[0];
	auto highestWeight = 0u;
	auto highestIndex = 0u;
	for (auto idx = 1u; idx <= highestLearnedSymbol; idx++) {
		while (symbolOccurrences[idx] <= 1) {
			if (idx >= highestLearnedSymbol) {
				symbolOccurrences[idx] = 0;
				highestLearnedSymbol--;
				break;
			} else {
				symbolOccurrences[idx] = symbolOccurrences[highestLearnedSymbol];
				symbolOccurrences[highestLearnedSymbol] = 0;
				symbols[idx] = symbols[highestLearnedSymbol];
				highestLearnedSymbol--;
			}
		}
		if (!symbolOccurrences[idx]) {
			break;
		}
		symbolOccurrences[idx] /= 2;
		totalOccurrence += symbolOccurrences[idx];
		if (symbolOccurrences[idx] > highestWeight) {
			highestWeight = symbolOccurrences[idx];
			highestIndex = idx;
		}
	}
	if (highestWeight && (highestIndex != highestLearnedSymbol)) {
		std::swap(symbolOccurrences[highestLearnedSymbol], symbolOccurrences[highestIndex]);
		std::swap(symbols[highestLearnedSymbol], symbols[highestIndex]);
	}
	if ((highestLearnedSymbol != usedSymbolCount) && !symbolOccurrences[0]) {
		symbolOccurrences[0] = 1;
		totalOccurrence++;
	}
	std::fill(symbolWeights.begin() + highestLearnedSymbol + 1, symbolWeights.end(), One);
}

void Oodle1Decoder::Renormalize() {
	const auto quanta = 0x20000 / totalOccurrence;
	symbolWeights[0] = 0;
	auto accumulator = (symbolOccurrences[0] * quanta) / 8;
	for (auto idx = 1u; idx <= highestLearnedSymbol; idx++) {
		symbolWeights[idx] = accumulator;
		accumulator += ((symbolOccurrences[idx] * quanta) / 8);
	}
	if ((rapidRenormInterval * 2) < renormInterval) {
		rapidRenormInterval *= 2;
		nextRenormWeight = totalOccurrence + rapidRenormInterval;
	} else {
		nextRenormWeight = totalOccurrence + renormInterval;
	}
	highestNormalizedSymbol = highestLearnedSymbol;
	std::fill(symbolWeights.begin() + highestLearnedSymbol + 1, symbolWeights.end(), One);
}

uint32_t Oodle1Decoder::Decode(Oodle1Bitstream& bs, uint32_t alphabetSize) {
	if (totalOccurrence >= nextRenormWeight) {
		if (totalOccurrence >= decayThreshold) {
			Decay();
		}
		Renormalize();
	}
	const auto z = bs.Peek(One);
	auto symbolIdx = 0u;
	for (symbolIdx = 0u; symbolIdx <= highestNormalizedSymbol; symbolIdx++) {
		if (symbolWeights[symbolIdx + 1] > z) {
			break;
		}
	}
	bs.Consume(symbolWeights[symbolIdx], symbolWeights[symbolIdx + 1] - symbolWeights[symbolIdx], One);
	symbolOccurrences[symbolIdx]++;
	totalOccurrence++;
	if (symbolIdx) {
		return symbols[symbolIdx];
	} else {
		if (highestLearnedSymbol != highestNormalizedSymbol) {
			const auto b = bs.Get(2);
			if (b) {
				symbolIdx = bs.Get(highestLearnedSymbol - highestNormalizedSymbol) + highestNormalizedSymbol + 1;
				symbolOccurrences[symbolIdx] += 2;
				totalOccurrence += 2;
				return symbols[symbolIdx];
			}
		}
		highestLearnedSymbol++;
		const auto symbol = bs.Get(alphabetSize);
		symbols[highestLearnedSymbol] = symbol;
		symbolOccurrences[highestLearnedSymbol] += 2;
		totalOccurrence += 2;
		if (highestLearnedSymbol == usedSymbolCount) {
			totalOccurrence -= symbolOccurrences[0];
			symbolOccurrences[0] = 0;
		}
		return symbol;
	}
}

void Oodle1Decompressor::Initialize(const uint32_t *header) {
	windowSize = header[0] >> 9;
	// Initialize literal decoders
	litAlphabetSize = header[0] & 0x1FF;
	const auto uniqueLitCount = header[1] & 0x1FF;
	for (auto& decoder : litDecoders) {
		decoder.Initialize(litAlphabetSize, uniqueLitCount);
	}
	// Initialize repeat-length decoders
	auto repLens = header[2];
	for (auto groupIdx = 0u; groupIdx < 4u; groupIdx++) {
		for (auto decoderIdx = 0u; decoderIdx < 16; decoderIdx++) {
			auto& decoder = lenDecoders[(groupIdx * 16) + decoderIdx];
			decoder.Initialize(65, repLens >> 24);
		}
		repLens <<= 8;
	}
	lenDecoders[64].Initialize(65, repLens >> 24);
	// Initialize repeat-offset decoders
	offset1AlphabetSize = std::min(4u, windowSize + 1);
	const auto offset4AlphabetSize = std::min(256u, (windowSize / 4) + 1);
	const auto offset1024AlphabetSize = (windowSize / 1024) + 1;
	const auto largest1KOffset = header[1] >> 19;
	off1Decoder.Initialize(offset1AlphabetSize, offset1AlphabetSize);
	for (auto& decoder : off4Decoders) {
		decoder.Initialize(offset4AlphabetSize, offset4AlphabetSize);
	}
	off1024Decoder.Initialize(offset1024AlphabetSize, largest1KOffset + 1);
}

static void Repeat(uint8_t *output, uint32_t offset, uint32_t length) {
	const uint8_t *input = output - offset;
	while (length) {
		*output = *input;
		output++;
		input++;
		length--;
	}
}

uint32_t Oodle1Decompressor::Decompress(uint8_t *output) {
	const auto lenCode = lenDecoders[lastRepeatCode].Decode(bs, 65);
	lastRepeatCode = lenCode;
	if (!lenCode) {
		const auto lit = litDecoders[bytesOutput & 0x03].Decode(bs, litAlphabetSize);
		output[0] = lit;
		bytesOutput++;
		return 1;
	} else {
		const auto len = RepeatLengthTable[lenCode];
		const auto effectiveWindow = std::min(windowSize, bytesOutput);
		const auto off1 = off1Decoder.Decode(bs, offset1AlphabetSize) + 1;
		const auto off1k = off1024Decoder.Decode(bs, (effectiveWindow / 1024) + 1);
		const auto off4 = off4Decoders[off1k].Decode(bs, std::min(256u, (effectiveWindow / 4) + 1));
		const auto offset = (off1k * 1024) + (off4 * 4) + off1;
		bytesOutput += len;
		Repeat(output, offset, len);
		return len;
	}
}

}
