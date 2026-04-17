/*
 * Stewart - Map Editor for Family Guy: Back to the Multiverse
 * Copyright (C) 2026 sakis720
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <ostream>
#include "HoLib.hpp"

namespace GeometryCommon {

struct Vec3 { float x, y, z; };
struct Vec2 { float u, v; };
struct Face  { uint16_t a, b, c; };

struct MeshData {
    std::vector<Vec3>    vertices;
    std::vector<Face>    faces;
    std::vector<Vec2>    uvs;
    std::vector<Vec3>    normals;
    std::vector<uint8_t> ddsTexture;
    uint64_t             textureAssetID = 0;
    bool                 hasValidTexture = false;
    float                tint[3] = {1.0f, 1.0f, 1.0f};
    std::string          name;
    bool                 isSupported = true;
};

inline float HalfToFloat(uint16_t h) {
    uint32_t sign     = (h >> 15) & 1;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x3FF;
    uint32_t f;
    if (exponent == 0) {
        if (mantissa == 0) { f = sign << 31; }
        else {
            exponent = 1;
            while (!(mantissa & 0x400)) { mantissa <<= 1; exponent--; }
            mantissa &= 0x3FF;
            f = (sign << 31) | ((exponent + 127 - 15) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        f = (sign << 31) | 0x7F800000 | (mantissa << 13);
    } else {
        f = (sign << 31) | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }
    float result;
    std::memcpy(&result, &f, 4);
    return result;
}

inline std::string ToHexID(uint64_t id) {
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)id);
    return std::string(buf);
}

std::vector<uint8_t> ReadAssetBytes(const HoLib::Archive& archive, const HoLib::AssetEntry& asset);
uint64_t ReadAddressLE(const std::vector<uint8_t>& buf, size_t offset);
std::vector<std::string> SplitBy(const std::string& s, char delim);
std::string BaseName(const std::string& name);

std::vector<Vec3> ParseVertices(const std::vector<uint8_t>& buf);
std::vector<Face> ParseFaces(const std::vector<uint8_t>& buf);
std::vector<Vec2> ParseUVs(const std::vector<uint8_t>& buf);
std::vector<Vec3> ParseNormals(const std::vector<uint8_t>& buf);

bool ResolveDDSTexture(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    const std::vector<uint8_t>& sgData,
    const std::string& meshName,
    MeshData& out,
    std::ostream& log);

const HoLib::AssetEntry* FindRawBlobByID(const std::vector<std::shared_ptr<HoLib::Archive>>& archives, uint64_t targetID, const HoLib::Archive** outArchive = nullptr);
const HoLib::AssetEntry* FindAssetByIDAndType(const std::vector<std::shared_ptr<HoLib::Archive>>& archives, uint64_t targetID, uint32_t targetType, const HoLib::Archive** outArchive = nullptr);

} // namespace GeometryCommon
