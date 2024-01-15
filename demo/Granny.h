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
#ifndef GRANNY_H
#define GRANNY_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

struct Buffer;

struct GrannySectionHeader {
	enum class Encoding {
		Raw = 0u,
		Oodle0 = 1u,
		Oodle1 = 2u,
	} encoding;

	uint32_t fileOffset = 0u;
	uint32_t fileSize = 0u;
	uint32_t memSize = 0u;
	uint32_t alignment = 0u;
	uint32_t stream0Stop = 0u;	// Switch from stream0 -> stream1 after decompressing this many bytes
	uint32_t stream1Stop = 0u;	// Switch from stream1 -> stream2 after decompressing this many bytes
	uint32_t relocOffset = 0u;
	uint32_t relocCount = 0u;
	uint32_t marshalOffset = 0u;
	uint32_t marshalCount = 0u;

	GrannySectionHeader() = default;

	bool Load(Buffer& buffer, uint32_t totalFileSize);
};

struct GrannyFile {
public:
	static constexpr auto SectionHeaderSize = 44u;
	static constexpr auto SignatureLength = 16u;
	static constexpr std::array<uint8_t,SignatureLength> SignatureLE = {
		0xb8, 0x67, 0xb0, 0xca, 0xf8, 0x6d, 0xb1, 0x0f,
		0x84, 0x72, 0x8c, 0x7e, 0x5e, 0x19, 0x00, 0x1e,
	};
	static constexpr auto UserDataSize = 16u;

	GrannyFile() = default;

	GrannyFile(const std::vector<uint8_t>& raw) {
		if (!LoadFromBytes(raw)) {
			throw std::runtime_error("Failed to parse Granny data");
		}
	}

	const std::vector<uint8_t>& GetData() const { return data; }
	bool LoadFromBytes(const std::vector<uint8_t>& raw);

private:
	uint32_t crc = 0u;
	std::vector<uint8_t> data;
	uint32_t dataBase = 0u;
	uint64_t rootNodeType = 0u;
	uint64_t rootNodeObject = 0u;
	std::vector<GrannySectionHeader> sectionHeaders;
	uint32_t totalFileSize = 0u;
	uint32_t totalHeaderSize = 0u;
	std::array<uint8_t,UserDataSize> userData = { 0 };
	uint32_t userTag = 0u;
	uint32_t version = 0u;

	void DecompressOodle1(const GrannySectionHeader& header, const uint8_t *input, uint8_t *output);
};

#endif
