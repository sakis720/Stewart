/*
 * Stewart - Map Editor for Family Guy: Back to the Multiverse
 * Copyright (C) 2026 sakis720
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "ObjectExporter.hpp"

namespace ObjectExporter {

using namespace GeometryCommon;

bool ParseStaticGeometry(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    const HoLib::Archive&    arch,
    const HoLib::AssetEntry& sgAsset,
    MeshData& out,
    std::ostream& log)
{
    auto sgData = ReadAssetBytes(arch, sgAsset);
    if (sgData.size() < 0x110) return false;

    auto fetchBlob = [&](uint64_t id) -> std::vector<uint8_t> {
        if (!id) return {};
        const HoLib::Archive*    a = nullptr;
        const HoLib::AssetEntry* e = FindRawBlobByID(archives, id, &a);
        return (e && a) ? ReadAssetBytes(*a, *e) : std::vector<uint8_t>{};
    };

    uint64_t facesID   = ReadAddressLE(sgData, 0x70);
    uint64_t vertsID   = ReadAddressLE(sgData, 0xE0);
    uint64_t uvsID     = ReadAddressLE(sgData, 0x100);
    uint64_t normalsID = ReadAddressLE(sgData, 0xF0);

    auto vertsBlob   = fetchBlob(vertsID);
    auto facesBlob   = fetchBlob(facesID);
    auto uvsBlob     = fetchBlob(uvsID);
    auto normalsBlob = fetchBlob(normalsID);

    if (vertsBlob.empty() || facesBlob.empty()) return false;
    
    out.vertices = ParseVertices(vertsBlob);
    out.faces    = ParseFaces(facesBlob);
    if (!uvsBlob.empty())     out.uvs     = ParseUVs(uvsBlob);
    if (!normalsBlob.empty()) out.normals  = ParseNormals(normalsBlob);
    
    ResolveDDSTexture(archives, sgData, out.name, out, log);

    return !out.vertices.empty() && !out.faces.empty();
}

std::string GroupKeyFromName(const std::string& assetName)
{
    std::string base = BaseName(assetName);
    size_t pos = base.find("_BSP_");
    if (pos != std::string::npos) {
        std::string sub = base.substr(pos + 5);
        auto dot = sub.find('.');
        if (dot != std::string::npos) sub = sub.substr(0, dot);
        return sub;
    }
    auto parts = SplitBy(base, '_');
    if (parts.size() >= 3) {
        std::string sub = parts[2];
        auto dot = sub.find('.');
        if (dot != std::string::npos) sub = sub.substr(0, dot);
        return sub;
    }
    if (parts.size() >= 2) return parts[1];
    return "default";
}

std::string ObjectNameFromName(const std::string& assetName)
{
    std::string base = BaseName(assetName);
    auto parts = SplitBy(base, '_');
    if (parts.size() <= 2) return base;
    std::string result;
    for (size_t i = 2; i < parts.size(); i++) {
        if (i > 2) result += '_';
        result += parts[i];
    }
    return result;
}

} // namespace ObjectExporter
