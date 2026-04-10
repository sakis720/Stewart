#pragma once
#include <fstream>
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#include <stdlib.h>
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)
#else
#include <byteswap.h>
#endif

namespace HoLib {
class EndianReader {
    std::ifstream stream;
public:
    bool isBigEndian = false;

    EndianReader(const std::string& path) {
        stream.open(path, std::ios::binary);
        if (!stream.is_open()) throw std::runtime_error("Failed to open file: " + path);
    }
    
    std::ifstream& GetStream() { return stream; }

    void Seek(uint64_t offset) {
        stream.seekg(offset, std::ios::beg);
    }

    uint64_t GetPosition() {
        return stream.tellg();
    }

    template<typename T>
    T Read() {
        T val;
        stream.read(reinterpret_cast<char*>(&val), sizeof(T));
        if (isBigEndian) {
            if constexpr (sizeof(T) == 2) val = bswap_16(val);
            else if constexpr (sizeof(T) == 4) val = bswap_32(val);
            else if constexpr (sizeof(T) == 8) val = bswap_64(val);
        }
        return val;
    }

    uint8_t ReadUInt8() { return Read<uint8_t>(); }
    uint16_t ReadUInt16() { return Read<uint16_t>(); }
    uint32_t ReadUInt32() { return Read<uint32_t>(); }
    uint64_t ReadUInt64() { return Read<uint64_t>(); }
    int16_t ReadInt16() { return Read<int16_t>(); }
    int32_t ReadInt32() { return Read<int32_t>(); }
    int64_t ReadInt64() { return Read<int64_t>(); }

    std::string ReadString(size_t len) {
        std::string s(len, '\0');
        stream.read(&s[0], len);
        return s;
    }

    std::string ReadCString() {
        std::string s;
        char c;
        while (stream.read(&c, 1) && c != '\0') {
            s += c;
        }
        return s;
    }
    
    std::vector<uint8_t> ReadBytes(size_t len) {
        std::vector<uint8_t> buf(len);
        stream.read(reinterpret_cast<char*>(buf.data()), len);
        return buf;
    }
    
    uint32_t AssertTag(uint32_t tag1, uint32_t tag2 = 0) {
        uint32_t val = ReadUInt32();
        if (val != tag1 && val != tag2) {
            // we have to check swapped endian too just in case we haven't determined endianness
            uint32_t swapped = bswap_32(val);
            if (swapped == tag1 || swapped == tag2) {
                return swapped;
            }
        }
        return val;
    }
};
}
