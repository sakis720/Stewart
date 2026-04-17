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
#include "GeometryCommon.hpp"

namespace SkeletalExporter {

bool ParseSkinGeometry(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    const HoLib::Archive& arch,
    const HoLib::AssetEntry& skinAsset,
    GeometryCommon::MeshData& out,
    std::ostream& log);

} // namespace SkeletalExporter
