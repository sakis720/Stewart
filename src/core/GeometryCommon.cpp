/*
 * Stewart - Map Editor for Family Guy: Back to the Multiverse
 * Copyright (C) 2026 sakis720
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "GeometryCommon.hpp"
#include <iostream>

namespace GeometryCommon {

std::vector<uint8_t> ReadAssetBytes(const HoLib::Archive& archive, const HoLib::AssetEntry& asset) {
    HoLib::EndianReader reader(archive.ArchivePath);
    return asset.ReadData(reader);
}

uint64_t ReadAddressLE(const std::vector<uint8_t>& buf, size_t offset) {
    if (offset + 8 > buf.size()) return 0;
    uint64_t id = 0;
    for (int i = 7; i >= 0; i--) id = (id << 8) | buf[offset + i];
    return id;
}

std::vector<std::string> SplitBy(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == delim) { if (!cur.empty()) { parts.push_back(cur); cur.clear(); } }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

std::string BaseName(const std::string& name) {
    std::string s = name;
    auto p = s.rfind('/');
    if (p != std::string::npos) s = s.substr(p + 1);
    p = s.rfind('\\');
    if (p != std::string::npos) s = s.substr(p + 1);
    return s;
}

static bool UVUsePadding4(const std::vector<uint8_t>& buf) {
    if (buf.size() < 8) return false;
    uint8_t b4=buf[4], b5=buf[5], b6=buf[6], b7=buf[7];
    return (b4==0x33 && b5==0x33 && b6==0x33 && b7==0xFF)
        || (b4==0xFF && b5==0xFF && b6==0xFF && b7==0xFF)
        || (b4==0x00 && b5==0x00 && b6==0x00 && b7==0x40)
        || (b4==0x00 && b5==0x00 && b6==0x00 && b7==0x00);
}

static bool NormalUsePadding4(const std::vector<uint8_t>& buf) {
    if (buf.size() < 7) return false;
    return (buf[4] == 0xFF && buf[5] == 0xFF && buf[6] == 0xFF);
}

std::vector<Vec3> ParseVertices(const std::vector<uint8_t>& buf) {
    std::vector<Vec3> v; v.reserve(buf.size() / 12);
    for (size_t i = 0; i + 12 <= buf.size(); i += 12) {
        Vec3 p; std::memcpy(&p.x, buf.data()+i, 4);
                std::memcpy(&p.y, buf.data()+i+4, 4);
                std::memcpy(&p.z, buf.data()+i+8, 4);
        v.push_back(p);
    }
    return v;
}

std::vector<Face> ParseFaces(const std::vector<uint8_t>& buf) {
    std::vector<Face> f; f.reserve(buf.size() / 6);
    for (size_t i = 0; i + 6 <= buf.size(); i += 6) {
        Face t; std::memcpy(&t.a, buf.data()+i, 2);
                std::memcpy(&t.b, buf.data()+i+2, 2);
                std::memcpy(&t.c, buf.data()+i+4, 2);
        f.push_back(t);
    }
    return f;
}

std::vector<Vec2> ParseUVs(const std::vector<uint8_t>& buf) {
    size_t stride = 4 + (UVUsePadding4(buf) ? 4 : 0);
    std::vector<Vec2> uvs; uvs.reserve(buf.size() / stride);
    for (size_t i = 0; i + stride <= buf.size(); i += stride) {
        uint16_t hu, hv;
        std::memcpy(&hu, buf.data()+i,   2);
        std::memcpy(&hv, buf.data()+i+2, 2);
        uvs.push_back({ HalfToFloat(hu), HalfToFloat(hv) });
    }
    return uvs;
}

std::vector<Vec3> ParseNormals(const std::vector<uint8_t>& buf) {
    size_t stride = 8 + (NormalUsePadding4(buf) ? 4 : 0);
    std::vector<Vec3> ns; ns.reserve(buf.size() / stride);
    for (size_t i = 0; i + stride <= buf.size(); i += stride) {
        int16_t nx, ny, nz;
        std::memcpy(&nx, buf.data()+i,   2);
        std::memcpy(&ny, buf.data()+i+2, 2);
        std::memcpy(&nz, buf.data()+i+4, 2);
        ns.push_back({ nx/32767.f, ny/32767.f, nz/32767.f });
    }
    return ns;
}

const HoLib::AssetEntry* FindRawBlobByID(const std::vector<std::shared_ptr<HoLib::Archive>>& archives, uint64_t targetID, const HoLib::Archive** outArchive) {
    for (const auto& arch : archives)
        for (const auto& asset : arch->Assets)
            if ((asset.AssetType == (uint32_t)HoLib::AssetType::RawBlob || 
                 asset.AssetType == (uint32_t)HoLib::AssetType::GEN_RawBlob)
                && asset.AssetID == targetID)
            {
                if (outArchive) *outArchive = arch.get();
                return &asset;
            }
    return nullptr;
}

const HoLib::AssetEntry* FindAssetByIDAndType(const std::vector<std::shared_ptr<HoLib::Archive>>& archives, uint64_t targetID, uint32_t targetType, const HoLib::Archive** outArchive) {
    for (const auto& arch : archives)
        for (const auto& asset : arch->Assets)
            if (asset.AssetID == targetID && asset.AssetType == targetType) {
                if (outArchive) *outArchive = arch.get();
                return &asset;
            }
    return nullptr;
}

bool ResolveDDSTexture(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    const std::vector<uint8_t>& sgData,
    const std::string& meshName,
    MeshData& out,
    std::ostream& log)
{
    const uint32_t MATERIAL_TYPE = (uint32_t)HoLib::AssetType::Material;
    const uint32_t TEXTURE_TYPE  = (uint32_t)HoLib::AssetType::Texture;

    if (sgData.size() < 0x40) return false;

    uint64_t matID = ReadAddressLE(sgData, 0x38);
    if (!matID) return false;

    const HoLib::Archive*    matArch  = nullptr;
    const HoLib::AssetEntry* matAsset = FindAssetByIDAndType(archives, matID, MATERIAL_TYPE, &matArch);
    if (!matAsset) return false;

    auto matData = ReadAssetBytes(*matArch, *matAsset);
    if (matData.size() < 100) return false;

    size_t texIDOffset = matData.size() - 100;
    uint64_t texID = ReadAddressLE(matData, texIDOffset);
    
    const HoLib::AssetEntry* texAsset = nullptr;
    const HoLib::Archive*    texArch  = nullptr;

    auto tryResolve = [&](uint64_t id) -> bool {
        if (!id) return false;
        texAsset = FindAssetByIDAndType(archives, id, TEXTURE_TYPE, &texArch);
        return texAsset != nullptr;
    };

    if (!tryResolve(texID)) {
        texIDOffset += 8;
        texID = ReadAddressLE(matData, texIDOffset);
        if (!tryResolve(texID)) {
            out.hasValidTexture = false;
            uint32_t hash = 0x811c9dc5;
            for (char c : meshName) { hash ^= (uint8_t)c; hash *= 0x01000193; }
            out.tint[0] = ((hash >>  0) & 0xFF) / 255.0f;
            out.tint[1] = ((hash >>  8) & 0xFF) / 255.0f;
            out.tint[2] = ((hash >> 16) & 0xFF) / 255.0f;
            return true; 
        }
    }

    out.hasValidTexture = true;
    if (matData.size() >= texIDOffset + 16 + 12) {
        float r, g, b;
        std::memcpy(&r, matData.data() + texIDOffset + 16, 4);
        std::memcpy(&g, matData.data() + texIDOffset + 20, 4);
        std::memcpy(&b, matData.data() + texIDOffset + 24, 4);
        out.tint[0] = r; out.tint[1] = g; out.tint[2] = b;
    }

    auto texData = ReadAssetBytes(*texArch, *texAsset);
    if (texData.size() < 8) return false;

    uint64_t blobID = ReadAddressLE(texData, 0);
    if (!blobID) return false;

    const HoLib::Archive*    blobArch  = nullptr;
    const HoLib::AssetEntry* blobAsset = FindRawBlobByID(archives, blobID, &blobArch);
    if (!blobAsset) return false;

    out.ddsTexture    = ReadAssetBytes(*blobArch, *blobAsset);
    out.textureAssetID = texID;
    return true;
}

} // namespace GeometryCommon
