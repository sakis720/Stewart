/*
 * Stewart - Map Editor for Family Guy: Back to the Multiverse
 * Copyright (C) 2026 sakis720
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "SkeletalExporter.hpp"

namespace SkeletalExporter {

using namespace GeometryCommon;

bool ParseSkinGeometry(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    const HoLib::Archive&    arch,
    const HoLib::AssetEntry& skinAsset,
    MeshData& out,
    std::ostream& log)
{
    auto skinData = ReadAssetBytes(arch, skinAsset);
    if (skinData.size() < 0x140) return false;

    const uint8_t signature[] = { 0xFF, 0x02, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E };
    auto it = std::search(skinData.begin(), skinData.end(), std::begin(signature), std::end(signature));
    if (it != skinData.end()) {
        log << "  " << out.name << ": Multi-part model not yet supported\n";
        out.isSupported = false;
        return true;
    }

    auto fetchBlob = [&](uint64_t id) -> std::vector<uint8_t> {
        if (!id) return {};
        const HoLib::Archive*    a = nullptr;
        const HoLib::AssetEntry* e = FindRawBlobByID(archives, id, &a);
        return (e && a) ? ReadAssetBytes(*a, *e) : std::vector<uint8_t>{};
    };

    uint64_t facesID   = ReadAddressLE(skinData, 0x70);
    uint64_t vertsID   = ReadAddressLE(skinData, 0x110);
    uint64_t uvsID     = ReadAddressLE(skinData, 0x130);

    auto vertsBlob   = fetchBlob(vertsID);
    auto facesBlob   = fetchBlob(facesID);
    auto uvsBlob     = fetchBlob(uvsID);

    if (vertsBlob.empty() || facesBlob.empty()) return false;
    
    size_t stride = 20;
    out.vertices.reserve(vertsBlob.size() / stride);
    for (size_t i = 0; i + stride <= vertsBlob.size(); i += stride) {
        Vec3 p; std::memcpy(&p.x, vertsBlob.data()+i, 4);
                std::memcpy(&p.y, vertsBlob.data()+i+4, 4);
                std::memcpy(&p.z, vertsBlob.data()+i+8, 4);
        out.vertices.push_back(p);
    }

    out.faces = ParseFaces(facesBlob);

    if (!uvsBlob.empty()) {
        size_t uvStride = 4;
        if (uvsBlob.size() >= 8) {
            uint8_t b4=uvsBlob[4], b5=uvsBlob[5], b6=uvsBlob[6], b7=uvsBlob[7];
            if ((b4==0x33 && b5==0x33 && b6==0x33 && b7==0xFF) ||
                (b4==0xFF && b5==0xFF && b6==0xFF && b7==0xFF) ||
                (b4==0x00 && b5==0x00 && b6==0x00 && b7==0x40) ||
                (b4==0x00 && b5==0x00 && b6==0x00 && b7==0x00)) {
                uvStride = 8;
            }
        }
        out.uvs.reserve(uvsBlob.size() / uvStride);
        for (size_t i = 0; i + 4 <= uvsBlob.size(); i += uvStride) {
            uint16_t hu, hv;
            std::memcpy(&hu, uvsBlob.data()+i, 2);
            std::memcpy(&hv, uvsBlob.data()+i+2, 2);
            out.uvs.push_back({ HalfToFloat(hu), HalfToFloat(hv) });
        }
    }
    
    ResolveDDSTexture(archives, skinData, out.name, out, log);
    return !out.vertices.empty() && !out.faces.empty();
}

} // namespace SkeletalExporter
