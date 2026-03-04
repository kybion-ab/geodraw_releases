/*******************************************************************************
 * File: terrain_parser.hpp
 *
 * Description: Parser for Cesium Quantized Mesh terrain format (.terrain files).
 * Handles gzip decompression, header parsing, and decoding of vertex data,
 * triangle indices, and edge indices.
 *
 * Cesium Quantized Mesh Format:
 * https://github.com/CesiumGS/quantized-mesh
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/modules/earth/terrain_mesh.hpp"
#include "geodraw/modules/earth/earth_tiles.hpp"
#include <vector>
#include <cstdint>

namespace geodraw {
namespace earth {

/**
 * Parse a Cesium Quantized Mesh terrain tile
 *
 * The terrain file format is:
 * 1. Gzip compressed binary data
 * 2. Little-endian byte order
 * 3. Structure:
 *    - Header (88 bytes): bounding sphere, min/max height, occlusion point
 *    - Vertex data: u[], v[], height[] arrays (uint16, delta+zigzag encoded)
 *    - Index data: triangle indices (high-water-mark encoded)
 *    - Edge indices: west/south/east/north edge vertex indices
 *
 * @param compressedData Gzip-compressed terrain data from HTTP response
 * @param coord Tile coordinate for computing geographic positions
 * @return Parsed terrain mesh data (check .valid for success)
 */
GEODRAW_API TerrainMeshData parseQuantizedMesh(const std::vector<uint8_t>& compressedData,
                                               const TileCoord& coord);

/**
 * Parse from uncompressed data (for testing or pre-decompressed data)
 *
 * @param data Uncompressed terrain data
 * @param coord Tile coordinate for computing geographic positions
 * @return Parsed terrain mesh data (check .valid for success)
 */
GEODRAW_API TerrainMeshData parseQuantizedMeshUncompressed(const std::vector<uint8_t>& data,
                                                            const TileCoord& coord);

/**
 * Decompress gzip data
 *
 * @param compressedData Gzip-compressed data
 * @param decompressedData Output buffer for decompressed data
 * @return true on success, false on decompression error
 */
GEODRAW_API bool decompressGzip(const std::vector<uint8_t>& compressedData,
                                std::vector<uint8_t>& decompressedData);

} // namespace earth
} // namespace geodraw
