#include "MapExporter.hpp"
#include "DdsConverter.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <map>
#include <filesystem>

namespace MapExporter {

namespace fs = std::filesystem;

// ============================================================================
//  Low-level helpers
// ============================================================================

static std::vector<uint8_t> ReadAssetBytes(
    const HoLib::Archive&    archive,
    const HoLib::AssetEntry& asset)
{
    HoLib::EndianReader reader(archive.ArchivePath);
    return asset.ReadData(reader);
}

// Find RawBlob / GEN_RawBlob by AssetID
static const HoLib::AssetEntry* FindRawBlobByID(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    uint64_t targetID,
    const HoLib::Archive** outArchive = nullptr)
{
    static const uint32_t RAWBLOB_TYPE     = 0xBDE21A8D;
    static const uint32_t GEN_RAWBLOB_TYPE = 0x4DB6973E;
    for (const auto& arch : archives)
        for (const auto& asset : arch->Assets)
            if ((asset.AssetType == RAWBLOB_TYPE || asset.AssetType == GEN_RAWBLOB_TYPE)
                && asset.AssetID == targetID)
            {
                if (outArchive) *outArchive = arch.get();
                return &asset;
            }
    return nullptr;
}

// Find an asset by ID + exact type
static const HoLib::AssetEntry* FindAssetByIDAndType(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    uint64_t targetID,
    uint32_t targetType,
    const HoLib::Archive** outArchive = nullptr)
{
    for (const auto& arch : archives)
        for (const auto& asset : arch->Assets)
            if (asset.AssetID == targetID && asset.AssetType == targetType) {
                if (outArchive) *outArchive = arch.get();
                return &asset;
            }
    return nullptr;
}

// Read 8 bytes at offset as little-endian uint64 (i.e. "reversed" per spec).
// Spec example: bytes C7 10 CA D7 91 A3 D5 9B  →  reversed value 9BD5A391D7CA10C7
// i.e. byte[0] is the LSB of the result.
static uint64_t ReadAddressLE(const std::vector<uint8_t>& buf, size_t offset)
{
    if (offset + 8 > buf.size()) return 0;
    uint64_t id = 0;
    for (int i = 7; i >= 0; i--)
        id = (id << 8) | buf[offset + i];
    return id;
}

// ============================================================================
//  Name helpers
// ============================================================================

// Split a string by delimiter, returning all non-empty parts
static std::vector<std::string> SplitBy(const std::string& s, char delim)
{
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == delim) { if (!cur.empty()) { parts.push_back(cur); cur.clear(); } }
        else               cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

// Given a full asset name (may include path separator '/'),
// return just the final filename component.
static std::string BaseName(const std::string& name)
{
    std::string s = name;
    auto p = s.rfind('/');
    if (p != std::string::npos) s = s.substr(p + 1);
    p = s.rfind('\\');
    if (p != std::string::npos) s = s.substr(p + 1);
    return s;
}

// Given BSP_AMXX_AlwaysOn.8A3EB7... return "AMXX" (the group key).
// Splits by '_', skips first segment ("BSP"), returns second.
// Falls back to "default" if the name doesn't have enough segments.
// Given BSP_CHXX_BSP_CityHall.E27156324363E94D return "CityHall".
// It looks for segment after "_BSP_" or falls back to segments after "BSP_CODE_".
std::string GroupKeyFromName(const std::string& assetName)
{
    std::string base = BaseName(assetName);
    
    // Look for "_BSP_"
    size_t pos = base.find("_BSP_");
    if (pos != std::string::npos) {
        std::string sub = base.substr(pos + 5); // Skip "_BSP_"
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

// Short object name = everything after "BSP_GROUP_"  (segments 2+, joined by '_')
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

// ============================================================================
//  Geometry parsing
// ============================================================================

static bool UVUsePadding4(const std::vector<uint8_t>& buf) {
    if (buf.size() < 8) return false;
    uint8_t b4=buf[4], b5=buf[5], b6=buf[6], b7=buf[7];
    return (b4==0x33 && b5==0x33 && b6==0x33 && b7==0xFF)
        || (b4==0xFF && b5==0xFF && b6==0xFF && b7==0xFF)
        || (b4==0x00 && b5==0x00 && b6==0x00 && b7==0x40);
}

static bool NormalUsePadding4(const std::vector<uint8_t>& buf) {
    if (buf.size() < 7) return false;
    return (buf[4]==0xFF && buf[5]==0xFF && buf[6]==0xFF);
}

static std::vector<Vec3> ParseVertices(const std::vector<uint8_t>& buf) {
    std::vector<Vec3> v; v.reserve(buf.size() / 12);
    for (size_t i = 0; i + 12 <= buf.size(); i += 12) {
        Vec3 p; std::memcpy(&p.x, buf.data()+i, 4);
                std::memcpy(&p.y, buf.data()+i+4, 4);
                std::memcpy(&p.z, buf.data()+i+8, 4);
        v.push_back(p);
    }
    return v;
}

static std::vector<Face> ParseFaces(const std::vector<uint8_t>& buf) {
    std::vector<Face> f; f.reserve(buf.size() / 6);
    for (size_t i = 0; i + 6 <= buf.size(); i += 6) {
        Face t; std::memcpy(&t.a, buf.data()+i, 2);
                std::memcpy(&t.b, buf.data()+i+2, 2);
                std::memcpy(&t.c, buf.data()+i+4, 2);
        f.push_back(t);
    }
    return f;
}

static std::vector<Vec2> ParseUVs(const std::vector<uint8_t>& buf) {
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

static std::vector<Vec3> ParseNormals(const std::vector<uint8_t>& buf) {
    size_t stride = 8 + (NormalUsePadding4(buf) ? 4 : 0); // 6 shorts + 2 pad + opt 4
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

// ============================================================================
//  Texture chain
// ============================================================================
// SG[0x38] -> Material(0x189F55BF) -> [size-100] -> Texture(0x2952081F) -> [0] -> RawBlob -> DDS

static bool ResolveDDSTexture(
    const std::vector<std::shared_ptr<HoLib::Archive>>& archives,
    const std::vector<uint8_t>& sgData,
    const std::string& meshName,
    MeshData& out,
    std::ostream& log)
{
    static const uint32_t MATERIAL_TYPE = 0x189F55BF;
    static const uint32_t TEXTURE_TYPE  = 0x2952081F;

    if (sgData.size() < 0x40) { log << "  " << meshName << ": SG too small\n"; return false; }

    uint64_t matID = ReadAddressLE(sgData, 0x38);
    if (!matID) { log << "  " << meshName << ": matID=0 at SG[0x38]\n"; return false; }
    log << "  " << meshName << ": matID=" << std::hex << matID << std::dec << "\n";

    const HoLib::Archive*    matArch  = nullptr;
    const HoLib::AssetEntry* matAsset = FindAssetByIDAndType(archives, matID, MATERIAL_TYPE, &matArch);
    if (!matAsset) { log << "  " << meshName << ": Material not found\n"; return false; }
    log << "  " << meshName << ": mat=" << matAsset->Name << "\n";

    auto matData = ReadAssetBytes(*matArch, *matAsset);
    if (matData.size() < 100) { log << "  " << meshName << ": mat data too small\n"; return false; }

    uint64_t texID = ReadAddressLE(matData, matData.size() - 100);
    if (!texID) { log << "  " << meshName << ": texID=0 at mat[size-100]\n"; return false; }
    log << "  " << meshName << ": texID=" << std::hex << texID << std::dec << "\n";

    const HoLib::Archive*    texArch  = nullptr;
    const HoLib::AssetEntry* texAsset = FindAssetByIDAndType(archives, texID, TEXTURE_TYPE, &texArch);
    if (!texAsset) { log << "  " << meshName << ": Texture asset not found\n"; return false; }
    log << "  " << meshName << ": tex=" << texAsset->Name << "\n";

    auto texData = ReadAssetBytes(*texArch, *texAsset);
    if (texData.size() < 8) { log << "  " << meshName << ": tex data too small\n"; return false; }

    uint64_t blobID = ReadAddressLE(texData, 0);
    if (!blobID) { log << "  " << meshName << ": blobID=0 at tex[0]\n"; return false; }
    log << "  " << meshName << ": blobID=" << std::hex << blobID << std::dec << "\n";

    const HoLib::Archive*    blobArch  = nullptr;
    const HoLib::AssetEntry* blobAsset = FindRawBlobByID(archives, blobID, &blobArch);
    if (!blobAsset) { log << "  " << meshName << ": RawBlob not found\n"; return false; }

    out.ddsTexture    = ReadAssetBytes(*blobArch, *blobAsset);
    out.textureAssetID = texID;
    log << "  " << meshName << ": OK dds=" << out.ddsTexture.size() << " bytes\n";
    return true;
}

// ============================================================================
//  Parse one StaticGeometry asset → MeshData
// ============================================================================

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

    out.name     = sgAsset.Name;
    out.vertices = ParseVertices(vertsBlob);
    out.faces    = ParseFaces(facesBlob);
    if (!uvsBlob.empty())     out.uvs     = ParseUVs(uvsBlob);
    if (!normalsBlob.empty()) out.normals  = ParseNormals(normalsBlob);
    
    ResolveDDSTexture(archives, sgData, BaseName(sgAsset.Name), out, log);

    return !out.vertices.empty() && !out.faces.empty();
}

// ============================================================================
//  GLB export  (one file per group)
// ============================================================================

static void ExportMeshesToGLB(
    const std::vector<MeshData>& meshes,
    const std::string& outputDir,
    const std::string& groupName)
{
    std::string safe = groupName;
    for (char& c : safe) if (c=='/'||c=='\\'||c==':'||c==' ') c='_';

    std::vector<uint8_t> bin;
    struct BufView  { uint32_t offset, len; int target; };
    struct Accessor { uint32_t bv, count; int ct; std::string type; float mn[3], mx[3]; bool bounds=false; };
    struct MGltf    { int pos=-1,idx=-1,uv=-1,nrm=-1; };

    std::vector<BufView>  bviews;
    std::vector<Accessor> accs;
    std::vector<MGltf>    mgltf;

    auto align4 = [&]() { while (bin.size()%4) bin.push_back(0); };
    auto addBV  = [&](const void* data, size_t len, int tgt) -> uint32_t {
        align4();
        uint32_t off = (uint32_t)bin.size();
        const uint8_t* p = static_cast<const uint8_t*>(data);
        bin.insert(bin.end(), p, p+len);
        bviews.push_back({off,(uint32_t)len,tgt});
        return (uint32_t)bviews.size()-1;
    };
    auto addAcc = [&](uint32_t bv, uint32_t cnt, int ct, const std::string& tp,
                      const float* mn=nullptr, const float* mx=nullptr) -> int {
        Accessor a; a.bv=bv; a.count=cnt; a.ct=ct; a.type=tp;
        if (mn&&mx) { std::memcpy(a.mn,mn,12); std::memcpy(a.mx,mx,12); a.bounds=true; }
        accs.push_back(a); return (int)accs.size()-1;
    };

    // Material and Texture tracking for GLTF
    struct GltfMat { std::string name; std::string pngUri; };
    std::vector<GltfMat> gltfMats;
    std::vector<int> meshToMat(meshes.size(), -1);

    for (size_t mi = 0; mi < meshes.size(); mi++) {
        const MeshData& m = meshes[mi];
        MGltf g;
        if (!m.vertices.empty()) {
            float mn[3]={m.vertices[0].x,m.vertices[0].y,m.vertices[0].z};
            float mx[3]={mn[0],mn[1],mn[2]};
            for (auto& v:m.vertices){mn[0]=std::min(mn[0],v.x);mn[1]=std::min(mn[1],v.y);mn[2]=std::min(mn[2],v.z);
                                     mx[0]=std::max(mx[0],v.x);mx[1]=std::max(mx[1],v.y);mx[2]=std::max(mx[2],v.z);}
            g.pos = addAcc(addBV(m.vertices.data(),m.vertices.size()*12,34962),(uint32_t)m.vertices.size(),5126,"VEC3",mn,mx);
        }
        if (!m.faces.empty())
            g.idx = addAcc(addBV(m.faces.data(),m.faces.size()*6,34963),(uint32_t)(m.faces.size()*3),5123,"SCALAR");
        if (!m.uvs.empty()&&m.uvs.size()==m.vertices.size())
            g.uv  = addAcc(addBV(m.uvs.data(),m.uvs.size()*8,34962),(uint32_t)m.uvs.size(),5126,"VEC2");
        if (!m.normals.empty()&&m.normals.size()==m.vertices.size())
            g.nrm = addAcc(addBV(m.normals.data(),m.normals.size()*12,34962),(uint32_t)m.normals.size(),5126,"VEC3");
        mgltf.push_back(g);

        if (!m.ddsTexture.empty()) {
            std::string texName = ToHexID(m.textureAssetID);
            std::string pngName = texName + ".png";
            std::string dp = outputDir + "/" + pngName;

            bool ok = fs::exists(dp);
            if (!ok) {
                ok = DdsToPngFile(m.ddsTexture, dp);
            }

            if (ok) {
                meshToMat[mi] = (int)gltfMats.size();
                gltfMats.push_back({texName, pngName});
            }
        }
    }

    // Build GLTF JSON
    std::ostringstream js;
    js << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"HoExtractor\"},\"scene\":0,";
    js << "\"scenes\":[{\"nodes\":[";
    for (size_t i=0;i<meshes.size();i++){if(i)js<<",";js<<i;}
    js << "]}],\"nodes\":[";
    for (size_t i=0;i<meshes.size();i++){
        if(i)js<<",";
        std::string n=ObjectNameFromName(meshes[i].name);
        std::replace(n.begin(),n.end(),'"','_');
        js<<"{\"mesh\":"<<i<<",\"name\":\""<<n<<"\"}";
    }
    js << "],\"meshes\":[";
    for (size_t mi=0;mi<meshes.size();mi++){
        if(mi)js<<",";
        const MGltf& g=mgltf[mi];
        js<<"{\"primitives\":[{\"attributes\":{";
        bool first=true;
        if(g.pos>=0){js<<"\"POSITION\":"<<g.pos;first=false;}
        if(g.uv >=0){if(!first)js<<",";js<<"\"TEXCOORD_0\":"<<g.uv;first=false;}
        if(g.nrm>=0){if(!first)js<<",";js<<"\"NORMAL\":"<<g.nrm;}
        js<<"}";
        if(g.idx>=0) js<<",\"indices\":"<<g.idx;
        if(meshToMat[mi] >= 0) js<<",\"material\":"<<meshToMat[mi];
        js<<"}]}";
    }
    js << "],\"materials\":[";
    for (size_t i=0; i<gltfMats.size(); i++) {
        if(i)js<<",";
        js<<"{\"name\":\""<<gltfMats[i].name<<"\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":"<<i<<"},\"metallicFactor\":0.0,\"roughnessFactor\":1.0}}";
    }
    js << "],\"textures\":[";
    for (size_t i=0; i<gltfMats.size(); i++) {
        if(i)js<<",";
        js<<"{\"source\":"<<i<<"}";
    }
    js << "],\"images\":[";
    for (size_t i=0; i<gltfMats.size(); i++) {
        if(i)js<<",";
        js<<"{\"uri\":\""<<gltfMats[i].pngUri<<"\"}";
    }
    js<<"],\"bufferViews\":[";
    for(size_t i=0;i<bviews.size();i++){
        if(i)js<<",";
        js<<"{\"buffer\":0,\"byteOffset\":"<<bviews[i].offset<<",\"byteLength\":"<<bviews[i].len<<",\"target\":"<<bviews[i].target<<"}";
    }
    js<<"],\"accessors\":[";
    for(size_t i=0;i<accs.size();i++){
        if(i)js<<",";
        const Accessor& a=accs[i];
        js<<"{\"bufferView\":"<<a.bv<<",\"componentType\":"<<a.ct<<",\"count\":"<<a.count<<",\"type\":\""<<a.type<<"\"";
        if(a.bounds&&a.type=="VEC3")
            js<<",\"min\":["<<a.mn[0]<<","<<a.mn[1]<<","<<a.mn[2]<<"]"
              <<",\"max\":["<<a.mx[0]<<","<<a.mx[1]<<","<<a.mx[2]<<"]";
        js<<"}";
    }
    align4();
    js<<"],\"buffers\":[{\"byteLength\":"<<bin.size()<<"}]}";

    std::string jsonStr = js.str();
    while (jsonStr.size()%4) jsonStr+=' ';
    while (bin.size()%4) bin.push_back(0);

    uint32_t jLen=(uint32_t)jsonStr.size(), bLen=(uint32_t)bin.size();
    uint32_t total=12+8+jLen+(bLen?8+bLen:0);

    std::ofstream glb(outputDir+"/"+safe+".glb",std::ios::binary);
    uint32_t magic=0x46546C67,ver=2;
    glb.write((char*)&magic,4); glb.write((char*)&ver,4); glb.write((char*)&total,4);
    uint32_t jt=0x4E4F534A;
    glb.write((char*)&jLen,4); glb.write((char*)&jt,4); glb.write(jsonStr.data(),jLen);
    if(bLen){ uint32_t bt=0x004E4942;
        glb.write((char*)&bLen,4); glb.write((char*)&bt,4); glb.write((char*)bin.data(),bLen); }
}



bool WriteGroupGLB(
    const std::string& outputPath,
    const std::map<std::string, std::vector<MeshData>>& groups,
    std::string& statusOut)
{
    std::vector<MeshData> allMeshes;
    for (auto const& [name, meshes] : groups) {
        for (auto const& m : meshes) allMeshes.push_back(m);
    }
    
    if (allMeshes.empty()) {
        statusOut = "No meshes to export.";
        return false;
    }

    std::string outDir = fs::path(outputPath).parent_path().string();
    std::string fileName = fs::path(outputPath).stem().string();
    
    try {
        // Call the internal static version
        ExportMeshesToGLB(allMeshes, outDir, fileName); 
        statusOut = "Successfully exported GLB.";
        return true;
    } catch (std::exception& e) {
        statusOut = std::string("GLB Export Error: ") + e.what();
        return false;
    }
}

} // namespace MapExporter
