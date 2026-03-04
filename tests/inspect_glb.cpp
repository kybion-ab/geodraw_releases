/*******************************************************************************
 * File: inspect_glb.cpp
 *
 * Description: Debug tool to inspect glTF model structure
 *
 *******************************************************************************/

#include <nlohmann/json.hpp>

#include "geodraw/external/stb/stb_image.h"
#include "geodraw/external/stb/stb_image_write.h"
#include "geodraw/external/tinygltf/tiny_gltf.h"

#include <iostream>
#include <string>

void inspectGLB(const std::string& path) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool success = loader.LoadBinaryFromFile(&model, &err, &warn, path);

    if (!success) {
        std::cerr << "Failed to load: " << err << "\n";
        return;
    }

    std::cout << "=== glTF Model Structure: " << path << " ===\n\n";

    std::cout << "Scenes: " << model.scenes.size() << "\n";
    std::cout << "Nodes: " << model.nodes.size() << "\n";
    std::cout << "Meshes: " << model.meshes.size() << "\n";
    std::cout << "Materials: " << model.materials.size() << "\n\n";

    // Print scene hierarchy
    if (!model.scenes.empty()) {
        std::cout << "=== Scene Hierarchy ===\n";
        const auto& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
        std::cout << "Scene: " << scene.name << "\n";
        std::cout << "Root nodes: " << scene.nodes.size() << "\n\n";
    }

    // Print all nodes with their transforms
    std::cout << "=== Nodes ===\n";
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const auto& node = model.nodes[i];
        std::cout << "[" << i << "] " << node.name << "\n";

        if (node.mesh >= 0) {
            std::cout << "  Mesh: " << node.mesh << " (" << model.meshes[node.mesh].name << ")\n";
        }

        if (!node.translation.empty()) {
            std::cout << "  Translation: [" << node.translation[0] << ", "
                      << node.translation[1] << ", " << node.translation[2] << "]\n";
        }

        if (!node.rotation.empty()) {
            std::cout << "  Rotation (quat): [" << node.rotation[0] << ", "
                      << node.rotation[1] << ", " << node.rotation[2] << ", "
                      << node.rotation[3] << "]\n";
        }

        if (!node.scale.empty()) {
            std::cout << "  Scale: [" << node.scale[0] << ", "
                      << node.scale[1] << ", " << node.scale[2] << "]\n";
        }

        if (!node.matrix.empty()) {
            std::cout << "  Matrix: [4x4 matrix present]\n";
        }

        if (!node.children.empty()) {
            std::cout << "  Children: ";
            for (auto c : node.children) std::cout << c << " ";
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    // Print mesh info
    std::cout << "=== Meshes ===\n";
    for (size_t i = 0; i < model.meshes.size(); ++i) {
        const auto& mesh = model.meshes[i];
        std::cout << "[" << i << "] " << mesh.name << "\n";
        std::cout << "  Primitives: " << mesh.primitives.size() << "\n";

        for (size_t j = 0; j < mesh.primitives.size(); ++j) {
            const auto& prim = mesh.primitives[j];
            std::cout << "    [" << j << "] Material: " << prim.material;
            if (prim.material >= 0 && prim.material < static_cast<int>(model.materials.size())) {
                std::cout << " (" << model.materials[prim.material].name << ")";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    // Print materials with colors
    std::cout << "=== Materials ===\n";
    for (size_t i = 0; i < model.materials.size(); ++i) {
        const auto& mat = model.materials[i];
        std::cout << "[" << i << "] " << mat.name << "\n";

        const auto& color = mat.pbrMetallicRoughness.baseColorFactor;
        if (color.size() >= 3) {
            std::cout << "  Base Color: RGB("
                      << color[0] << ", " << color[1] << ", " << color[2] << ")\n";
        }
        std::cout << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-glb>\n";
        return 1;
    }

    inspectGLB(argv[1]);
    return 0;
}
