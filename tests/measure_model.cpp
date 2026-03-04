/*******************************************************************************
 * File: measure_model.cpp
 *
 * Description: Quick tool to measure glTF model dimensions
 *
 *******************************************************************************/

#include <nlohmann/json.hpp>

#include "geodraw/external/stb/stb_image.h"
#include "geodraw/external/stb/stb_image_write.h"
#include "geodraw/external/tinygltf/tiny_gltf.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <limits>

glm::mat4 getNodeTransform(const tinygltf::Node& node) {
    glm::mat4 transform(1.0f);

    if (node.matrix.size() == 16) {
        for (int i = 0; i < 16; i++) {
            transform[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
        return transform;
    }

    if (node.translation.size() == 3) {
        transform = glm::translate(transform, glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])
        ));
    }

    if (node.rotation.size() == 4) {
        glm::quat rotation(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2])
        );
        transform = transform * glm::mat4_cast(rotation);
    }

    if (node.scale.size() == 3) {
        transform = glm::scale(transform, glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])
        ));
    }

    return transform;
}

void processMeshBounds(const tinygltf::Model& model, int nodeIndex,
                       const glm::mat4& parentTransform,
                       glm::dvec3& minPt, glm::dvec3& maxPt) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
        return;
    }

    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 worldTransform = parentTransform * getNodeTransform(node);

    if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];

        for (const auto& primitive : mesh.primitives) {
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end()) continue;

            const tinygltf::Accessor& accessor = model.accessors[posIt->second];
            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            const float* data = reinterpret_cast<const float*>(
                &buffer.data[bufferView.byteOffset + accessor.byteOffset]);

            size_t stride = accessor.ByteStride(bufferView) / sizeof(float);
            if (stride == 0) stride = 3;

            for (size_t i = 0; i < accessor.count; ++i) {
                size_t idx = i * stride;
                glm::vec4 pos = worldTransform * glm::vec4(data[idx], data[idx+1], data[idx+2], 1.0f);
                // Convert Y-up to Z-up
                glm::dvec3 converted(pos.x, pos.z, pos.y);
                minPt = glm::min(minPt, converted);
                maxPt = glm::max(maxPt, converted);
            }
        }
    }

    for (int childIndex : node.children) {
        processMeshBounds(model, childIndex, worldTransform, minPt, maxPt);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-glb>\n";
        return 1;
    }

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool success = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]);

    if (!success) {
        std::cerr << "Failed to load: " << err << "\n";
        return 1;
    }

    // Detect large scale
    float detectedScale = 1.0f;
    for (const auto& node : model.nodes) {
        if (node.scale.size() == 3) {
            float sx = static_cast<float>(node.scale[0]);
            float sy = static_cast<float>(node.scale[1]);
            float sz = static_cast<float>(node.scale[2]);
            if (std::abs(sx - sy) < 0.01f && std::abs(sy - sz) < 0.01f && sx > 10.0f) {
                if (detectedScale == 1.0f || sx < detectedScale) {
                    detectedScale = sx;
                }
            }
        }
    }

    glm::dvec3 minPt(std::numeric_limits<double>::max());
    glm::dvec3 maxPt(std::numeric_limits<double>::lowest());

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (!model.scenes.empty() && sceneIndex < static_cast<int>(model.scenes.size())) {
        const tinygltf::Scene& scene = model.scenes[sceneIndex];
        for (int nodeIndex : scene.nodes) {
            glm::mat4 identity(1.0f);
            processMeshBounds(model, nodeIndex, identity, minPt, maxPt);
        }
    }

    // Apply scale compensation
    if (detectedScale > 10.0f) {
        float invScale = 1.0f / detectedScale;
        minPt *= invScale;
        maxPt *= invScale;
    }

    glm::dvec3 size = maxPt - minPt;

    std::cout << "\n=== Model Dimensions ===\n";
    std::cout << "Model: " << argv[1] << "\n\n";

    std::cout << "Bounding Box:\n";
    std::cout << "  Min: (" << minPt.x << ", " << minPt.y << ", " << minPt.z << ")\n";
    std::cout << "  Max: (" << maxPt.x << ", " << maxPt.y << ", " << maxPt.z << ")\n";
    std::cout << "\nDimensions:\n";
    std::cout << "  Width  (X): " << size.x << " units\n";
    std::cout << "  Depth  (Y): " << size.y << " units\n";
    std::cout << "  Height (Z): " << size.z << " units\n";

    std::cout << "\nAssuming units are meters:\n";
    std::cout << "  Height: " << size.z << " m (" << (size.z * 100) << " cm)\n";

    if (detectedScale > 10.0f) {
        std::cout << "\nNote: Scale compensation of " << detectedScale
                  << "x was applied\n";
    }

    return 0;
}
