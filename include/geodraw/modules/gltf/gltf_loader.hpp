/*******************************************************************************
 * File: gltf_loader.hpp
 *
 * Description: Header-only module for loading glTF 2.0 binary (.glb) files
 * and converting them to GeoDraw's Mesh3 format. Handles embedded textures,
 * multiple meshes, and coordinate system transformation.
 *
 * Why required: Enables loading and rendering of standard 3D model files in
 * glTF format, widely used in web and real-time 3D applications.
 *
 *
 *******************************************************************************/

#pragma once

// Use nlohmann/json from system instead of tinygltf's bundled version
#include <nlohmann/json.hpp>

// Use STB libraries from GeoDraw's external directory
#include "geodraw/external/stb/stb_image.h"
#include "geodraw/external/stb/stb_image_write.h"

#include "geodraw/geometry/geometry.hpp"
#include "geodraw/renderer/irenderer.hpp"
#include "geodraw/external/tinygltf/tiny_gltf.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <limits>

namespace geodraw {
namespace gltf {

/**
 * LoadedModel - Container for all meshes and textures from a glTF file
 *
 * Represents a complete loaded 3D model with all its meshes, materials,
 * and associated OpenGL texture handles for rendering.
 */
enum class ModelType {
    UNKNOWN,
    VEHICLE,
    CYCLIST,
    PEDESTRIAN
};

struct LoadedModel {
    std::vector<Mesh3> meshes;              // Converted mesh geometry
    std::vector<GLuint> textureHandles;     // OpenGL texture handles
    std::vector<glm::vec3> colors;          // Material base colors (RGB)
    std::vector<std::string> texturePaths;  // Paths for cleanup tracking
    BBox3 bounds;                            // Computed bounding box
    std::string name;                        // Model filename
    float detectedScale = 1.0f;             // Root scale factor detected (e.g. 100.0 for cm->m)
    ModelType modelType = ModelType::UNKNOWN; // Detected model type
    float realisticScale = 1.0f;            // Scale applied for realistic dimensions

    bool isValid() const { return !meshes.empty(); }
};

/**
 * Detect model type based on filename and dimensions
 *
 * Uses heuristics to identify vehicles, cyclists, or pedestrians.
 *
 * @param filename Model filename
 * @param dimensions Model dimensions (width, depth, height)
 * @return Detected model type
 */
inline ModelType detectModelType(const std::string& filename, const glm::dvec3& dimensions) {
    // Convert to lowercase for case-insensitive matching
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Check filename for keywords
    if (lower.find("car") != std::string::npos ||
        lower.find("vehicle") != std::string::npos ||
        lower.find("truck") != std::string::npos ||
        lower.find("van") != std::string::npos) {
        return ModelType::VEHICLE;
    }

    if (lower.find("cyclist") != std::string::npos ||
        lower.find("bike") != std::string::npos ||
        lower.find("bicycle") != std::string::npos) {
        return ModelType::CYCLIST;
    }

    if (lower.find("pedestrian") != std::string::npos ||
        lower.find("person") != std::string::npos ||
        lower.find("human") != std::string::npos ||
        lower.find("man") != std::string::npos ||
        lower.find("woman") != std::string::npos) {
        return ModelType::PEDESTRIAN;
    }

    // Fallback: Use dimension heuristics
    double width = dimensions.x;
    double depth = dimensions.y;
    double height = dimensions.z;

    // Vehicles: wide, long, medium height ratio
    double aspectRatio = std::max(width, depth) / height;
    if (aspectRatio > 1.2 && height > 1.2 && height < 3.0) {
        return ModelType::VEHICLE;
    }

    // Cyclist: tall with bike, medium width
    if (height > 1.8 && height < 2.5 && width > 0.4 && width < 3.0) {
        return ModelType::CYCLIST;
    }

    // Pedestrian: human proportions
    if (height > 1.5 && height < 2.2 && width < 1.0) {
        return ModelType::PEDESTRIAN;
    }

    return ModelType::UNKNOWN;
}

/**
 * Get realistic scale factor for model type
 *
 * Returns scale multiplier to achieve real-world dimensions.
 * Based on measured dimensions vs. expected real-world sizes.
 *
 * @param modelType Detected model type
 * @param currentHeight Current model height in meters
 * @return Scale factor to apply
 */
inline float getRealisticScaleFactor(ModelType modelType, double currentHeight) {
    switch (modelType) {
        case ModelType::VEHICLE:
            // Sedans are typically 1.4-1.6m tall, models measure ~2.03m
            return 1.5f / currentHeight;

        case ModelType::CYCLIST:
            // Person crouched on bike ~1.5m, models measure ~2.01m
            return 1.5f / currentHeight;

        case ModelType::PEDESTRIAN:
            // Average adult height 1.75m, models measure ~1.86m
            return 1.75f / currentHeight;

        case ModelType::UNKNOWN:
        default:
            return 1.0f;  // No adjustment
    }
}

/**
 * Convert glTF coordinate system (Y-up) to GeoDraw (Z-up)
 *
 * Transforms positions by swapping Y and Z axes.
 * glTF: X-right, Y-up, Z-backward
 * GeoDraw: X-right, Y-forward, Z-up
 */
inline Pos3 transformCoordinates(float x, float y, float z) {
    return Pos3(x, z, y);  // Swap Y and Z
}

/**
 * Extract vertex positions from glTF accessor
 *
 * Reads position data from a glTF accessor and converts to Pos3 array.
 * Handles coordinate system transformation.
 *
 * @param model The glTF model
 * @param accessor The accessor containing position data
 * @return Vector of positions in GeoDraw coordinate system
 */
inline std::vector<Pos3> extractPositions(const tinygltf::Model& model,
                                          const tinygltf::Accessor& accessor) {
    std::vector<Pos3> positions;

    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    const float* data = reinterpret_cast<const float*>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset]);

    size_t stride = accessor.ByteStride(bufferView) / sizeof(float);
    if (stride == 0) stride = 3;  // Default for VEC3

    for (size_t i = 0; i < accessor.count; ++i) {
        size_t idx = i * stride;
        positions.push_back(transformCoordinates(data[idx], data[idx + 1], data[idx + 2]));
    }

    return positions;
}

/**
 * Extract UV texture coordinates from glTF accessor
 *
 * Reads UV coordinates from a glTF accessor.
 * Note: glTF UV origin is bottom-left, OpenGL is bottom-left (same).
 *
 * @param model The glTF model
 * @param accessor The accessor containing UV data
 * @return Vector of UV coordinates
 */
inline std::vector<glm::vec2> extractUVs(const tinygltf::Model& model,
                                         const tinygltf::Accessor& accessor) {
    std::vector<glm::vec2> uvs;

    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    const float* data = reinterpret_cast<const float*>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset]);

    size_t stride = accessor.ByteStride(bufferView) / sizeof(float);
    if (stride == 0) stride = 2;  // Default for VEC2

    for (size_t i = 0; i < accessor.count; ++i) {
        size_t idx = i * stride;
        uvs.push_back(glm::vec2(data[idx], data[idx + 1]));
    }

    return uvs;
}

/**
 * Extract triangle indices from glTF accessor
 *
 * Reads index data and converts to unsigned int array.
 * Handles different index component types (UNSIGNED_SHORT, UNSIGNED_INT).
 *
 * @param model The glTF model
 * @param accessor The accessor containing index data
 * @return Vector of triangle indices
 */
inline std::vector<unsigned int> extractIndices(const tinygltf::Model& model,
                                                 const tinygltf::Accessor& accessor) {
    std::vector<unsigned int> indices;

    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    const unsigned char* data = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(data);
        for (size_t i = 0; i < accessor.count; ++i) {
            indices.push_back(static_cast<unsigned int>(indices16[i]));
        }
    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(data);
        for (size_t i = 0; i < accessor.count; ++i) {
            indices.push_back(indices32[i]);
        }
    }

    return indices;
}

/**
 * Compute 4x4 transformation matrix from glTF node
 *
 * glTF nodes can specify transforms in two ways:
 * 1. As a 4x4 matrix directly
 * 2. As separate translation, rotation (quaternion), scale (TRS)
 *
 * @param node The glTF node
 * @return 4x4 transformation matrix
 */
inline glm::mat4 getNodeTransform(const tinygltf::Node& node) {
    glm::mat4 transform(1.0f);

    // If matrix is specified, use it directly
    if (node.matrix.size() == 16) {
        // glTF matrices are column-major (same as GLM)
        for (int i = 0; i < 16; i++) {
            transform[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
        return transform;
    }

    // Otherwise, build from TRS (Translation, Rotation, Scale)

    // Translation
    if (node.translation.size() == 3) {
        transform = glm::translate(transform, glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])
        ));
    }

    // Rotation (quaternion: x, y, z, w)
    if (node.rotation.size() == 4) {
        glm::quat rotation(
            static_cast<float>(node.rotation[3]),  // w
            static_cast<float>(node.rotation[0]),  // x
            static_cast<float>(node.rotation[1]),  // y
            static_cast<float>(node.rotation[2])   // z
        );
        transform = transform * glm::mat4_cast(rotation);
    }

    // Scale
    if (node.scale.size() == 3) {
        transform = glm::scale(transform, glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])
        ));
    }

    return transform;
}

/**
 * Compute world transformation for a node by accumulating parent transforms
 *
 * Recursively multiplies parent transforms from root to leaf.
 *
 * @param model The glTF model
 * @param nodeIndex Index of node to compute transform for
 * @param parentTransform Accumulated parent transformation
 * @return World transformation matrix for this node
 */
inline glm::mat4 computeNodeWorldTransform(const tinygltf::Model& model,
                                            int nodeIndex,
                                            const glm::mat4& parentTransform = glm::mat4(1.0f)) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
        return parentTransform;
    }

    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 localTransform = getNodeTransform(node);
    glm::mat4 worldTransform = parentTransform * localTransform;

    return worldTransform;
}

/**
 * Apply transformation matrix to mesh vertices
 *
 * Transforms all vertices in a Mesh3 by a 4x4 matrix, then applies
 * coordinate system conversion (Y-up to Z-up).
 *
 * @param mesh The mesh to transform (modified in place)
 * @param transform The transformation matrix
 */
inline void transformMesh(Mesh3& mesh, const glm::mat4& transform) {
    for (auto& vertex : mesh.vertices) {
        glm::vec4 pos = transform * glm::vec4(vertex.pos, 1.0f);
        // Apply coordinate system conversion: Y-up → Z-up
        vertex.pos = glm::dvec3(pos.x, pos.z, pos.y);
    }
}

/**
 * Load texture from glTF image and upload to GPU
 *
 * Creates a temporary PNG file from the glTF embedded image data and
 * uses Renderer::loadTexture() to upload to OpenGL.
 *
 * @param model The glTF model
 * @param textureIndex Index of texture in model.textures
 * @param renderer Renderer for texture loading
 * @param tempPath Temporary file path for image export
 * @return OpenGL texture handle (0 on failure)
 */
inline GLuint loadTextureFromGLTF(const tinygltf::Model& model,
                                   int textureIndex,
                                   IRenderer& renderer,
                                   const std::string& tempPath) {
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size())) {
        return 0;
    }

    const tinygltf::Texture& texture = model.textures[textureIndex];
    if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size())) {
        return 0;
    }

    const tinygltf::Image& image = model.images[texture.source];

    // Write image to temporary file
    // tinygltf has already decoded the image into image.image vector
    if (image.image.empty()) {
        std::cerr << "Warning: Empty image data for texture " << textureIndex << "\n";
        return 0;
    }

    // Use stb_image_write to save to temp file
    std::string tempFile = tempPath + "_tex" + std::to_string(textureIndex) + ".png";

    // Determine format
    int channels = image.component;
    if (channels != 3 && channels != 4) {
        std::cerr << "Warning: Unsupported image format (channels=" << channels << ")\n";
        return 0;
    }

    // Write PNG file
    if (!stbi_write_png(tempFile.c_str(), image.width, image.height,
                        channels, image.image.data(), image.width * channels)) {
        std::cerr << "Warning: Failed to write temporary texture file\n";
        return 0;
    }

    // Load via renderer
    GLuint handle = renderer.loadTexture(tempFile);

    // Note: We keep the temp file because renderer caches by path
    // The cleanup function will handle texture unloading

    return handle;
}

/**
 * Process a glTF node recursively, extracting meshes with transforms
 *
 * Traverses scene graph depth-first, accumulating transforms and
 * extracting mesh geometry.
 *
 * @param model The glTF model
 * @param nodeIndex Index of node to process
 * @param parentTransform Accumulated parent transformation
 * @param result LoadedModel to populate
 * @param renderer Renderer for texture loading
 * @param tempPrefix Prefix for temporary texture files
 * @param minPt Running bounding box minimum
 * @param maxPt Running bounding box maximum
 * @param hasBounds Whether any bounds have been computed
 */
inline void processNode(const tinygltf::Model& model,
                        int nodeIndex,
                        const glm::mat4& parentTransform,
                        LoadedModel& result,
                        IRenderer& renderer,
                        const std::string& tempPrefix,
                        glm::dvec3& minPt,
                        glm::dvec3& maxPt,
                        bool& hasBounds) {

    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
        return;
    }

    const tinygltf::Node& node = model.nodes[nodeIndex];

    // Compute world transform for this node
    glm::mat4 worldTransform = computeNodeWorldTransform(model, nodeIndex, parentTransform);

    // Process mesh if this node has one
    if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];

        for (const auto& primitive : mesh.primitives) {
            // Only handle triangle primitives
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                continue;
            }

            Mesh3 mesh3;

            // Extract positions (required)
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end()) {
                continue;
            }

            const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
            // Extract positions WITHOUT coordinate transform (we'll apply it after node transform)
            const tinygltf::BufferView& bufferView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            const float* data = reinterpret_cast<const float*>(
                &buffer.data[bufferView.byteOffset + posAccessor.byteOffset]);

            size_t stride = posAccessor.ByteStride(bufferView) / sizeof(float);
            if (stride == 0) stride = 3;

            for (size_t i = 0; i < posAccessor.count; ++i) {
                size_t idx = i * stride;
                // Keep in glTF coordinate system for now
                mesh3.vertices.push_back(Pos3(data[idx], data[idx + 1], data[idx + 2]));
            }

            if (mesh3.vertices.empty()) {
                continue;
            }

            // Extract UVs (optional)
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (uvIt != primitive.attributes.end()) {
                const tinygltf::Accessor& uvAccessor = model.accessors[uvIt->second];
                mesh3.uvs = extractUVs(model, uvAccessor);

                if (mesh3.uvs.size() != mesh3.vertices.size()) {
                    mesh3.uvs.clear();
                }
            }

            // Extract indices (required)
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                mesh3.indices = extractIndices(model, indexAccessor);
            } else {
                continue;
            }

            // Apply world transform to mesh vertices
            transformMesh(mesh3, worldTransform);

            // Update bounding box
            for (const auto& v : mesh3.vertices) {
                minPt = glm::min(minPt, v.pos);
                maxPt = glm::max(maxPt, v.pos);
                hasBounds = true;
            }

            // Extract material properties
            GLuint textureHandle = 0;
            glm::vec3 baseColor(1.0f, 1.0f, 1.0f);

            if (primitive.material >= 0 &&
                primitive.material < static_cast<int>(model.materials.size())) {

                const tinygltf::Material& material = model.materials[primitive.material];

                // Extract base color factor
                const auto& colorFactor = material.pbrMetallicRoughness.baseColorFactor;
                if (colorFactor.size() >= 3) {
                    baseColor = glm::vec3(
                        static_cast<float>(colorFactor[0]),
                        static_cast<float>(colorFactor[1]),
                        static_cast<float>(colorFactor[2])
                    );
                }

                // Check for base color texture
                if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                    int texIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                    textureHandle = loadTextureFromGLTF(model, texIndex, renderer, tempPrefix);

                    if (textureHandle != 0) {
                        std::string texPath = tempPrefix + "_tex" + std::to_string(texIndex) + ".png";
                        result.texturePaths.push_back(texPath);
                    }
                }
            }

            result.meshes.push_back(mesh3);
            result.textureHandles.push_back(textureHandle);
            result.colors.push_back(baseColor);
        }
    }

    // Recursively process children
    for (int childIndex : node.children) {
        processNode(model, childIndex, worldTransform, result, renderer,
                   tempPrefix, minPt, maxPt, hasBounds);
    }
}

/**
 * Load glTF 2.0 binary (.glb) file and convert to GeoDraw format
 *
 * Main loading function that:
 * 1. Parses .glb binary file using tinygltf
 * 2. Converts all mesh primitives to Mesh3 format
 * 3. Loads embedded textures to GPU
 * 4. Computes overall bounding box
 * 5. Returns LoadedModel with all data
 *
 * Coordinate system transformation:
 * - glTF uses Y-up right-handed coordinates
 * - GeoDraw uses Z-up right-handed coordinates
 * - Positions are transformed by swapping Y and Z
 *
 * Material support (Phase 1):
 * - Extracts base color texture from PBR materials
 * - Ignores metallic/roughness factors
 * - Falls back to solid color if no texture
 *
 * @param path Path to .glb file
 * @param renderer Renderer for texture loading
 * @return LoadedModel with meshes and textures, or std::nullopt on failure
 */
inline std::optional<LoadedModel> loadGLB(const std::string& path,
                                           IRenderer& renderer) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // Load binary glTF file
    bool success = loader.LoadBinaryFromFile(&model, &err, &warn, path);

    if (!warn.empty()) {
        std::cout << "glTF Warning: " << warn << "\n";
    }

    if (!err.empty()) {
        std::cerr << "glTF Error: " << err << "\n";
        return std::nullopt;
    }

    if (!success) {
        std::cerr << "Failed to load glTF file: " << path << "\n";
        return std::nullopt;
    }

    LoadedModel result;
    result.name = path.substr(path.find_last_of("/\\") + 1);

    // Extract base path for temporary texture files
    std::string basePath = path.substr(0, path.find_last_of("/\\"));
    std::string tempPrefix = basePath + "/.tmp_" + result.name;

    // Initialize bounding box
    glm::dvec3 minPt(std::numeric_limits<double>::max());
    glm::dvec3 maxPt(std::numeric_limits<double>::lowest());
    bool hasBounds = false;

    // Detect uniform large scale in any node (common export issue: cm vs m units)
    float detectedRootScale = 1.0f;

    // Scan all nodes for large uniform scales
    for (const auto& node : model.nodes) {
        if (node.scale.size() == 3) {
            float sx = static_cast<float>(node.scale[0]);
            float sy = static_cast<float>(node.scale[1]);
            float sz = static_cast<float>(node.scale[2]);
            // Check if scale is uniform and large (>10x suggests unit conversion issue)
            if (std::abs(sx - sy) < 0.01f && std::abs(sy - sz) < 0.01f && sx > 10.0f) {
                if (detectedRootScale == 1.0f || sx < detectedRootScale) {
                    detectedRootScale = sx;  // Use the smallest large scale found
                }
            }
        }
    }

    if (detectedRootScale > 10.0f) {
        std::cout << "Detected large uniform scale: " << detectedRootScale
                  << "x (likely unit conversion issue)\n";
        std::cout << "Will compensate by dividing model by " << detectedRootScale << "\n";
    }

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (!model.scenes.empty() && sceneIndex < static_cast<int>(model.scenes.size())) {
        const tinygltf::Scene& scene = model.scenes[sceneIndex];

        // Process all root nodes in the scene
        for (int nodeIndex : scene.nodes) {
            glm::mat4 identity(1.0f);
            processNode(model, nodeIndex, identity, result, renderer,
                       tempPrefix, minPt, maxPt, hasBounds);
        }
    } else {
        std::cerr << "Warning: No valid scene found in glTF file\n";
    }

    // Apply scale compensation if detected
    if (detectedRootScale > 10.0f) {
        float invScale = 1.0f / detectedRootScale;
        for (auto& mesh : result.meshes) {
            for (auto& vertex : mesh.vertices) {
                vertex.pos *= invScale;
            }
        }
        // Update bounding box
        minPt *= invScale;
        maxPt *= invScale;
        result.detectedScale = detectedRootScale;
    }

    if (result.meshes.empty()) {
        std::cerr << "Warning: No valid meshes found in glTF file\n";
        return std::nullopt;
    }

    // Set bounding box
    if (hasBounds) {
        result.bounds = BBox3{Pos3{minPt.x, minPt.y, minPt.z},
                              Pos3{maxPt.x, maxPt.y, maxPt.z}};
    }

    // Detect model type and apply realistic scaling
    glm::dvec3 dimensions = maxPt - minPt;
    result.modelType = detectModelType(result.name, dimensions);

    if (result.modelType != ModelType::UNKNOWN) {
        result.realisticScale = getRealisticScaleFactor(result.modelType, dimensions.z);

        // Apply realistic scaling
        for (auto& mesh : result.meshes) {
            for (auto& vertex : mesh.vertices) {
                vertex.pos *= result.realisticScale;
            }
        }

        // Update bounding box
        result.bounds.min.pos *= result.realisticScale;
        result.bounds.max.pos *= result.realisticScale;

        const char* typeNames[] = {"UNKNOWN", "VEHICLE", "CYCLIST", "PEDESTRIAN"};
        std::cout << "Detected model type: " << typeNames[static_cast<int>(result.modelType)] << "\n";
        std::cout << "Applying realistic scale: " << result.realisticScale
                  << "x (height: " << dimensions.z << "m -> "
                  << (dimensions.z * result.realisticScale) << "m)\n";
    }

    std::cout << "Loaded glTF model: " << result.name << "\n";
    std::cout << "  Meshes: " << result.meshes.size() << "\n";
    std::cout << "  Textures: " << result.textureHandles.size() << "\n";
    std::cout << "  Vertices: ";
    size_t totalVerts = 0;
    for (const auto& m : result.meshes) totalVerts += m.vertices.size();
    std::cout << totalVerts << "\n";

    return result;
}

/**
 * Cleanup loaded model resources
 *
 * Unloads all textures from GPU and removes temporary texture files.
 *
 * @param model The model to cleanup
 * @param renderer Renderer for texture unloading
 */
inline void unloadModel(LoadedModel& model, IRenderer& renderer) {
    // Unload textures
    for (const auto& path : model.texturePaths) {
        renderer.unloadTexture(path);

        // Remove temporary texture file
        std::remove(path.c_str());
    }

    model.texturePaths.clear();
    model.textureHandles.clear();
    model.meshes.clear();
}

} // namespace gltf
} // namespace geodraw
