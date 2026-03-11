/*******************************************************************************
 * File: earth_pbf.hpp
 *
 * Description: PBF (Mapbox Vector Tile) parsing for the Earth visualization
 * module. Decodes MVT/PBF data into renderable geometry (polylines for roads,
 * triangulated meshes for buildings/water).
 *
 * Uses vtzero (header-only MVT decoder) and earcut for triangulation.
 *
 * Why required: STREETS mode fetches PBF tiles from MapTiler but needs parsing
 * logic to convert the raw protobuf data into renderable geometry.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/modules/earth/earth_coords.hpp"
#include "geodraw/modules/earth/earth_tiles.hpp"
#include "geodraw/geometry/geometry.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <stdexcept>

namespace geodraw {
namespace earth {
namespace pbf {

// =============================================================================
// Vector Layer Filter
// =============================================================================

/**
 * Specifies which MVT layers to parse from vector tiles
 *
 * MapTiler v3 vector tiles contain many layers:
 * - transportation: Roads, paths, railways
 * - building: Building footprints
 * - water: Water bodies, rivers
 * - landuse: Parks, forests, etc.
 * - landcover: Vegetation, ice, etc.
 * - place: City/town labels
 * - poi: Points of interest
 * - ... and more
 *
 * Parsing all layers is expensive. This filter allows selecting only
 * the layers needed for your use case.
 *
 * Usage:
 *   // Parse only roads and buildings (fast)
 *   VectorLayerSet layers;
 *   layers.enableTransportation();
 *   layers.enableBuilding();
 *
 *   // Parse everything (slow)
 *   VectorLayerSet layers = VectorLayerSet::all();
 *
 *   // Empty filter = ERROR (must explicitly specify)
 *   VectorLayerSet layers;  // Throws in parsePBF if used empty
 */
class VectorLayerSet {
public:
    VectorLayerSet() = default;

    // === Named constructors ===

    /**
     * Create a filter that enables ALL layers
     * Use sparingly - parsing all layers is expensive
     */
    static VectorLayerSet all() {
        VectorLayerSet set;
        set.enableTransportation();
        set.enableBuilding();
        set.enableWater();
        set.enableLanduse();
        set.enableLandcover();
        return set;
    }

    /**
     * Create a filter for common map visualization (roads + buildings)
     */
    static VectorLayerSet roadsAndBuildings() {
        VectorLayerSet set;
        set.enableTransportation();
        set.enableBuilding();
        return set;
    }

    /**
     * Create a filter for roads only
     */
    static VectorLayerSet roadsOnly() {
        VectorLayerSet set;
        set.enableTransportation();
        return set;
    }

    // === Layer enable methods ===

    VectorLayerSet& enableTransportation() { layers_.insert("transportation"); return *this; }
    VectorLayerSet& enableBuilding() { layers_.insert("building"); return *this; }
    VectorLayerSet& enableWater() { layers_.insert("water"); return *this; }
    VectorLayerSet& enableLanduse() { layers_.insert("landuse"); return *this; }
    VectorLayerSet& enableLandcover() { layers_.insert("landcover"); return *this; }

    /**
     * Enable a custom layer by name
     * Use for layers not covered by the convenience methods
     */
    VectorLayerSet& enable(const std::string& layerName) {
        layers_.insert(layerName);
        return *this;
    }

    // === Query methods ===

    /**
     * Check if a layer is enabled
     */
    bool isEnabled(const std::string& layerName) const {
        return layers_.count(layerName) > 0;
    }

    /**
     * Check if any layers are enabled
     */
    bool hasAnyEnabled() const {
        return !layers_.empty();
    }

    /**
     * Check if empty (no layers enabled)
     */
    bool empty() const {
        return layers_.empty();
    }

    /**
     * Get the set of enabled layer names
     */
    const std::unordered_set<std::string>& enabledLayers() const {
        return layers_;
    }

private:
    std::unordered_set<std::string> layers_;
};

// =============================================================================
// Road Styling
// =============================================================================

/**
 * Road classification for styling
 */
enum class RoadClass {
    MOTORWAY,       // Highways, freeways
    PRIMARY,        // Primary roads
    SECONDARY,      // Secondary roads
    TERTIARY,       // Tertiary roads
    RESIDENTIAL,    // Residential streets
    SERVICE,        // Service roads, parking lots
    PATH,           // Paths, footways, cycleways
    OTHER           // Unclassified
};

/**
 * Get road class from MVT "class" property string
 */
GEODRAW_API RoadClass roadClassFromString(const std::string& classStr);

/**
 * Get styling parameters for a road class
 */
struct RoadStyle {
    glm::vec3 color;
    float thickness;
};

GEODRAW_API RoadStyle getRoadStyle(RoadClass roadClass);

// =============================================================================
// Parsed Feature Types
// =============================================================================

/**
 * Parsed road feature
 */
struct Road {
    Polyline3 geometry;         // Road centerline in ENU coordinates (raw)
    Polyline3 elevated;         // Pre-elevated for rendering (cached)
    RoadClass roadClass;        // Road classification
    std::string name;           // Road name (if available)
    float thickness;            // Line thickness for rendering
    glm::vec3 color;            // Line color for rendering
    int oneway = 0;             // 1=oneway in direction, -1=oneway opposite, 0=bidirectional
    int lanes = 0;              // Total lane count (0 = use default for road class)
};

/**
 * Parsed building feature
 */
struct Building {
    Mesh3 mesh;                 // Triangulated polygon mesh in ENU coordinates
    Mesh3 mesh3D;               // Pre-computed 3D extrusion mesh (roof + walls)
    Polyline3 outline;          // Building outline for wireframe rendering (raw)
    Polyline3 outlineElevated;  // Pre-elevated and closed outline (cached)
    float height;               // Building height (if available)
};

/**
 * Parsed water feature
 */
struct Water {
    Mesh3 mesh;                 // Triangulated polygon mesh in ENU coordinates
};

/**
 * Container for all parsed features from a vector tile
 */
struct ParsedVectorTile {
    std::vector<Road> roads;
    std::vector<Building> buildings;
    std::vector<Water> water;
    Mesh3 buildingsMesh;        // Combined flat footprints (one mesh per tile)
    Mesh3 buildingsMesh3D;      // Combined 3D extrusions (one mesh per tile)
    bool parsed = false;        // True if parsing succeeded
    std::string errorMessage;   // Error message if parsing failed
};

// =============================================================================
// Parsing Functions
// =============================================================================

/**
 * Parse a Mapbox Vector Tile (PBF) into renderable geometry
 *
 * Decodes the PBF data using vtzero and converts features to ENU coordinates
 * relative to the given reference point.
 *
 * IMPORTANT: You must specify which layers to parse via the `layers` parameter.
 * An empty VectorLayerSet will throw std::invalid_argument.
 *
 * Available layers (MapTiler v3):
 * - "transportation": Roads, paths, railways
 * - "building": Building footprints
 * - "water": Water bodies, rivers
 * - "landuse": Parks, forests, industrial areas
 * - "landcover": Vegetation, ice, sand
 *
 * @param pbfData Raw PBF/MVT data from HTTP response
 * @param coord Tile coordinate (for coordinate conversion)
 * @param reference Geographic reference point for ENU conversion
 * @param layers Which MVT layers to parse (must not be empty)
 * @return ParsedVectorTile containing parsed features from enabled layers
 * @throws std::invalid_argument if layers is empty
 */
GEODRAW_API ParsedVectorTile parsePBF(
    const std::vector<unsigned char>& pbfData,
    const TileCoord& coord,
    const GeoReference& reference,
    const VectorLayerSet& layers,
    CoordinateOutput coordinateOutput = CoordinateOutput::ENU);

} // namespace pbf
} // namespace earth
} // namespace geodraw
