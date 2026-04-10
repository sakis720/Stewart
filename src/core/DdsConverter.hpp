#pragma once
#include <vector>
#include <cstdint>
#include <string>

struct RawImage {
    std::vector<uint8_t> pixels;
    uint32_t width = 0, height = 0;
};

// Convert a DDS to raw RGBA8 pixels.
RawImage DdsToRawRgba(const std::vector<uint8_t>& ddsData);

// Convert a DDS file (in memory) to PNG (returned as in-memory bytes).
// Handles BC1/DXT1, BC3/DXT5, BC5/ATI2, RGBA8, BGRA8.
// Returns an empty vector if conversion fails.
std::vector<uint8_t> DdsToPng(const std::vector<uint8_t>& ddsData);

// Convenience: save PNG to disk, returns true on success.
bool DdsToPngFile(const std::vector<uint8_t>& ddsData, const std::string& pngPath);
