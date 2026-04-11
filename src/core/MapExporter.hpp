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
#include "HoLib.hpp"

namespace MapExporter {

// ─── Data structures ────────────────────────────────────────────────────────

struct Vec3 { float x, y, z; };
struct Vec2 { float u, v; };
struct Face  { uint16_t a, b, c; };

struct MeshData {
    std::vector<Vec3>    vertices;
    std::vector<Face>    faces;
    std::vector<Vec2>    uvs;
    std::vector<Vec3>    normals;     // decoded from signed short
    std::vector<uint8_t> ddsTexture; // raw DDS bytes (may be empty)
    uint64_t             textureAssetID = 0;
    bool                 hasValidTexture = false;
    float                tint[3] = {1.0f, 1.0f, 1.0f}; // RGB color multiplier
    std::string          name;
};

// ─── Half-float helper ───────────────────────────────────────────────────────
inline float HalfToFloat(uint16_t h) {
    uint32_t sign     = (h >> 15) & 1;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x3FF;
    uint32_t f;
    if (exponent == 0) {
        if (mantissa == 0) { f = sign << 31; }
        else {
            // Denormalized
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

// ─── Byte reversal (8 bytes as described in the spec) ───────────────────────
inline uint64_t ReverseBytes8(const uint8_t* p) {
    return ((uint64_t)p[0]       ) |
           ((uint64_t)p[1] << 8  ) |
           ((uint64_t)p[2] << 16 ) |
           ((uint64_t)p[3] << 24 ) |
           ((uint64_t)p[4] << 32 ) |
           ((uint64_t)p[5] << 40 ) |
           ((uint64_t)p[6] << 48 ) |
           ((uint64_t)p[7] << 56 );
}

// Convert uint64 to uppercase hex string (16 chars, no prefix)
inline std::string ToHexID(uint64_t id) {
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)id);
    return std::string(buf);
}

// ─── Forward decl ────────────────────────────────────────────────────────────

// Main entry point:
//   archives       – all loaded .ho archives
//   outputDir      – where to write the OBJ (or GLB) file
//   exportGLB      – if true, write .glb; otherwise write .obj + .mtl
bool ParseStaticGeometry(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    const HoLib::Archive& arch,
    const HoLib::AssetEntry& sgAsset,
    MeshData& out,
    std::ostream& log);


std::string GroupKeyFromName(const std::string& assetName);
std::string ObjectNameFromName(const std::string& assetName);

bool WriteGroupGLB(
    const std::string& outputPath,
    const std::map<std::string, std::vector<MeshData>>& groups,
    std::string& statusOut);

} // namespace MapExporter
