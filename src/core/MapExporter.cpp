/*
 * Stewart - Map Editor for Family Guy: Back to the Multiverse
 * Copyright (C) 2026 sakis720
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "MapExporter.hpp"
#include "DdsConverter.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace MapExporter {

namespace fs = std::filesystem;
using namespace GeometryCommon;

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

    struct GltfMat { 
        uint64_t texID; 
        float r, g, b; 
        std::string pngUri;
        int texIndex = -1;
    };
    std::vector<GltfMat> materials;
    std::vector<int> meshToMat(meshes.size(), -1);

    for (size_t mi = 0; mi < meshes.size(); mi++) {
        const MeshData& m = meshes[mi];
        int matIdx = -1;
        for (int i = 0; i < (int)materials.size(); i++) {
            if (materials[i].texID == m.textureAssetID && 
                materials[i].r == m.tint[0] && 
                materials[i].g == m.tint[1] && 
                materials[i].b == m.tint[2]) {
                matIdx = i;
                break;
            }
        }
        if (matIdx == -1) {
            matIdx = (int)materials.size();
            GltfMat newMat;
            newMat.texID = m.textureAssetID;
            newMat.r = m.tint[0];
            newMat.g = m.tint[1];
            newMat.b = m.tint[2];
            if (m.textureAssetID != 0 && !m.ddsTexture.empty()) {
                std::string texName = ToHexID(m.textureAssetID);
                newMat.pngUri = texName + ".png";
                std::string dp = outputDir + "/" + newMat.pngUri;
                if (!fs::exists(dp)) DdsToPngFile(m.ddsTexture, dp);
            }
            materials.push_back(newMat);
        }
        meshToMat[mi] = matIdx;

        MGltf g;
        if (!m.vertices.empty()) {
            float mn[3] = { m.vertices[0].x, m.vertices[0].y, m.vertices[0].z };
            float mx[3] = { mn[0], mn[1], mn[2] };
            for (auto& v : m.vertices) {
                mn[0] = std::min(mn[0], v.x); mn[1] = std::min(mn[1], v.y); mn[2] = std::min(mn[2], v.z);
                mx[0] = std::max(mx[0], v.x); mx[1] = std::max(mx[1], v.y); mx[2] = std::max(mx[2], v.z);
            }
            g.pos = addAcc(addBV(m.vertices.data(), m.vertices.size() * 12, 34962), (uint32_t)m.vertices.size(), 5126, "VEC3", mn, mx);
        }
        if (!m.faces.empty())
            g.idx = addAcc(addBV(m.faces.data(),m.faces.size()*6,34963),(uint32_t)(m.faces.size()*3),5123,"SCALAR");
        if (!m.uvs.empty()&&m.uvs.size()==m.vertices.size())
            g.uv  = addAcc(addBV(m.uvs.data(),m.uvs.size()*8,34962),(uint32_t)m.uvs.size(),5126,"VEC2");
        if (!m.normals.empty()&&m.normals.size()==m.vertices.size())
            g.nrm = addAcc(addBV(m.normals.data(),m.normals.size()*12,34962),(uint32_t)m.normals.size(),5126,"VEC3");
        mgltf.push_back(g);
    }

    struct GltfTex { int sourceIndex; };
    std::vector<GltfTex> gltfTextures;
    struct GltfImg { std::string uri; };
    std::vector<GltfImg> gltfImages;

    for (auto& mat : materials) {
        if (!mat.pngUri.empty()) {
            int imgIdx = -1;
            for (int i = 0; i < (int)gltfImages.size(); i++) {
                if (gltfImages[i].uri == mat.pngUri) { imgIdx = i; break; }
            }
            if (imgIdx == -1) {
                imgIdx = (int)gltfImages.size();
                gltfImages.push_back({mat.pngUri});
            }
            mat.texIndex = (int)gltfTextures.size();
            gltfTextures.push_back({imgIdx});
        }
    }

    std::ostringstream js;
    js << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Stewart\"},\"scene\":0,";
    js << "\"scenes\":[{\"nodes\":[";
    for (size_t i=0;i<meshes.size();i++){if(i)js<<",";js<<i;}
    js << "]}],\"nodes\":[";
    for (size_t i=0;i<meshes.size();i++){
        if(i)js<<",";
        std::string n=meshes[i].name;
        std::replace(n.begin(),n.end(),'"','_');
        js<<"{\"mesh\":"<<i<<",\"name\":\""<<n<<"\"}";
    }
    js << "],\"meshes\":[";
    for (size_t mi=0; mi<meshes.size(); mi++){
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
    for (size_t i=0; i<materials.size(); i++) {
        if(i)js<<",";
        auto& mat = materials[i];
        js<<"{\"name\":\"mat_"<<i<<"\",\"pbrMetallicRoughness\":{";
        js<<"\"baseColorFactor\":["<<mat.r<<","<<mat.g<<","<<mat.b<<",1.0],";
        if (mat.texIndex >= 0) js<<"\"baseColorTexture\":{\"index\":"<<mat.texIndex<<"},";
        js<<"\"metallicFactor\":0.0,\"roughnessFactor\":1.0}}";
    }
    js << "],\"textures\":[";
    for (size_t i=0; i<gltfTextures.size(); i++) {
        if(i)js<<",";
        js<<"{\"source\":"<<gltfTextures[i].sourceIndex<<"}";
    }
    js << "],\"images\":[";
    for (size_t i=0; i<gltfImages.size(); i++) {
        if(i)js<<",";
        js<<"{\"uri\":\""<<gltfImages[i].uri<<"\"}";
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
    if (allMeshes.empty()) { statusOut = "No meshes to export."; return false; }

    std::string outDir = fs::path(outputPath).parent_path().string();
    std::string fileName = fs::path(outputPath).stem().string();
    try {
        ExportMeshesToGLB(allMeshes, outDir, fileName); 
        statusOut = "Successfully exported GLB.";
        return true;
    } catch (std::exception& e) {
        statusOut = std::string("GLB Export Error: ") + e.what();
        return false;
    }
}

} // namespace MapExporter
