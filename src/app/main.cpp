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
#include "MapExporter.hpp"
#include "Viewer.hpp"

namespace fs = std::filesystem;

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
    std::map<std::string, std::vector<MapExporter::MeshData>> mapGroups;
    std::map<std::string, bool> groupVisibility;
    Viewer::MapViewer viewer;
    int selectedArchive = -1;
    
    bool enableLogging = false;
    bool showAbout = false;
    std::string statusMsg = "Ready. Load a folder.";
    
    // ...Viewport state...
    uint32_t viewportTex = 0, viewportFbo = 0, viewportDepth = 0;
    int vw = 0, vh = 0;

    auto loadSelectedMap = [&](int idx) {
        if (idx < 0 || idx >= archiveInfos.size()) return;
        
        mapGroups.clear();
        std::ofstream dummyLog;
        if (enableLogging) dummyLog.open("import_log.txt");
        
        // We need all archives for texture resolution
        std::vector<std::shared_ptr<HoLib::Archive>> all;
        for(auto& ai : archiveInfos) all.push_back(ai.arch);

        std::map<std::string, int> nameCounts;

        auto& target = archiveInfos[idx];
        for (auto& asset : target.arch->Assets) {
            if (asset.AssetType == 0x86EF2978 || asset.AssetType == 0xDE180D0E) {
                std::string nameUpper = asset.Name;
                for(auto& c: nameUpper) c = toupper(c);
                if (nameUpper.find("BSP_") != std::string::npos) {
                    MapExporter::MeshData mesh;
                    // Provide a cleaner name to ParseStaticGeometry for logging
                    std::string baseName = MapExporter::ObjectNameFromName(asset.Name);
                    auto dot = baseName.find('.');
                    if (dot != std::string::npos) baseName = baseName.substr(0, dot);
                    
                    int count = nameCounts[baseName]++;
                    if (count > 0) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), ".%03d", count);
                        mesh.name = baseName + buf;
                    } else {
                        mesh.name = baseName;
                    }

                    if (MapExporter::ParseStaticGeometry(all, *target.arch, asset, mesh, dummyLog)) {
                        std::string grp = MapExporter::GroupKeyFromName(asset.Name);
                        mapGroups[grp].push_back(std::move(mesh));
                    }
                }
            }
        }
        viewer.LoadMeshes(mapGroups);
        groupVisibility.clear();
        for (auto const& [name, meshes] : mapGroups) groupVisibility[name] = true;

        statusMsg = "Loaded " + target.arch->SectionName + " (" + std::to_string(mapGroups.size()) + " groups)";
    };

    auto updateViewport = [&](int w, int h) {
        if (w == vw && h == vh) return;
        vw = w; vh = h;
        if (viewportTex) glDeleteTextures(1, &viewportTex);
        if (viewportDepth) glDeleteRenderbuffers(1, &viewportDepth);
        if (viewportFbo) glDeleteFramebuffers(1, &viewportFbo);

        glGenFramebuffers(1, &viewportFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, viewportFbo);

        glGenTextures(1, &viewportTex);
        glBindTexture(GL_TEXTURE_2D, viewportTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewportTex, 0);

        glGenRenderbuffers(1, &viewportDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, viewportDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, viewportDepth);

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
                    auto p = pfd::save_file("Export Map to GLB", "map.glb", { "GLB Files", "*.glb" }).result();
                    if (!p.empty()) {
                        auto filteredGroups = mapGroups;
                        // Filter by visibility AND shadows AND texture validity
                        for (auto it = filteredGroups.begin(); it != filteredGroups.end(); ) {
                            if (!groupVisibility[it->first]) {
                                it = filteredGroups.erase(it);
                            } else {
                                auto& meshes = it->second;
                                meshes.erase(std::remove_if(meshes.begin(), meshes.end(), [&](const MapExporter::MeshData& m) {
                                    if (viewer.showOnlyTextured && !m.hasValidTexture) return true;
                                    
                                    if (!viewer.showShadows) {
                                        std::string lower = m.name;
                                        for(auto& c : lower) c = (char)tolower(c);
                                        return lower.find("shadow") != std::string::npos;
                                    }
                                    return false;
                                }), meshes.end());
                                
                                if (meshes.empty()) it = filteredGroups.erase(it);
                                else ++it;
                            }
                        }
                        if (MapExporter::WriteGroupGLB(p, filteredGroups, statusMsg)) {
                            statusMsg = "Exported to " + p;
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(window, true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Options")) {
                ImGui::MenuItem("Show Shadows", NULL, &viewer.showShadows);
                ImGui::MenuItem("Show Only Textured", NULL, &viewer.showOnlyTextured);
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

        // Left Sidebar: Archives & Groups
        ImGui::BeginChild("Sidebar", ImVec2(300, 0), true);
        
        float availH = ImGui::GetContentRegionAvail().y;

        // --- Archives Section (Top) ---
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Archives in Folder:");
        ImGui::Separator();
        
        ImGui::BeginChild("ArchivesScroll", ImVec2(0, availH * 0.45f), false);
        for (int i = 0; i < (int)archiveInfos.size(); ++i) {
            std::string label = archiveInfos[i].arch->SectionName;
            if (ImGui::Selectable(label.c_str(), selectedArchive == i)) {
                selectedArchive = i;
                loadSelectedMap(i);
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        
        // --- Map Groups Section (Bottom) ---
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Map Groups:");
        
        if (!mapGroups.empty()) {
            if (ImGui::Button("Select All", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 0))) {
                for (auto& pair : groupVisibility) pair.second = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Deselect", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                for (auto& pair : groupVisibility) pair.second = false;
            }
            
            ImGui::BeginChild("GroupsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (auto& pair : groupVisibility) {
                std::string lower = pair.first;
                for (auto& c : lower) c = (char)tolower(c);
                if (lower.find("shadow") != std::string::npos || lower.find("shadowcast") != std::string::npos) {
                    continue;
                }
                ImGui::Checkbox(pair.first.c_str(), &pair.second);
            }
            ImGui::EndChild();
        } else {
            ImGui::TextDisabled("Load a map to see groups");
        }
        
        ImGui::EndChild();
        ImGui::SameLine();

        // Right Content: Viewport
        ImGui::BeginChild("ViewportContainer");
        ImVec2 size = ImGui::GetContentRegionAvail();
        updateViewport((int)size.x, (int)size.y);

        if (vw > 0 && vh > 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, viewportFbo);
            glViewport(0, 0, vw, vh);
            glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // Handle Blender-style Cam
            if (ImGui::IsWindowHovered()) {
                auto& cam = viewer.GetCamera();
                
                // MMB Orbit
                if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !io.KeyShift) {
                    cam.yaw   += io.MouseDelta.x * 0.5f;
                    cam.pitch -= io.MouseDelta.y * 0.5f;
                }
                
                // Shift + MMB Pan
                if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && io.KeyShift) {
                    float speed = cam.distance * 0.001f;
                    glm::vec3 forward = glm::normalize(glm::vec3(cos(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch)), 
                                                                  sin(glm::radians(cam.pitch)), 
                                                                  sin(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch))));
                    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
                    glm::vec3 up = glm::normalize(glm::cross(right, forward));
                    
                    cam.target -= right * io.MouseDelta.x * speed;
                    cam.target += up * io.MouseDelta.y * speed;
                }

                // Zoom
                cam.distance -= io.MouseWheel * (cam.distance * 0.1f);
                if (cam.distance < 0.1f) cam.distance = 0.1f;
                
                viewer.HandleInput(deltaTime);
            }

            viewer.Render(vw, vh, groupVisibility);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            ImGui::Image((void*)(intptr_t)viewportTex, size, ImVec2(0, 1), ImVec2(1, 0));
        }
        ImGui::EndChild();

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
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
