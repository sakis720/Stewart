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
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glad.h"
#include "MapExporter.hpp"

namespace Viewer {

struct GpuMesh {
    uint32_t vao = 0, vbo = 0, ebo = 0;
    uint32_t textureId = 0;
    uint32_t indexCount = 0;
    std::string name;
};

struct Camera {
    glm::vec3 position = {0, 5, 10};
    glm::vec3 target = {0, 0, 0};
    float fov = 45.0f;
    float yaw = -90.0f;
    float pitch = 0.0f;
    float distance = 10.0f;
    bool orbitMode = true;

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspect) const;
};

class MapViewer {
public:
    MapViewer();
    ~MapViewer();

    void LoadMeshes(const std::map<std::string, std::vector<MapExporter::MeshData>>& groups);
    void Render(int width, int height);
    void HandleInput(float deltaTime);

    bool showShadows = false;
    Camera& GetCamera() { return camera; }

private:
    void Clear();
    uint32_t LoadTexture(const std::vector<uint8_t>& ddsData);

    std::map<std::string, std::vector<GpuMesh>> gpuGroups;
    Camera camera;
    uint32_t shaderProgram = 0;
};

} // namespace Viewer
