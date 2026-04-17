/*
 * Stewart - Map Editor for Family Guy: Back to the Multiverse
 * Copyright (C) 2026 sakis720
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <map>

#include "glad.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "portable-file-dialogs.h"
#include "resource.h"

#include "HoLib.hpp"
#include "GeometryCommon.hpp"
#include "ObjectExporter.hpp"
#include "SkeletalExporter.hpp"
#include "MapExporter.hpp"
#include "Viewer.hpp"

namespace fs = std::filesystem;
using namespace GeometryCommon;

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Stewart", NULL, NULL);
    if (window == NULL) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    if (hIcon) {
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Disable imgui.ini generation
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // App state
    struct ArchiveInfo {
        std::shared_ptr<HoLib::Archive> arch;
        bool isLoaded = false;
    };
    std::vector<ArchiveInfo> archiveInfos;
    std::map<std::string, std::vector<MeshData>> mapGroups;
    std::map<std::string, bool> groupVisibility;
    
    std::map<std::string, std::vector<MeshData>> skeletalModels;
    std::map<std::string, bool> skeletalVisibility;

    std::map<std::string, std::vector<MeshData>> objectModels;
    std::map<std::string, bool> objectVisibility;

    Viewer::MapViewer viewer;
    Viewer::MapViewer skeletalViewer;
    Viewer::MapViewer objectViewer;

    int activeTab = 0;
    int selectedArchive = -1;
    bool enableLogging = false;
    bool showAbout = false;
    std::string statusMsg = "Ready. Load a folder.";
    
    // Viewport state
    uint32_t viewportTex = 0, viewportFbo = 0, viewportDepth = 0;
    uint32_t skelViewportTex = 0, skelViewportFbo = 0, skelViewportDepth = 0;
    uint32_t objViewportTex = 0, objViewportFbo = 0, objViewportDepth = 0;
    int vw = 0, vh = 0;
    int skvw = 0, skvh = 0;
    int objvw = 0, objvh = 0;

    auto loadSelectedMap = [&](int idx) {
        if (idx < 0 || idx >= (int)archiveInfos.size()) return;
        
        mapGroups.clear();
        skeletalModels.clear();
        objectModels.clear();
        std::ofstream dummyLog;
        if (enableLogging) dummyLog.open("import_log.txt");
        
        std::vector<std::shared_ptr<HoLib::Archive>> all;
        for(auto& ai : archiveInfos) all.push_back(ai.arch);

        std::map<std::string, int> nameCounts;
        std::map<std::string, int> skelNameCounts;
        std::map<std::string, int> objNameCounts;

        auto& target = archiveInfos[idx];
        for (auto& asset : target.arch->Assets) {
            if (asset.AssetType == 0x86EF2978 || asset.AssetType == 0xDE180D0E) {
                std::string nameUpper = asset.Name;
                for(auto& c: nameUpper) c = toupper(c);
                bool isBsp = (nameUpper.find("BSP_") != std::string::npos);

                MeshData mesh;
                std::string baseName = ObjectExporter::ObjectNameFromName(asset.Name);
                auto dot = baseName.find('.');
                if (dot != std::string::npos) baseName = baseName.substr(0, dot);
                
                int& count = isBsp ? nameCounts[baseName] : objNameCounts[baseName];
                if (count > 0) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), ".%03d", count);
                    mesh.name = baseName + buf;
                } else {
                    mesh.name = baseName;
                }
                count++;

                if (ObjectExporter::ParseStaticGeometry(all, *target.arch, asset, mesh, dummyLog)) {
                    std::string grp = ObjectExporter::GroupKeyFromName(asset.Name);
                    if (isBsp) mapGroups[grp].push_back(std::move(mesh));
                    else objectModels[grp].push_back(std::move(mesh));
                }
            }
            else if (asset.AssetType == (uint32_t)HoLib::AssetType::SkinGeometry) {
                MeshData mesh;
                mesh.name = asset.Name;
                if (SkeletalExporter::ParseSkinGeometry(all, *target.arch, asset, mesh, dummyLog)) {
                    std::string groupName = asset.Name;
                    int count = ++skelNameCounts[groupName];
                    char buf[16];
                    snprintf(buf, sizeof(buf), " %02d", count);
                    mesh.name += buf; 
                    skeletalModels[groupName].push_back(std::move(mesh));
                }
            }
        }
        viewer.LoadMeshes(mapGroups);
        groupVisibility.clear();
        for (auto const& [name, meshes] : mapGroups) groupVisibility[name] = true;

        skeletalViewer.LoadMeshes(skeletalModels);
        skeletalVisibility.clear();
        for (auto const& [name, meshes] : skeletalModels) skeletalVisibility[name] = true;

        objectViewer.LoadMeshes(objectModels);
        objectVisibility.clear();
        for (auto const& [name, meshes] : objectModels) objectVisibility[name] = true;

        statusMsg = "Loaded " + target.arch->SectionName + " (" + std::to_string(mapGroups.size()) + " map groups, " + std::to_string(skeletalModels.size()) + " skeletal models, " + std::to_string(objectModels.size()) + " object groups)";
    };

    auto updateViewport = [&](int w, int h, uint32_t& tex, uint32_t& fbo, uint32_t& depth, int& vw_ref, int& vh_ref) {
        if (w == vw_ref && h == vh_ref) return;
        vw_ref = w; vh_ref = h;
        if (tex) glDeleteTextures(1, &tex);
        if (depth) glDeleteRenderbuffers(1, &depth);
        if (fbo) glDeleteFramebuffers(1, &fbo);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };

    float lastTime = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("MainDock", NULL, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load Folder...")) {
                    auto dir = pfd::select_folder("Select Level Folder", ".").result();
                    if (!dir.empty()) {
                        archiveInfos.clear();
                        mapGroups.clear();
                        viewer.LoadMeshes(mapGroups);
                        selectedArchive = -1;
                        
                        for (auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                            if (entry.path().extension() == ".ho") {
                                try {
                                    archiveInfos.push_back({ std::make_shared<HoLib::Archive>(entry.path().string()) });
                                } catch(...) {}
                            }
                        }
                        statusMsg = "Found " + std::to_string(archiveInfos.size()) + " archives. Click one to load map geometry.";
                    }
                }
                if (ImGui::MenuItem("Export GLB...", NULL, false, selectedArchive != -1)) {
                    auto p = pfd::save_file("Export to GLB", "export.glb", { "GLB Files", "*.glb" }).result();
                    if (!p.empty()) {
                        // Use activeTab (assuming it's defined and updated in the tab bar)
                        if (activeTab == 0) { // Map
                            auto filteredGroups = mapGroups;
                            for (auto it = filteredGroups.begin(); it != filteredGroups.end(); ) {
                                if (!groupVisibility[it->first]) { it = filteredGroups.erase(it); }
                                else {
                                    it->second.erase(std::remove_if(it->second.begin(), it->second.end(), [&](const MeshData& m) {
                                        if (viewer.showOnlyTextured && !m.hasValidTexture) return true;
                                        if (!viewer.showShadows) {
                                            std::string lower = m.name;
                                            for(auto& c : lower) c = (char)tolower(c);
                                            return lower.find("shadow") != std::string::npos;
                                        }
                                        return false;
                                    }), it->second.end());
                                    if (it->second.empty()) it = filteredGroups.erase(it); else ++it;
                                }
                            }
                            if (MapExporter::WriteGroupGLB(p, filteredGroups, statusMsg)) statusMsg = "Exported map to " + p;
                        } else if (activeTab == 2) { // Objects
                            std::map<std::string, std::vector<MeshData>> filteredGroups;
                            for (auto const& [name, meshes] : objectModels) {
                                if (objectVisibility[name]) filteredGroups[name] = meshes;
                            }
                            if (MapExporter::WriteGroupGLB(p, filteredGroups, statusMsg)) statusMsg = "Exported objects to " + p;
                        } else {
                            statusMsg = "Export for this tab not implemented.";
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(window, true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Options")) {
                ImGui::TextDisabled("Map Viewer:");
                ImGui::MenuItem("Show Shadows##map", NULL, &viewer.showShadows);
                ImGui::MenuItem("Show Only Textured##map", NULL, &viewer.showOnlyTextured);
                ImGui::Separator();
                ImGui::TextDisabled("Skeletal Viewer:");
                ImGui::MenuItem("Show Only Textured##skel", NULL, &skeletalViewer.showOnlyTextured);
                ImGui::Separator();
                ImGui::TextDisabled("Object Viewer:");
                //ImGui::MenuItem("Show Shadows##obj", NULL, &objectViewer.showShadows);
                ImGui::MenuItem("Show Only Textured##obj", NULL, &objectViewer.showOnlyTextured);
                ImGui::Separator();
                ImGui::MenuItem("Enable Logging", NULL, &enableLogging);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) showAbout = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (showAbout) {
            ImGui::OpenPopup("About");
        }
        if (ImGui::BeginPopupModal("About", &showAbout, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Stewart");
            ImGui::Separator();
            ImGui::Text("Map& 3D Viewer for");
            ImGui::Text("Family Guy: Back to the Multiverse");
            ImGui::Spacing();
            ImGui::Text("Developed by: sakis720");
            ImGui::Text("License: GPL-3.0");
            ImGui::Text("Version: 0.7 (WIP)");
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0))) { showAbout = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Map Viewer")) {
                activeTab = 0;
                // Left Sidebar: Archives & Groups
                ImGui::BeginChild("Sidebar", ImVec2(350, 0), true);
                float availH = ImGui::GetContentRegionAvail().y;
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Archives in Folder:");
                ImGui::Separator();
                ImGui::BeginChild("ArchivesScroll", ImVec2(0, availH * 0.45f), false);
                for (int i = 0; i < (int)archiveInfos.size(); ++i) {
                    if (ImGui::Selectable(archiveInfos[i].arch->SectionName.c_str(), selectedArchive == i)) {
                        selectedArchive = i;
                        loadSelectedMap(i);
                    }
                }
                ImGui::EndChild();
                ImGui::Spacing(); ImGui::Separator();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Map Groups:");
                if (!mapGroups.empty()) {
                    if (ImGui::Button("Select All##map", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 0))) {
                        for (auto& pair : groupVisibility) pair.second = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Deselect##map", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        for (auto& pair : groupVisibility) pair.second = false;
                    }
                    ImGui::BeginChild("GroupsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                    for (auto& pair : groupVisibility) {
                        std::string lower = pair.first; for (auto& c : lower) c = (char)tolower(c);
                        if (lower.find("shadow") != std::string::npos || lower.find("shadowcast") != std::string::npos) continue;
                        ImGui::Checkbox(pair.first.c_str(), &pair.second);
                    }
                    ImGui::EndChild();
                } else ImGui::TextDisabled("Load a map to see groups");
                ImGui::EndChild();
                ImGui::SameLine();

                // Right Content: Viewport
                ImGui::BeginChild("ViewportContainer");
                ImVec2 size = ImGui::GetContentRegionAvail();
                updateViewport((int)size.x, (int)size.y, viewportTex, viewportFbo, viewportDepth, vw, vh);
                if (vw > 0 && vh > 0) {
                    glBindFramebuffer(GL_FRAMEBUFFER, viewportFbo);
                    glViewport(0, 0, vw, vh);
                    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    
                    bool hovered = ImGui::IsWindowHovered();
                    if (hovered && !viewer.isFlyMode) {
                        auto& cam = viewer.GetCamera();
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !io.KeyShift) {
                            cam.yaw += io.MouseDelta.x * 0.5f; cam.pitch -= io.MouseDelta.y * 0.5f;
                        }
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && io.KeyShift) {
                            float speed = cam.distance * 0.001f;
                            glm::vec3 fwd = glm::normalize(glm::vec3(cos(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch)), sin(glm::radians(cam.pitch)), sin(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch))));
                            glm::vec3 rgt = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
                            glm::vec3 up = glm::normalize(glm::cross(rgt, fwd));
                            cam.target -= rgt * io.MouseDelta.x * speed; cam.target += up * io.MouseDelta.y * speed;
                        }
                        cam.distance -= io.MouseWheel * (cam.distance * 0.1f);
                        if (cam.distance < 0.1f) cam.distance = 0.1f;
                        viewer.HandleInput(deltaTime);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Z) && (hovered || viewer.isFlyMode)) {
                        viewer.isFlyMode = !viewer.isFlyMode;
                        glfwSetInputMode(window, GLFW_CURSOR, viewer.isFlyMode ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                        if (viewer.isFlyMode) viewer.GetCamera().orbitMode = false;
                    }
                    if (viewer.isFlyMode && (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                        viewer.isFlyMode = false; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_O) && (hovered || viewer.isFlyMode)) viewer.GetCamera().orbitMode = !viewer.GetCamera().orbitMode;
                    if (ImGui::IsKeyDown(ImGuiKey_KeypadAdd)) viewer.movementSpeed += 100.0f * deltaTime;
                    if (ImGui::IsKeyDown(ImGuiKey_KeypadSubtract)) viewer.movementSpeed -= 100.0f * deltaTime;
                    if (viewer.movementSpeed < 1.0f) viewer.movementSpeed = 1.0f;
                    if (viewer.isFlyMode) viewer.HandleInput(deltaTime);

                    viewer.Render(vw, vh, groupVisibility);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    ImGui::Image((void*)(intptr_t)viewportTex, size, ImVec2(0, 1), ImVec2(1, 0));
                    
                    ImVec2 overlayPos = ImVec2(ImGui::GetItemRectMin().x + 10, ImGui::GetItemRectMin().y + 10);
                    ImGui::SetCursorScreenPos(overlayPos);
                    ImGui::BeginGroup();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), viewer.isFlyMode ? "FLY MODE ACTIVE" : "MAP VIEWER");
                    ImGui::Text("Camera: %s", viewer.GetCamera().orbitMode ? "Orbit" : "Free");
                    ImGui::Text("Speed: %.1f", viewer.movementSpeed);
                    ImGui::EndGroup();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Skeletal Viewer")) {
                activeTab = 1;
                // Left Sidebar: Archives & Models
                ImGui::BeginChild("SkelSidebar", ImVec2(350, 0), true);
                float availH = ImGui::GetContentRegionAvail().y;
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Archives in Folder:");
                ImGui::Separator();
                ImGui::BeginChild("SkelArchivesScroll", ImVec2(0, availH * 0.45f), false);
                for (int i = 0; i < (int)archiveInfos.size(); ++i) {
                    if (ImGui::Selectable((archiveInfos[i].arch->SectionName + "##skel").c_str(), selectedArchive == i)) {
                        selectedArchive = i; loadSelectedMap(i);
                    }
                }
                ImGui::EndChild();
                ImGui::Spacing(); ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Skeletal Models:");
                if (!skeletalModels.empty()) {
                    if (ImGui::Button("Select All##skel", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 0))) {
                        for (auto& pair : skeletalVisibility) {
                            bool allSupported = true; for(auto& m : skeletalModels[pair.first]) if(!m.isSupported) { allSupported = false; break; }
                            if(allSupported) pair.second = true;
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Deselect##skel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        for (auto& pair : skeletalVisibility) pair.second = false;
                    }
                    ImGui::BeginChild("SkelModelsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                    for (auto& pair : skeletalVisibility) {
                        bool anyUnsupported = false; for(auto& m : skeletalModels[pair.first]) if(!m.isSupported) { anyUnsupported = true; break; }
                        if (anyUnsupported) { ImGui::BeginDisabled(); bool dummy = false; ImGui::Checkbox((pair.first + " (Not Supported)").c_str(), &dummy); ImGui::EndDisabled(); }
                        else ImGui::Checkbox(pair.first.c_str(), &pair.second);
                    }
                    ImGui::EndChild();
                } else ImGui::TextDisabled("Load an archive to see models");
                ImGui::EndChild();
                ImGui::SameLine();

                // Right Content: Viewport
                ImGui::BeginChild("SkelViewportContainer");
                ImVec2 size = ImGui::GetContentRegionAvail();
                updateViewport((int)size.x, (int)size.y, skelViewportTex, skelViewportFbo, skelViewportDepth, skvw, skvh);
                if (skvw > 0 && skvh > 0) {
                    glBindFramebuffer(GL_FRAMEBUFFER, skelViewportFbo);
                    glViewport(0, 0, skvw, skvh);
                    glClearColor(0.12f, 0.1f, 0.1f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    bool hovered = ImGui::IsWindowHovered();
                    if (hovered && !skeletalViewer.isFlyMode) {
                        auto& cam = skeletalViewer.GetCamera();
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !io.KeyShift) {
                            cam.yaw += io.MouseDelta.x * 0.5f; cam.pitch -= io.MouseDelta.y * 0.5f;
                        }
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && io.KeyShift) {
                            float speed = cam.distance * 0.001f;
                            glm::vec3 fwd = glm::normalize(glm::vec3(cos(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch)), sin(glm::radians(cam.pitch)), sin(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch))));
                            glm::vec3 rgt = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
                            glm::vec3 up = glm::normalize(glm::cross(rgt, fwd));
                            cam.target -= rgt * io.MouseDelta.x * speed; cam.target += up * io.MouseDelta.y * speed;
                        }
                        cam.distance -= io.MouseWheel * (cam.distance * 0.1f);
                        if (cam.distance < 0.1f) cam.distance = 0.1f;
                        skeletalViewer.HandleInput(deltaTime);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Z) && (hovered || skeletalViewer.isFlyMode)) {
                        skeletalViewer.isFlyMode = !skeletalViewer.isFlyMode;
                        glfwSetInputMode(window, GLFW_CURSOR, skeletalViewer.isFlyMode ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                        if (skeletalViewer.isFlyMode) skeletalViewer.GetCamera().orbitMode = false;
                    }
                    if (skeletalViewer.isFlyMode && (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                        skeletalViewer.isFlyMode = false; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_O) && (hovered || skeletalViewer.isFlyMode)) skeletalViewer.GetCamera().orbitMode = !skeletalViewer.GetCamera().orbitMode;
                    if (ImGui::IsKeyDown(ImGuiKey_KeypadAdd)) skeletalViewer.movementSpeed += 100.0f * deltaTime;
                    if (ImGui::IsKeyDown(ImGuiKey_KeypadSubtract)) skeletalViewer.movementSpeed -= 100.0f * deltaTime;
                    if (skeletalViewer.movementSpeed < 1.0f) skeletalViewer.movementSpeed = 1.0f;
                    if (skeletalViewer.isFlyMode) skeletalViewer.HandleInput(deltaTime);
                    skeletalViewer.Render(skvw, skvh, skeletalVisibility);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    ImGui::Image((void*)(intptr_t)skelViewportTex, size, ImVec2(0, 1), ImVec2(1, 0));
                    ImVec2 overlayPos = ImVec2(ImGui::GetItemRectMin().x + 10, ImGui::GetItemRectMin().y + 10);
                    ImGui::SetCursorScreenPos(overlayPos);
                    ImGui::BeginGroup();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1), skeletalViewer.isFlyMode ? "FLY MODE ACTIVE" : "SKELETAL VIEWER");
                    ImGui::Text("Camera: %s", skeletalViewer.GetCamera().orbitMode ? "Orbit" : "Free");
                    ImGui::Text("Speed: %.1f", skeletalViewer.movementSpeed);
                    ImGui::EndGroup();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Object Viewer")) {
                activeTab = 2;
                // Left Sidebar: Archives & Objects
                ImGui::BeginChild("ObjSidebar", ImVec2(350, 0), true);
                float availH = ImGui::GetContentRegionAvail().y;
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Archives in Folder:");
                ImGui::Separator();
                ImGui::BeginChild("ObjArchivesScroll", ImVec2(0, availH * 0.45f), false);
                for (int i = 0; i < (int)archiveInfos.size(); ++i) {
                    if (ImGui::Selectable((archiveInfos[i].arch->SectionName + "##obj").c_str(), selectedArchive == i)) {
                        selectedArchive = i; loadSelectedMap(i);
                    }
                }
                ImGui::EndChild();
                ImGui::Spacing(); ImGui::Separator();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "Object Groups:");
                if (!objectModels.empty()) {
                    if (ImGui::Button("Select All##obj", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 0))) {
                        for (auto& pair : objectVisibility) pair.second = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Deselect##obj", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        for (auto& pair : objectVisibility) pair.second = false;
                    }
                    ImGui::BeginChild("ObjGroupsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                    for (auto& pair : objectVisibility) {
                        ImGui::Checkbox(pair.first.c_str(), &pair.second);
                    }
                    ImGui::EndChild();
                } else ImGui::TextDisabled("Load an archive to see objects");
                ImGui::EndChild();
                ImGui::SameLine();

                // Right Content: Viewport
                ImGui::BeginChild("ObjViewportContainer");
                ImVec2 size = ImGui::GetContentRegionAvail();
                updateViewport((int)size.x, (int)size.y, objViewportTex, objViewportFbo, objViewportDepth, objvw, objvh);
                if (objvw > 0 && objvh > 0) {
                    glBindFramebuffer(GL_FRAMEBUFFER, objViewportFbo);
                    glViewport(0, 0, objvw, objvh);
                    glClearColor(0.1f, 0.12f, 0.1f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    bool hovered = ImGui::IsWindowHovered();
                    if (hovered && !objectViewer.isFlyMode) {
                        auto& cam = objectViewer.GetCamera();
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !io.KeyShift) {
                            cam.yaw += io.MouseDelta.x * 0.5f; cam.pitch -= io.MouseDelta.y * 0.5f;
                        }
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && io.KeyShift) {
                            float speed = cam.distance * 0.001f;
                            glm::vec3 fwd = glm::normalize(glm::vec3(cos(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch)), sin(glm::radians(cam.pitch)), sin(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch))));
                            glm::vec3 rgt = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
                            glm::vec3 up = glm::normalize(glm::cross(rgt, fwd));
                            cam.target -= rgt * io.MouseDelta.x * speed; cam.target += up * io.MouseDelta.y * speed;
                        }
                        cam.distance -= io.MouseWheel * (cam.distance * 0.1f);
                        if (cam.distance < 0.1f) cam.distance = 0.1f;
                        objectViewer.HandleInput(deltaTime);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Z) && (hovered || objectViewer.isFlyMode)) {
                        objectViewer.isFlyMode = !objectViewer.isFlyMode;
                        glfwSetInputMode(window, GLFW_CURSOR, objectViewer.isFlyMode ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                        if (objectViewer.isFlyMode) objectViewer.GetCamera().orbitMode = false;
                    }
                    if (objectViewer.isFlyMode && (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                        objectViewer.isFlyMode = false; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_O) && (hovered || objectViewer.isFlyMode)) objectViewer.GetCamera().orbitMode = !objectViewer.GetCamera().orbitMode;
                    if (ImGui::IsKeyDown(ImGuiKey_KeypadAdd)) objectViewer.movementSpeed += 100.0f * deltaTime;
                    if (ImGui::IsKeyDown(ImGuiKey_KeypadSubtract)) objectViewer.movementSpeed -= 100.0f * deltaTime;
                    if (objectViewer.movementSpeed < 1.0f) objectViewer.movementSpeed = 1.0f;
                    if (objectViewer.isFlyMode) objectViewer.HandleInput(deltaTime);
                    objectViewer.Render(objvw, objvh, objectVisibility);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    ImGui::Image((void*)(intptr_t)objViewportTex, size, ImVec2(0, 1), ImVec2(1, 0));
                    ImVec2 overlayPos = ImVec2(ImGui::GetItemRectMin().x + 10, ImGui::GetItemRectMin().y + 10);
                    ImGui::SetCursorScreenPos(overlayPos);
                    ImGui::BeginGroup();
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1), objectViewer.isFlyMode ? "FLY MODE ACTIVE" : "OBJECT VIEWER");
                    ImGui::Text("Camera: %s", objectViewer.GetCamera().orbitMode ? "Orbit" : "Free");
                    ImGui::Text("Speed: %.1f", objectViewer.movementSpeed);
                    ImGui::EndGroup();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End(); // MainDock

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (viewportTex) glDeleteTextures(1, &viewportTex);
    if (viewportDepth) glDeleteRenderbuffers(1, &viewportDepth);
    if (viewportFbo) glDeleteFramebuffers(1, &viewportFbo);
    
    if (skelViewportTex) glDeleteTextures(1, &skelViewportTex);
    if (skelViewportDepth) glDeleteRenderbuffers(1, &skelViewportDepth);
    if (skelViewportFbo) glDeleteFramebuffers(1, &skelViewportFbo);
    
    if (objViewportTex) glDeleteTextures(1, &objViewportTex);
    if (objViewportDepth) glDeleteRenderbuffers(1, &objViewportDepth);
    if (objViewportFbo) glDeleteFramebuffers(1, &objViewportFbo);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
