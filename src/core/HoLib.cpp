#include "HoLib.hpp"
#include <iostream>

using namespace HoLib;

Archive::Archive(const std::string& filename) : ArchivePath(filename) {
    EndianReader reader(filename);
    
    uint32_t tag = reader.AssertTag(0x1A4C4548, 0x1A424548); // HEL, HEB
    
    // Read one byte to determine endianness
    uint8_t firstByte = reader.ReadUInt8();
    reader.isBigEndian = (firstByte == 0);
    reader.Seek(4); // Seek past tag
    
    reader.Seek(0x800); // Wait, Data is 0x800 - 4 bytes long. The stream is now at 0x800
    
    // MAST block
    uint32_t mastTag = reader.AssertTag(0x5453414D); // MAST
    int mastPageSize = reader.ReadInt32();
    reader.ReadInt32();
    int mastStringTableOffset = reader.ReadInt32();
    int mastStringTableSize = reader.ReadInt32();
    reader.ReadInt32(); reader.ReadInt32(); reader.ReadInt32();
    
    // TCES block
    uint32_t tcesTag = reader.AssertTag(0x53454354, 0x54434553); // TCES or SECT
    uint32_t tcesFlags = reader.ReadUInt32();
    reader.ReadInt32(); reader.ReadInt32();
    int tcesSize = reader.ReadInt32();
    reader.ReadInt32(); reader.ReadInt32();
    int tcesPageOffset = reader.ReadInt32(); // << 11
    reader.ReadInt32(); reader.ReadInt32(); reader.ReadInt32(); reader.ReadInt32(); 
    reader.ReadInt32(); reader.ReadInt32(); reader.ReadInt32(); reader.ReadInt32();
    
    // Jump to section
    uint64_t sectionPos = tcesPageOffset << 11;
    reader.Seek(sectionPos);
    
    // Section block
    uint32_t sectTag = reader.AssertTag(0x54434553); // SECT
    int sectLayerCount = reader.ReadInt32();
    reader.ReadInt32();
    int sectNameOffset = reader.ReadInt32();
    int sectNameSize = reader.ReadInt32();
    int sectSubLayersOffset = reader.ReadInt32();
    int sectSubLayersSize = reader.ReadInt32();
    reader.ReadInt32();
    
    std::vector<Layer> layers(sectLayerCount);
    for (int i = 0; i < sectLayerCount; ++i) {
        layers[i].Read(reader);
    }
    
    SectionName = reader.ReadCString();
    
    std::vector<AssetEntry> assetEntries;
    std::map<uint32_t, std::map<uint64_t, DirectoryEntry>> directoryPerFlags;
    
    for (int i = 0; i < sectLayerCount; ++i) {
        Layer& layer = layers[i];
        reader.Seek(sectionPos + layer.SubLayerOffset);
        
        std::string magic = reader.ReadString(4);
        
        if (magic == std::string("PSL\0", 4)) {
            int size = reader.ReadInt32();
            int sectionCount = reader.ReadInt32();
            reader.ReadInt32();
            
            std::vector<AssetSection> aSections(sectionCount);
            for (int s = 0; s < sectionCount; ++s) {
                aSections[s].Type = reader.ReadInt32();
                aSections[s].StartOffset = reader.ReadInt32();
                aSections[s].Size = reader.ReadInt32();
                aSections[s].Alignment = reader.ReadInt32();
            }
            
            int layerOffset = layer.PageOffset << 11;
            
            for (int s = 0; s < sectionCount; ++s) {
                if (aSections[s].Type == 0) { // AssetGroup
                    reader.Seek(layerOffset + aSections[s].StartOffset);
                    int count = reader.ReadInt32();
                    reader.ReadInt32(); // unknown1
                    reader.Seek(reader.GetPosition() + 0x18); // padding 0x74 filled
                    
                    for (int c = 0; c < count; ++c) {
                        AssetEntry entry;
                        entry.Size = reader.ReadInt32();
                        entry.RelativeDataOffset = reader.ReadInt32();
                        entry.DataSize = reader.ReadInt32();
                        entry.Unknown1 = reader.ReadInt32();
                        entry.AssetID = reader.ReadUInt64();
                        entry.AssetType = reader.ReadUInt32();
                        entry.Unknown2 = reader.ReadInt16();
                        entry.Unknown3 = reader.ReadInt16();
                        
                        entry.Flags = layer.Flags;
                        entry.absolutePageOffset = layerOffset;
                        assetEntries.push_back(entry);
                    }
                }
            }
        } 
        else if (magic == "PSLD") {
            int size = reader.ReadInt32();
            int amountOfEntries = reader.ReadInt32();
            int dataSize = reader.ReadInt32();
            reader.ReadInt32(); reader.ReadInt32(); reader.ReadInt32(); reader.ReadInt32();
            
            int layerOffset = layer.PageOffset << 11;
            reader.Seek(layerOffset);
            
            std::vector<int> entrySizes(amountOfEntries);
            for (int a = 0; a < amountOfEntries; ++a) {
                entrySizes[a] = reader.ReadInt32();
            }
            
            reader.Seek(layerOffset + dataSize);
            
            for (int a = 0; a < amountOfEntries; ++a) {
                uint64_t startPos = reader.GetPosition();
                DirectoryEntry entry;
                entry.AssetID = reader.ReadUInt64();
                entry.FileNameOffset = reader.ReadInt32();
                entry.Unknown3 = reader.ReadInt32();
                entry.Unknown4 = reader.ReadInt32();
                entry.Unknown5 = reader.ReadInt32();
                entry.Unknown6 = reader.ReadInt32();
                entry.Unknown7 = reader.ReadInt32();
                entry.FileName = reader.ReadCString();
                
                directoryPerFlags[layer.Flags][entry.AssetID] = entry;
                
                reader.Seek(startPos + entrySizes[a]);
            }
        }
    }
    
    for (auto& entry : assetEntries) {
        if (directoryPerFlags.find(entry.Flags) != directoryPerFlags.end()) {
            auto& dir = directoryPerFlags[entry.Flags];
            if (dir.find(entry.AssetID) != dir.end()) {
                entry.Name = dir[entry.AssetID].FileName;
            }
        }
    }
    
    Assets = assetEntries;
}
