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
#include <map>
#include "EndianReader.hpp"

namespace HoLib {

struct AssetEntry {
    std::string Name;
    uint32_t Flags;
    uint32_t Size;
    uint32_t RelativeDataOffset;
    uint32_t DataSize;
    uint32_t Unknown1;
    uint64_t AssetID;
    uint32_t AssetType;
    int16_t Unknown2;
    int16_t Unknown3;
    
    // Extracted properties backing fields
    int absolutePageOffset;

    std::vector<uint8_t> ReadData(EndianReader& reader) const {
        reader.Seek(absolutePageOffset + RelativeDataOffset);
        return reader.ReadBytes(DataSize);
    }
};

struct AssetSection {
    int Type;
    int StartOffset;
    int Size;
    int Alignment;
};

class Layer {
public:
    uint32_t LayerType;
    uint32_t Flags;
    int Index;
    int TCES_Unknown3;
    int Section_SizeOfData;
    int Unknown5, Unknown6;
    int PageOffset;
    int TotalLayerSize;
    int TotalLayerSize2;
    int PageSize;
    int Unknown11, Unknown12, Unknown13;
    int SubLayerOffset;
    int Unknown15;
    
    void Read(EndianReader& reader) {
        LayerType = reader.ReadUInt32();
        Flags = reader.ReadUInt32();
        Index = reader.ReadInt32();
        TCES_Unknown3 = reader.ReadInt32();
        Section_SizeOfData = reader.ReadInt32();
        Unknown5 = reader.ReadInt32();
        Unknown6 = reader.ReadInt32();
        PageOffset = reader.ReadInt32();
        TotalLayerSize = reader.ReadInt32();
        TotalLayerSize2 = reader.ReadInt32();
        PageSize = reader.ReadInt32();
        Unknown11 = reader.ReadInt32();
        Unknown12 = reader.ReadInt32();
        Unknown13 = reader.ReadInt32();
        SubLayerOffset = reader.ReadInt32();
        Unknown15 = reader.ReadInt32();
    }
};

struct DirectoryEntry {
    uint64_t AssetID;
    int FileNameOffset;
    int Unknown3, Unknown4, Unknown5, Unknown6, Unknown7;
    std::string FileName;
};

class Archive {
public:
    std::string ArchivePath;
    std::vector<AssetEntry> Assets;
    std::string SectionName;

    Archive(const std::string& filename);
};

} // namespace HoLib
