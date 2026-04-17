/*
 * Stewart - Map Editor for Family Guy: Back to the Multiverse
 * Copyright (C) 2026 sakis720
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "glad.h"
#include <GLFW/glfw3.h>
#include "Viewer.hpp"
#include "DdsConverter.hpp"
#include "imgui.h"
#include <iostream>

namespace Viewer {

glm::mat4 Camera::GetViewMatrix() const {
    if (orbitMode) {
        float rYaw = glm::radians(yaw);
        float rPitch = glm::radians(pitch);
        glm::vec3 pos;
        pos.x = target.x + distance * cos(rPitch) * cos(rYaw);
        pos.y = target.y + distance * sin(rPitch);
        pos.z = target.z + distance * cos(rPitch) * sin(rYaw);
        return glm::lookAt(pos, target, glm::vec3(0, 1, 0));
    }
    return glm::lookAt(position, position + glm::vec3(cos(glm::radians(yaw)) * cos(glm::radians(pitch)), sin(glm::radians(pitch)), sin(glm::radians(yaw)) * cos(glm::radians(pitch))), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::GetProjectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 10000.0f);
}

MapViewer::MapViewer() {
    // Simple shader
    const char* vShader = "#version 130\n"
        "in vec3 pos; in vec2 uv; in vec3 nrm;\n"
        "out vec2 vUv; out vec3 vNrm;\n"
        "uniform mat4 mvp; \n"
        "void main() { vUv = uv; vNrm = nrm; gl_Position = mvp * vec4(pos, 1.0); }";
    const char* fShader = "#version 130\n"
        "in vec2 vUv; in vec3 vNrm;\n"
        "out vec4 color;\n"
        "uniform sampler2D tex; uniform bool hasTex;\n"
        "uniform vec3 tint;\n"
        "void main() {\n"
        "  vec3 light = normalize(vec3(0.5, 1.0, 0.3));\n"
        "  float d = max(dot(normalize(vNrm), light), 0.3);\n"
        "  if(hasTex) color = texture(tex, vUv) * vec4(tint, 1.0) * d;\n"
        "  else color = vec4(tint * 0.7, 1.0) * d;\n"
        "}";

    auto compile = [](GLenum type, const char* src) {
        uint32_t s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    };

    uint32_t vs = compile(GL_VERTEX_SHADER, vShader);
    uint32_t fs = compile(GL_FRAGMENT_SHADER, fShader);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

MapViewer::~MapViewer() {
    Clear();
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

void MapViewer::Clear() {
    for (auto& gp : gpuGroups) {
        for (auto& m : gp.second) {
            glDeleteVertexArrays(1, &m.vao);
            glDeleteBuffers(1, &m.vbo);
            glDeleteBuffers(1, &m.ebo);
            if (m.textureId) glDeleteTextures(1, &m.textureId);
        }
    }
    gpuGroups.clear();
}

uint32_t MapViewer::LoadTexture(const std::vector<uint8_t>& ddsData) {
    if (ddsData.empty()) return 0;
    RawImage raw = DdsToRawRgba(ddsData);
    if (raw.pixels.empty()) return 0;

    uint32_t tid;
    glGenTextures(1, &tid);
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, raw.width, raw.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    return tid;
}

void MapViewer::LoadMeshes(const std::map<std::string, std::vector<GeometryCommon::MeshData>>& groups) {
    Clear();
    for (auto& g : groups) {
        for (auto& m : g.second) {
            GpuMesh gm;
            gm.name = m.name;
            gm.indexCount = (uint32_t)m.faces.size() * 3;
            gm.hasValidTexture = m.hasValidTexture;
            gm.tint = { m.tint[0], m.tint[1], m.tint[2] };
            gm.isSupported = m.isSupported;

            struct Vert { glm::vec3 p; glm::vec2 u; glm::vec3 n; };
            std::vector<Vert> verts(m.vertices.size());
            for (size_t i = 0; i < m.vertices.size(); i++) {
                verts[i].p = { m.vertices[i].x, m.vertices[i].y, m.vertices[i].z };
                if (i < m.uvs.size()) verts[i].u = { m.uvs[i].u, m.uvs[i].v };
                if (i < m.normals.size()) verts[i].n = { m.normals[i].x, m.normals[i].y, m.normals[i].z };
            }

            glGenVertexArrays(1, &gm.vao);
            glBindVertexArray(gm.vao);

            glGenBuffers(1, &gm.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
            glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vert), verts.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &gm.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.faces.size() * 6, m.faces.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0); // pos
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)0);
            glEnableVertexAttribArray(1); // uv
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)12);
            glEnableVertexAttribArray(2); // nrm
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)20);

            gm.textureId = LoadTexture(m.ddsTexture);
            gpuGroups[g.first].push_back(gm);
        }
    }
}

void MapViewer::Render(int width, int height, const std::map<std::string, bool>& groupVisibility, const std::map<std::string, bool>& meshVisibility) {
    if (width <= 0 || height <= 0) return;
    
    glUseProgram(shaderProgram);
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 proj = camera.GetProjectionMatrix((float)width / height);
    
    int mvpLoc = glGetUniformLocation(shaderProgram, "mvp");
    int hasTexLoc = glGetUniformLocation(shaderProgram, "hasTex");
    int tintLoc = glGetUniformLocation(shaderProgram, "tint");

    glEnable(GL_DEPTH_TEST);
    for (auto& gp : gpuGroups) {
        auto it = groupVisibility.find(gp.first);
        if (it != groupVisibility.end() && !it->second) continue;

        for (auto& m : gp.second) {
            if (!m.isSupported) continue;
            if (showOnlyTextured && !m.hasValidTexture) continue;

            auto vit = meshVisibility.find(m.name);
            if (vit != meshVisibility.end() && !vit->second) continue;

            if (!showShadows) {
                // Skip meshes with "Shadow" or "ShadowCaster" in name (case-insensitive-ish)
                std::string lower = m.name;
                for(auto& c : lower) c = (char)tolower(c);
                if (lower.find("shadow") != std::string::npos || lower.find("shadowcaster") != std::string::npos)
                    continue;
            }

            glm::mat4 mvp = proj * view; // No model matrix yet
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &mvp[0][0]);
            
            if (m.textureId) {
                glUniform1i(hasTexLoc, 1);
                glBindTexture(GL_TEXTURE_2D, m.textureId);
            } else {
                glUniform1i(hasTexLoc, 0);
            }

            glUniform3fv(tintLoc, 1, &m.tint[0]);
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_SHORT, 0);
        }
    }
}

void MapViewer::HandleInput(float deltaTime) {
    ImGuiIO& io = ImGui::GetIO();
    float speed = movementSpeed * deltaTime;

    if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) speed *= 3.0f;

    if (isFlyMode) {
        camera.yaw += io.MouseDelta.x * 0.15f;
        camera.pitch -= io.MouseDelta.y * 0.15f;
        if (camera.pitch > 89.0f) camera.pitch = 89.0f;
        if (camera.pitch < -89.0f) camera.pitch = -89.0f;
    }

    glm::vec3 forward = glm::normalize(glm::vec3(cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch)), 
                                                  sin(glm::radians(camera.pitch)), 
                                                  sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch))));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    if (camera.orbitMode) {
        // Panning the target
        glm::vec3 panForward = glm::normalize(glm::vec3(forward.x, 0, forward.z));
        if (ImGui::IsKeyDown(ImGuiKey_W)) camera.target += panForward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) camera.target -= panForward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) camera.target -= right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) camera.target += right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) camera.target += glm::vec3(0, 1, 0) * speed;
        if (ImGui::IsKeyDown(ImGuiKey_E)) camera.target -= glm::vec3(0, 1, 0) * speed;
    } else {
        // Free look movement
        if (ImGui::IsKeyDown(ImGuiKey_W)) camera.position += forward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) camera.position -= forward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) camera.position -= right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) camera.position += right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) camera.position += up * speed;
        if (ImGui::IsKeyDown(ImGuiKey_E)) camera.position -= up * speed;
    }
}

} // namespace Viewer
   
 