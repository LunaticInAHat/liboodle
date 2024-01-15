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
#include "Granny.h"
#include "Buffer.h"
#include "Format.h"

bool GrannySectionHeader::Load(Buffer& buffer, uint32_t totalFileSize) {
	encoding = (Encoding)buffer.ReadU32();
	fileOffset = buffer.ReadU32();
	fileSize = buffer.ReadU32();
	memSize = buffer.ReadU32();
	alignment = buffer.ReadU32();
	stream0Stop = buffer.ReadU32();
	stream1Stop = buffer.ReadU32();
	relocOffset = buffer.ReadU32();
	relocCount = buffer.ReadU32();
	marshalOffset = buffer.ReadU32();
	marshalCount = buffer.ReadU32();
	if ((fileOffset > totalFileSize) || ((fileOffset + fileSize) > totalFileSize)) {
		std::cerr << Formatted("Granny section file offset / size are invalid (%08x + %x)", fileOffset, fileSize) << std::endl;
		return false;
	} else if (memSize < fileSize) {
		std::cerr << Formatted("Granny section memory size (%x) is invalid", memSize) << std::endl;
		return false;
	} else if ((relocOffset > totalFileSize) || ((relocOffset + (relocCount * 12)) > totalFileSize)) {
		std::cerr << Formatted("Granny2 section relocation table offset / size are invalid (%08x + %d entries)", relocOffset, relocCount) << std::endl;
		return false;
	} else if ((marshalOffset > totalFileSize) || ((marshalOffset + (marshalCount * 12)) > totalFileSize)) {
		std::cerr << Formatted("Granny2 section relocation table offset / size are invalid (%08x + %d entries)", relocOffset, relocCount) << std::endl;
		return false;
	} else if (marshalCount > 0) {
		std::cerr << Formatted("Granny2 section has %d marshal headers, which are unsupported", marshalCount) << std::endl;
		return false;
	}
	return true;
}


bool GrannyFile::LoadFromBytes(const std::vector<uint8_t>& raw) {
	if (raw.size() < 64) {
		std::cerr << "Granny file is implausibly small" << std::endl;
		return false;
	}
	Buffer buffer(raw);
	std::array<uint8_t,SignatureLength> signature;
	buffer.Read(signature);
	if (signature != SignatureLE) {
		std::cerr << "Granny file has invalid magic bytes" << std::endl;
		return false;
	}
	totalHeaderSize = buffer.ReadU32();
	buffer.ReadPadding(12);
	dataBase = buffer.Tell();	// This point appears to mark the end of one kind of header, and the beginning of another?
	version = buffer.ReadU32();
	if (version != 6) {
		std::cerr << Formatted("Granny file has unsupported version %d", version) << std::endl;
		return false;
	}
	totalFileSize = buffer.ReadU32();
	if (totalFileSize != buffer.Size()) {
		std::cerr << Formatted("Granny file claims length %d, but is actually %d", totalFileSize, buffer.Size()) << std::endl;
		return false;
	}
	crc = buffer.ReadU32();
	const auto sectionHdrOffset = buffer.ReadU32() + dataBase;
	const auto sectionCount = buffer.ReadU32();
	rootNodeType = buffer.ReadU64();
	rootNodeObject = buffer.ReadU64();
	userTag = buffer.ReadU32();
	buffer.Read(userData);

	if ((sectionHdrOffset < buffer.Tell()) || (sectionHdrOffset >= totalFileSize) || ((sectionHdrOffset + (sectionCount * SectionHeaderSize)) > totalFileSize)) {
		std::cerr << Formatted("Granny file has invalid section-header offset / count %x + %d", sectionHdrOffset, sectionCount) << std::endl;
		return false;
	} else if (totalHeaderSize < (sectionHdrOffset + (sectionCount * SectionHeaderSize))) {
		std::cerr << "Granny file has invalid total header size" << std::endl;
		return false;
	}

	auto totalMemSize = 0u;
	buffer.Seek(sectionHdrOffset);
	for (auto sectionIdx = 0u; sectionIdx < sectionCount; sectionIdx++) {
		GrannySectionHeader hdr;
		if (!hdr.Load(buffer, totalFileSize)) {
			return false;
		}
		sectionHeaders.push_back(hdr);
		totalMemSize += sectionHeaders.back().memSize;
	}

	data.resize(totalMemSize);
	auto memOffset = 0u;
	for (const auto& secHdr : sectionHeaders) {
		if (secHdr.memSize <= 0u) {
			continue;
		}
		buffer.Seek(secHdr.fileOffset);
		switch (secHdr.encoding) {
			case GrannySectionHeader::Encoding::Raw:
				buffer.Read(&data[memOffset], secHdr.fileSize);
				break;
			case GrannySectionHeader::Encoding::Oodle1: {
				DecompressOodle1(secHdr, buffer.Data() + secHdr.fileOffset, &data[memOffset]);
				break;
			}
			case GrannySectionHeader::Encoding::Oodle0:
			default:
				std::cerr << Formatted("Granny section uses unsupported encoding %d", secHdr.encoding) << std::endl;
				return false;
		}
		memOffset += secHdr.memSize;
	}

	return true;
}

void GrannyFile::DecompressOodle1(const GrannySectionHeader& header, const uint8_t *input, uint8_t *output) {
	const uint32_t *headerPtr = reinterpret_cast<const uint32_t*>(input);
	Oodle::Oodle1Bitstream bs(input + 36);
	const std::array<size_t,3> streamEndOffsets = { header.stream0Stop, header.stream1Stop, header.memSize };
	size_t outputOffset = 0u;
	for (auto streamIdx = 0u; streamIdx < 3u; streamIdx++) {
		if (outputOffset >= header.memSize) {
			break;
		}
		Oodle::Oodle1Decompressor decomp(bs);
		decomp.Initialize(headerPtr);
		headerPtr += 3;
		while (outputOffset < streamEndOffsets[streamIdx]) {
			outputOffset += decomp.Decompress(&output[outputOffset]);
		}
	}
}
