#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "DdsConverter.hpp"
#include <cstring>
#include <algorithm>
#include <vector>
#include <fstream>
#include <ios>

struct DDS_PIXELFORMAT {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

struct DDS_HEADER {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
};

static void DecompressDXT1Block(uint32_t x, uint32_t y, uint32_t width, const uint8_t* blockStorage, uint32_t* image) {
    uint16_t color0 = *(uint16_t*)(blockStorage);
    uint16_t color1 = *(uint16_t*)(blockStorage + 2);

    uint32_t temp;
    temp = (color0 >> 11) * 255 / 31;
    uint8_t r0 = (uint8_t)temp;
    temp = ((color0 >> 5) & 0x3F) * 255 / 63;
    uint8_t g0 = (uint8_t)temp;
    temp = (color0 & 0x1F) * 255 / 31;
    uint8_t b0 = (uint8_t)temp;

    temp = (color1 >> 11) * 255 / 31;
    uint8_t r1 = (uint8_t)temp;
    temp = ((color1 >> 5) & 0x3F) * 255 / 63;
    uint8_t g1 = (uint8_t)temp;
    temp = (color1 & 0x1F) * 255 / 31;
    uint8_t b1 = (uint8_t)temp;

    uint32_t code = *(uint32_t*)(blockStorage + 4);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            uint8_t finalR, finalG, finalB, finalA = 255;
            uint8_t position = (code >> (2 * (j * 4 + i))) & 0x03;

            if (color0 > color1) {
                switch (position) {
                    case 0: finalR = r0; finalG = g0; finalB = b0; break;
                    case 1: finalR = r1; finalG = g1; finalB = b1; break;
                    case 2: finalR = (2 * r0 + r1) / 3; finalG = (2 * g0 + g1) / 3; finalB = (2 * b0 + b1) / 3; break;
                    case 3: finalR = (r0 + 2 * r1) / 3; finalG = (g0 + 2 * g1) / 3; finalB = (b0 + 2 * b1) / 3; break;
                }
            } else {
                switch (position) {
                    case 0: finalR = r0; finalG = g0; finalB = b0; break;
                    case 1: finalR = r1; finalG = g1; finalB = b1; break;
                    case 2: finalR = (r0 + r1) / 2; finalG = (g0 + g1) / 2; finalB = (b0 + b1) / 2; break;
                    case 3: finalR = 0; finalG = 0; finalB = 0; finalA = 0; break;
                }
            }
            if (x + i < width)
                image[(y + j) * width + (x + i)] = (finalA << 24) | (finalB << 16) | (finalG << 8) | finalR;
        }
    }
}

static void DecompressDXT5Block(uint32_t x, uint32_t y, uint32_t width, const uint8_t* blockStorage, uint32_t* image) {
    uint8_t alpha0 = blockStorage[0];
    uint8_t alpha1 = blockStorage[1];
    uint8_t alphas[8];
    alphas[0] = alpha0;
    alphas[1] = alpha1;
    if (alpha0 > alpha1) {
        alphas[2] = (6 * alpha0 + 1 * alpha1) / 7;
        alphas[3] = (5 * alpha0 + 2 * alpha1) / 7;
        alphas[4] = (4 * alpha0 + 3 * alpha1) / 7;
        alphas[5] = (3 * alpha0 + 4 * alpha1) / 7;
        alphas[6] = (2 * alpha0 + 5 * alpha1) / 7;
        alphas[7] = (1 * alpha0 + 6 * alpha1) / 7;
    } else {
        alphas[2] = (4 * alpha0 + 1 * alpha1) / 5;
        alphas[3] = (3 * alpha0 + 2 * alpha1) / 5;
        alphas[4] = (2 * alpha0 + 3 * alpha1) / 5;
        alphas[5] = (1 * alpha0 + 4 * alpha1) / 5;
        alphas[6] = 0;
        alphas[7] = 255;
    }

    uint64_t alphaMask = 0;
    for(int i=0; i<6; ++i) alphaMask |= ((uint64_t)blockStorage[2+i] << (i*8));

    uint16_t color0 = *(uint16_t*)(blockStorage + 8);
    uint16_t color1 = *(uint16_t*)(blockStorage + 10);

    uint32_t temp;
    temp = (color0 >> 11) * 255 / 31; uint8_t r0 = (uint8_t)temp;
    temp = ((color0 >> 5) & 0x3F) * 255 / 63; uint8_t g0 = (uint8_t)temp;
    temp = (color0 & 0x1F) * 255 / 31; uint8_t b0 = (uint8_t)temp;

    temp = (color1 >> 11) * 255 / 31; uint8_t r1 = (uint8_t)temp;
    temp = ((color1 >> 5) & 0x3F) * 255 / 63; uint8_t g1 = (uint8_t)temp;
    temp = (color1 & 0x1F) * 255 / 31; uint8_t b1 = (uint8_t)temp;

    uint32_t code = *(uint32_t*)(blockStorage + 12);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            uint8_t alphaPos = (uint8_t)((alphaMask >> (3 * (j * 4 + i))) & 0x07);
            uint8_t finalA = alphas[alphaPos];
            uint8_t position = (code >> (2 * (j * 4 + i))) & 0x03;
            uint8_t finalR, finalG, finalB;
            switch (position) {
                case 0: finalR = r0; finalG = g0; finalB = b0; break;
                case 1: finalR = r1; finalG = g1; finalB = b1; break;
                case 2: finalR = (2 * r0 + r1) / 3; finalG = (2 * g0 + g1) / 3; finalB = (2 * b0 + b1) / 3; break;
                case 3: finalR = (r0 + 2 * r1) / 3; finalG = (g0 + 2 * g1) / 3; finalB = (b0 + 2 * b1) / 3; break;
            }
            if (x + i < width)
                image[(y + j) * width + (x + i)] = (finalA << 24) | (finalB << 16) | (finalG << 8) | finalR;
        }
    }
}

RawImage DdsToRawRgba(const std::vector<uint8_t>& ddsData) {
    if (ddsData.size() < 128 || memcmp(ddsData.data(), "DDS ", 4) != 0) return {};

    const DDS_HEADER* header = (const DDS_HEADER*)(ddsData.data() + 4);
    uint32_t width = header->dwWidth;
    uint32_t height = header->dwHeight;
    uint32_t fourCC = header->ddspf.dwFourCC;

    RawImage out;
    out.width = width;
    out.height = height;
    out.pixels.resize(width * height * 4);
    uint32_t* rgba = (uint32_t*)out.pixels.data();
    const uint8_t* src = ddsData.data() + 128;

    if (fourCC == 0x31545844) { // DXT1
        for (uint32_t y = 0; y < height; y += 4) {
            for (uint32_t x = 0; x < width; x += 4) {
                DecompressDXT1Block(x, y, width, src, rgba);
                src += 8;
            }
        }
    } else if (fourCC == 0x35545844) { // DXT5
        for (uint32_t y = 0; y < height; y += 4) {
            for (uint32_t x = 0; x < width; x += 4) {
                DecompressDXT5Block(x, y, width, src, rgba);
                src += 16;
            }
        }
    } else if (header->ddspf.dwRGBBitCount == 32) {
        bool isBGRA = (header->ddspf.dwBBitMask == 0x000000FF);
        for (uint32_t i = 0; i < width * height; i++) {
            uint32_t c = *(uint32_t*)(src + i * 4);
            if (isBGRA) {
                uint8_t b = (c & 0xFF), g = ((c >> 8) & 0xFF), r = ((c >> 16) & 0xFF), a = ((c >> 24) & 0xFF);
                rgba[i] = (a << 24) | (b << 16) | (g << 8) | r;
            } else {
                rgba[i] = c;
            }
        }
    } else {
        return {};
    }
    return out;
}

std::vector<uint8_t> DdsToPng(const std::vector<uint8_t>& ddsData) {
    RawImage raw = DdsToRawRgba(ddsData);
    if (raw.pixels.empty()) return {};

    int pngLen = 0;
    unsigned char* pngData = stbi_write_png_to_mem(raw.pixels.data(), raw.width * 4, raw.width, raw.height, 4, &pngLen);
    if (!pngData) return {};

    std::vector<uint8_t> result(pngData, pngData + pngLen);
    free(pngData);
    return result;
}

bool DdsToPngFile(const std::vector<uint8_t>& ddsData, const std::string& pngPath) {
    auto png = DdsToPng(ddsData);
    if (png.empty()) return false;
    std::ofstream out(pngPath, std::ios::binary);
    if (!out) return false;
    out.write((const char*)png.data(), png.size());
    return true;
}
