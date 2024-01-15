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
#ifndef BUFFER_H
#define BUFFER_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct BufferUnderrunException {
};

struct Buffer {
	size_t readCursor = 0;
	std::vector<uint8_t> bytes;
	bool readMode = true;

	Buffer() = default;

	Buffer(const Buffer& buffer, size_t length) {
		bytes.insert(bytes.end(), buffer.bytes.begin() + buffer.readCursor, buffer.bytes.begin() + buffer.readCursor + length);
	}

	explicit Buffer(const std::vector<uint8_t>& bytes) : bytes(bytes) { }

	Buffer(const std::vector<uint8_t>& bytes_, size_t offset, size_t length) {
		bytes.insert(bytes.end(), bytes_.begin() + offset, bytes_.begin() + offset + length);
	}

	size_t Size() const {
		return bytes.size();
	}

	const uint8_t *Data() const {
		return bytes.data();
	}

	const uint8_t *Remaining() const {
		if (readCursor < bytes.size()) {
			return &bytes[readCursor];
		} else {
			return nullptr;
		}
	}

	size_t Tell() const {
		return readCursor;
	}

	void Seek(size_t offset) {
		readCursor = offset;
	}

	bool Empty() const {
		return (readCursor >= bytes.size());
	}

	size_t BytesLeft() const {
		return (readCursor < bytes.size()) ? (bytes.size() - readCursor) : 0;
	}

	void Putback(size_t amount) {
		if (amount >= readCursor) {
			readCursor = 0;
		} else {
			readCursor -= amount;
		}
	}

	void AssertRemainingBytes(unsigned int count) const {
		if ((readCursor + count) > bytes.size()) {
			throw BufferUnderrunException();
		}
	}

	void AssertRemainingBytes(unsigned int count, unsigned int offset) const {
		if ((offset + count) > bytes.size()) {
			throw BufferUnderrunException();
		}
	}

	template <typename T> T Read() {
		return T(*this);
	}

	template <size_t N> void Read(std::array<uint8_t,N>& buffer) {
		AssertRemainingBytes(buffer.size());
		for (auto& v : buffer) {
			v = bytes[readCursor++];
		}
	}

	template <size_t N> void Read(std::array<uint16_t,N>& buffer) {
		for (auto& v : buffer) {
			v = ReadU16();
		}
	}

	template <size_t N> void Read(std::array<uint32_t,N>& buffer) {
		for (auto& v : buffer) {
			v = ReadU32();
		}
	}

	template <size_t N> void Read(std::array<float,N>& buffer) {
		for (auto& v : buffer) {
			v = ReadFloat();
		}
	}

	void Read(std::vector<uint8_t>& buffer) {
		AssertRemainingBytes(buffer.size());
		for (auto idx = 0u; idx < buffer.size(); idx++) {
			buffer[idx] = bytes[readCursor++];
		}
	}

	void Read(uint8_t* buffer, uint32_t size) {
		AssertRemainingBytes(size);
		for (auto idx = 0u; idx < size; idx++) {
			buffer[idx] = bytes[readCursor++];
		}
	}

	uint8_t PeekU8() {
		AssertRemainingBytes(1);
		return bytes[readCursor];
	}

	uint8_t ReadU8() {
		AssertRemainingBytes(1);
		return bytes[readCursor++];
	}

	int8_t ReadS8() {
		return (int8_t)ReadU8();
	}

	bool ReadBool() {
		return (ReadU8() != 0);
	}

	uint16_t ReadU16() {
		AssertRemainingBytes(2);
		const uint16_t data = bytes[readCursor] | ((uint16_t)bytes[readCursor + 1] << 8);
		readCursor += 2;
		return data;
	}

	uint16_t ReadU16BE() {
		AssertRemainingBytes(2);
		const uint16_t data = ((uint16_t)bytes[readCursor] << 8) | bytes[readCursor + 1];
		readCursor += 2;
		return data;
	}

	int16_t ReadS16() {
		return (int16_t)ReadU16();
	}

	uint32_t ReadU24() {
		AssertRemainingBytes(3);
		const uint32_t data = bytes[readCursor] | ((uint32_t)bytes[readCursor + 1] << 8) | ((uint32_t)bytes[readCursor + 2] << 16);
		readCursor += 3;
		return data;
	}

	uint32_t ReadU32() {
		AssertRemainingBytes(4);
		const uint32_t data = bytes[readCursor] | ((uint32_t)bytes[readCursor + 1] << 8) | ((uint32_t)bytes[readCursor + 2] << 16) | ((uint32_t)bytes[readCursor + 3] << 24);
		readCursor += 4;
		return data;
	}

	int32_t ReadS32() {
		AssertRemainingBytes(4);
		const uint32_t data = bytes[readCursor] | ((uint32_t)bytes[readCursor + 1] << 8) | ((uint32_t)bytes[readCursor + 2] << 16) | ((uint32_t)bytes[readCursor + 3] << 24);
		readCursor += 4;
		return (int32_t)data;
	}

	uint32_t ReadU32(size_t offset) const {
		AssertRemainingBytes(4, offset);
		return bytes[offset] | ((uint32_t)bytes[offset + 1] << 8) | ((uint32_t)bytes[offset + 2] << 16) | ((uint32_t)bytes[offset + 3] << 24);
	}

	int64_t ReadS64() {
		AssertRemainingBytes(8);
		const uint64_t data = bytes[readCursor] | ((uint32_t)bytes[readCursor + 1] << 8) | ((uint32_t)bytes[readCursor + 2] << 16) | ((uint32_t)bytes[readCursor + 3] << 24) |
				((uint64_t)bytes[readCursor + 4] << 32) | ((uint64_t)bytes[readCursor + 5] << 40) | ((uint64_t)bytes[readCursor + 6] << 48) | ((uint64_t)bytes[readCursor + 7] << 56);
		readCursor += 8;
		return (int64_t)data;
	}

	uint64_t ReadU64() {
		AssertRemainingBytes(8);
		const uint64_t data = bytes[readCursor] | ((uint32_t)bytes[readCursor + 1] << 8) | ((uint32_t)bytes[readCursor + 2] << 16) | ((uint32_t)bytes[readCursor + 3] << 24) |
				((uint64_t)bytes[readCursor + 4] << 32) | ((uint64_t)bytes[readCursor + 5] << 40) | ((uint64_t)bytes[readCursor + 6] << 48) | ((uint64_t)bytes[readCursor + 7] << 56);
		readCursor += 8;
		return data;
	}

	uint64_t ReadU64(size_t offset) const {
		AssertRemainingBytes(8, offset);
		return bytes[offset] | ((uint32_t)bytes[offset + 1] << 8) | ((uint32_t)bytes[offset + 2] << 16) | ((uint32_t)bytes[offset + 3] << 24) |
				((uint64_t)bytes[offset + 4] << 32) | ((uint64_t)bytes[offset + 5] << 40) | ((uint64_t)bytes[offset + 6] << 48) | ((uint64_t)bytes[offset + 7] << 56);
	}

	float ReadFloat() {
		AssertRemainingBytes(4);
		float data = *(const float*)&bytes[readCursor];
		readCursor += 4;
		return data;
	}

	void ReadPadding(int count) {
		AssertRemainingBytes(count);
		readCursor += count;
	}

	void ReadPadding(size_t offset, int count) const {
		AssertRemainingBytes(count, offset);
	}

	void AppendU8(uint8_t value) {
		readMode = false;
		bytes.push_back(value);
	}

	void AppendBool(bool value) {
		readMode = false;
		bytes.push_back((value) ? 1 : 0);
	}

	void AppendU16(uint16_t value) {
		AppendU8(value);
		AppendU8(value >> 8);
	}

	void AppendU16BE(uint16_t value) {
		AppendU8(value >> 8);
		AppendU8(value);
	}

	void AppendU24(uint32_t value) {
		AppendU8(value);
		AppendU8(value >> 8);
		AppendU8(value >> 16);
	}

	void AppendU32(uint32_t value) {
		AppendU8(value);
		AppendU8(value >> 8);
		AppendU8(value >> 16);
		AppendU8(value >> 24);
	}

	void AppendS32(int32_t value) {
		AppendU8(value);
		AppendU8(value >> 8);
		AppendU8(value >> 16);
		AppendU8(value >> 24);
	}

	void AppendU64(uint64_t value) {
		AppendU8(value);
		AppendU8(value >> 8);
		AppendU8(value >> 16);
		AppendU8(value >> 24);
		AppendU8(value >> 32);
		AppendU8(value >> 40);
		AppendU8(value >> 48);
		AppendU8(value >> 56);
	}

	void AppendU32BE(uint32_t value) {
		AppendU8(value >> 24);
		AppendU8(value >> 16);
		AppendU8(value >> 8);
		AppendU8(value);
	}

	void AppendFloat(float value) {
		const auto offset = bytes.size();
		bytes.push_back(0);
		bytes.push_back(0);
		bytes.push_back(0);
		bytes.push_back(0);
		*((float*)&bytes[offset]) = value;
	}

	template <size_t N> void Append(const std::array<uint8_t,N>& value) {
		readMode = false;
		bytes.insert(bytes.end(), value.begin(), value.end());
	}

	void Append(const Buffer& value) {
		readMode = false;
		bytes.insert(bytes.end(), value.bytes.begin(), value.bytes.end());
	}

	void Append(const std::vector<uint8_t>& value, bool explicitLen) {
		readMode = false;
		if (explicitLen) {
			AppendU32(value.size());
		}
		bytes.insert(bytes.end(), value.begin(), value.end());
	}

	void Append(const std::string& value, bool explicitLen, bool includeNull) {
		readMode = false;
		if (explicitLen) {
			bytes.push_back(value.size());
		}
		bytes.insert(bytes.end(), value.begin(), value.end());
		if (includeNull) {
			bytes.push_back('\0');
		}
	}

	void AppendCStr(const std::string& value) {
		Append(value, false, true);
	}

	void AppendLStr32(const std::string& value) {
		AppendU32(value.size());
		bytes.insert(bytes.end(), value.begin(), value.end());
	}

	void AppendPadding(size_t count) {
		while (count--) {
			AppendU8(0);
		}
	}

	uint8_t operator [] (size_t offset) const {
		if ((readCursor + offset + 1) > bytes.size()) {
			throw BufferUnderrunException();
		}
		return bytes[readCursor + offset];
	}
};

#endif
