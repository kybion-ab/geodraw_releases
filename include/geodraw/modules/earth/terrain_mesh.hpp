/*******************************************************************************
 * File: terrain_mesh.hpp
 *
 * Description: Data structures for parsed Cesium Quantized Mesh terrain data.
 * Contains header information, decoded vertex data, triangle indices, and
 * edge indices for skirt generation.
 *
 * Cesium Quantized Mesh Format:
 * https://github.com/CesiumGS/quantized-mesh
 *
 *
 ******************************************************************************/

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace geodraw {
namespace earth {

/**
 * Parsed terrain mesh data from Cesium Quantized Mesh format
 *
 * The quantized mesh format stores vertices as normalized uint16 values
 * (0-32767) relative to tile bounds. After parsing, vertices are converted
 * to geographic coordinates with height.
 *
 * Triangle indices use counter-clockwise winding for front-facing polygons.
 * Edge indices identify vertices on tile edges for skirt generation.
 */
struct TerrainMeshData {
    // =========================================================================
    // Header Data (from 88-byte header)
    // =========================================================================

    /**
     * Bounding sphere center in ECEF coordinates (meters)
     * Used for horizon occlusion testing
     */
    glm::dvec3 boundingCenter{0.0};

    /**
     * Minimum vertex height in meters above ellipsoid
     */
    float minHeight = 0.0f;

    /**
     * Maximum vertex height in meters above ellipsoid
     */
    float maxHeight = 0.0f;

    /**
     * Bounding sphere radius in meters
     */
    double boundingSphereRadius = 0.0;

    /**
     * Horizon occlusion point in ECEF coordinates
     * Used for early culling when tile is behind horizon
     */
    glm::dvec3 horizonOcclusionPoint{0.0};

    // =========================================================================
    // Vertex Data
    // =========================================================================

    /**
     * Decoded vertex with geographic position and height
     */
    struct Vertex {
        double lon;     // Longitude in degrees
        double lat;     // Latitude in degrees
        double height;  // Height in meters above ellipsoid
    };

    /**
     * All decoded vertices
     * Positions are interpolated within tile bounds based on u,v values
     */
    std::vector<Vertex> vertices;

    // =========================================================================
    // Index Data
    // =========================================================================

    /**
     * Triangle indices (decoded from high-water-mark encoding)
     * Counter-clockwise winding order
     * Size is always a multiple of 3
     */
    std::vector<uint32_t> indices;

    // =========================================================================
    // Edge Indices (for skirt generation)
    // =========================================================================

    /**
     * Vertex indices along the west edge (longitude = tile west bound)
     * Ordered from south to north
     */
    std::vector<uint32_t> westEdge;

    /**
     * Vertex indices along the south edge (latitude = tile south bound)
     * Ordered from west to east
     */
    std::vector<uint32_t> southEdge;

    /**
     * Vertex indices along the east edge (longitude = tile east bound)
     * Ordered from south to north
     */
    std::vector<uint32_t> eastEdge;

    /**
     * Vertex indices along the north edge (latitude = tile north bound)
     * Ordered from west to east
     */
    std::vector<uint32_t> northEdge;

    // =========================================================================
    // Status
    // =========================================================================

    /**
     * Whether parsing succeeded
     */
    bool valid = false;

    /**
     * Error message if parsing failed
     */
    std::string errorMessage;

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * Check if terrain data is valid and ready to use
     */
    bool isValid() const {
        return valid && !vertices.empty() && !indices.empty();
    }

    /**
     * Get number of triangles
     */
    size_t triangleCount() const {
        return indices.size() / 3;
    }

    /**
     * Get number of vertices
     */
    size_t vertexCount() const {
        return vertices.size();
    }

    /**
     * Clear all data
     */
    void clear() {
        boundingCenter = glm::dvec3(0.0);
        minHeight = 0.0f;
        maxHeight = 0.0f;
        boundingSphereRadius = 0.0;
        horizonOcclusionPoint = glm::dvec3(0.0);
        vertices.clear();
        indices.clear();
        westEdge.clear();
        southEdge.clear();
        eastEdge.clear();
        northEdge.clear();
        valid = false;
        errorMessage.clear();
    }
};

} // namespace earth
} // namespace geodraw
