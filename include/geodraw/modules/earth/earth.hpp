/*******************************************************************************
 * File: earth.hpp
 *
 * Description: Main public API for the Earth visualization module.
 * Provides EarthLayer class for rendering map tiles with LOD management,
 * coordinate conversion, and caching.
 *
 * Why required: Integrates coordinate conversion, tile management, HTTP
 * fetching, and caching into a single easy-to-use API for rendering Earth
 * satellite imagery and vector tiles.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/scene/scene.hpp"
#include "geodraw/camera/camera.hpp"
#include "geodraw/renderer/irenderer.hpp"
#include "geodraw/modules/earth/earth_coords.hpp"
#include "geodraw/modules/earth/earth_tiles.hpp"
#include "geodraw/modules/earth/earth_cache.hpp"
#include "geodraw/modules/earth/earth_pbf.hpp"
#include "geodraw/modules/earth/tile_rate_limiter.hpp"
#include "geodraw/modules/earth/tile_provider.hpp"
#include "geodraw/modules/earth/terrain_mesh.hpp"

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace geodraw {

// Forward declaration
class App;

namespace earth {

// Forward declarations for internal components
class CameraChangeDetector;
class TileSelector;
class TileLoadManager;
class TileGPUCache;
struct VisibleTile;

} // namespace earth

namespace earth {

// CoordinateOutput enum is defined in earth_coords.hpp

// =============================================================================
// Earth Layer Configuration
// =============================================================================

struct LayerZoomLimits {
  int minZoom;
  int maxZoom;
};

/**
 * Configuration for EarthLayer
 *
 * The provider is used to fetch tiles from a map service (e.g., MapTiler, Mapbox).
 * The layerId specifies which layer to display from the provider.
 *
 * Example usage:
 *   auto provider = std::make_shared<MapTilerProvider>("YOUR_API_KEY");
 *   EarthLayerConfig config;
 *   config.provider = provider;
 *   config.layerId = "satellite";
 */
struct EarthLayerConfig {
    std::shared_ptr<TileProvider> provider;               // Tile provider (required)
    std::string layerId;                                  // Layer ID from provider (e.g., "satellite")
    GeoReference reference;                               // Scene origin in WGS84
    CoordinateOutput coordinateOutput = CoordinateOutput::ENU; // Coordinate output mode
    LayerZoomLimits layerZoomLimits;                      // Min and max LOD zoom levels
    float lodBias = 1.0f;                                 // LOD quality bias (>1 = more detail)
    float lodThreshold = 1024.0f;                         // Screen-space error threshold (pixels)
    CacheConfig cache;                                    // Cache configuration

    // === Rate limiting configuration ===
    // If not set, uses the provider's default rate limits
    std::optional<RateLimiterConfig> rateLimiter;         // Override provider rate limits

    // === Vector tile layer filtering (for vector layers like "streets") ===
    // REQUIRED when using a vector layer. Must specify which MVT layers to parse.
    // An empty VectorLayerSet will cause an error when parsing vector tiles.
    // Examples:
    //   vectorLayers = pbf::VectorLayerSet::roadsAndBuildings();  // Common case
    //   vectorLayers = pbf::VectorLayerSet::all();                // Parse everything (slow)
    //   vectorLayers.enableTransportation().enableBuilding();     // Custom selection
    pbf::VectorLayerSet vectorLayers;
};

// =============================================================================
// Earth Layer
// =============================================================================

/**
 * EarthLayer - Main class for Earth visualization
 *
 * Manages tile loading, LOD selection, caching, and rendering of map tiles.
 * Tiles are rendered as textured quads in the local ENU coordinate system.
 *
 * Usage:
 *   EarthLayerConfig config;
 *   config.apiKey = "YOUR_MAPTILER_KEY";
 *   config.reference = GeoReference(GeoCoord(37.7749, -122.4194, 0));  // SF
 *
 *   EarthLayer earth(config, renderer);
 *
 *   // Each frame:
 *   earth.update(camera, width, height);
 *   earth.addToScene(scene);
 *
 *   // Place geometry at GPS coordinates:
 *   ENUCoord local = earth.geoToLocal(GeoCoord(37.78, -122.41, 0));
 *   scene.Add(Pos3(local.east, local.north, local.up), ...);
 */
class GEODRAW_API EarthLayer {
public:
    /**
     * Create Earth layer with configuration
     *
     * @param config Layer configuration (API key, reference point, etc.)
     * @param renderer Reference to renderer for texture upload (IRenderer for testability)
     */
    EarthLayer(const EarthLayerConfig& config, IRenderer& renderer);

    ~EarthLayer();

    // Non-copyable
    EarthLayer(const EarthLayer&) = delete;
    EarthLayer& operator=(const EarthLayer&) = delete;

    // =========================================================================
    // Core API - Call Each Frame
    // =========================================================================

    /**
     * Tick - check if scene needs update (call every frame in drawCallback)
     *
     * This is the main per-frame check for event-driven updates. It:
     * - Checks if any async tiles finished loading
     * - Checks if camera moved significantly
     * - Only if camera moved: recomputes LOD visibility
     * - Returns true only when visible tile set ACTUALLY changed
     *
     * IMPORTANT: For STREETS layer, camera rotation/zoom alone does NOT
     * require a scene rebuild if the same tiles are visible. Only:
     * - New tiles becoming visible (LOD change, pan)
     * - Tiles finishing download/parsing
     * will trigger a scene rebuild.
     *
     * Use this with requestUpdate() for efficient event-driven rendering:
     *   if (earth.tick(camera, w, h)) app.requestUpdate();
     *
     * @param camera Current camera
     * @param viewportWidth Viewport width in pixels
     * @param viewportHeight Viewport height in pixels
     * @return true if the scene needs to be rebuilt (call update())
     */
    bool tick(const Camera& camera, int viewportWidth, int viewportHeight);

    /**
     * Update tile visibility and loading based on camera
     *
     * Call this every frame before addToScene(). It:
     * - Determines visible tiles based on camera frustum
     * - Selects appropriate LOD based on screen-space error
     * - Initiates loading for missing tiles
     * - Uploads loaded tiles to GPU as textures
     *
     * @param camera Current camera (for frustum and LOD calculations)
     * @param viewportWidth Viewport width in pixels
     * @param viewportHeight Viewport height in pixels
     * @return true if any tiles are still loading
     */
    bool update(const Camera& camera, int viewportWidth, int viewportHeight);

    /**
     * Add visible tiles to scene for rendering
     *
     * Call after update(). Adds textured quads for all visible tiles.
     * Uses parent-tile fallback for tiles still loading.
     *
     * @param scene Scene to add tile geometry to
     */
    void addToScene(Scene& scene);

    // =========================================================================
    // Coordinate Conversion
    // =========================================================================

    /**
     * Convert WGS84 geodetic coordinates to local ENU
     *
     * Use this to place geometry at GPS coordinates in the scene.
     *
     * @param geo WGS84 coordinates (latitude, longitude, altitude)
     * @return Local ENU coordinates relative to reference point
     */
    ENUCoord geoToLocal(const GeoCoord& geo) const;

    /**
     * Convert local ENU coordinates to WGS84 geodetic
     *
     * Use this to get GPS coordinates of geometry in the scene.
     *
     * @param local Local ENU coordinates
     * @return WGS84 coordinates
     */
    GeoCoord localToGeo(const ENUCoord& local) const;

    /**
     * Convert local ENU to glm::vec3 for rendering
     */
    glm::vec3 geoToVec3(const GeoCoord& geo) const {
        return geoToLocal(geo).toVec3();
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * Set the active layer by ID
     *
     * @param layerId Layer identifier from the provider (e.g., "satellite", "streets")
     */
    void setLayerId(const std::string& layerId);

    /**
     * Get the current layer ID
     */
    const std::string& getLayerId() const { return config_.layerId; }

    /**
     * Get the tile provider
     */
    std::shared_ptr<TileProvider> getProvider() const { return config_.provider; }

    /**
     * Set the geographic reference point (scene origin)
     *
     * Changes the origin of the local coordinate system.
     * Clears GPU textures as tile positions will change.
     */
    void setReference(const GeoReference& ref);

    /**
     * Get the current geographic reference
     */
    const GeoReference& getReference() const { return config_.reference; }

    /**
     * Set LOD bias (quality adjustment)
     *
     * @param bias >1.0 loads more detailed tiles, <1.0 loads coarser tiles
     */
    void setLodBias(float bias) { config_.lodBias = bias; }

    /**
     * Get current LOD bias
     */
    float getLodBias() const { return config_.lodBias; }

    /**
     * Set coordinate output mode
     *
     * @param output ENU (local tangent plane) or ECEF (Earth-centered)
     */
    void setCoordinateOutput(CoordinateOutput output);

    /**
     * Get current coordinate output mode
     */
    CoordinateOutput getCoordinateOutput() const { return config_.coordinateOutput; }

    /**
     * Convert WGS84 geodetic coordinates to output coordinates
     *
     * Returns coordinates in the configured output mode:
     * - ENU: Local East-North-Up relative to reference point
     * - ECEF: Earth-Centered Earth-Fixed (global coordinates)
     *
     * When using ECEF output, call Scene::setOrigin(camera ECEF position)
     * each frame for camera-relative rendering.
     *
     * @param geo WGS84 coordinates (latitude, longitude, altitude)
     * @return Output coordinates as glm::dvec3
     */
    glm::dvec3 geoToOutputCoords(const GeoCoord& geo) const {
        const WGS84 geoRad = geo.toRadians();
        if (config_.coordinateOutput == CoordinateOutput::ECEF) {
            ECEFCoord ecef = geoToECEF(geoRad);
            return glm::dvec3(ecef.x, ecef.y, ecef.z);
        } else {
            ENUCoord enu = geoToENU(geoRad, config_.reference);
            return enu.toDVec3();
        }
    }

    /**
     * Set whether to render textures on tiles
     */
    void setShowTextures(bool show) { showTextures_ = show; }

    /**
     * Get whether textures are shown
     */
    bool getShowTextures() const { return showTextures_; }

    /**
     * Set whether to render wireframe mesh
     */
    void setShowWireframe(bool show) { showWireframe_ = show; }

    /**
     * Get whether wireframe is shown
     */
    bool getShowWireframe() const { return showWireframe_; }

    /**
     * Set whether to show tile debug info (bounding boxes + labels)
     */
    void setShowTileDebug(bool show) { showTileDebug_ = show; }

    /**
     * Get whether tile debug is shown
     */
    bool getShowTileDebug() const { return showTileDebug_; }

    /**
     * Set whether to render buildings (vector layers only)
     */
    void setShowBuildings(bool show) { showBuildings_ = show; }

    /**
     * Get whether buildings are shown
     */
    bool getShowBuildings() const { return showBuildings_; }

    /**
     * Set whether to render roads/streets (vector layers only)
     */
    void setShowRoads(bool show) { showRoads_ = show; }

    /**
     * Get whether roads are shown
     */
    bool getShowRoads() const { return showRoads_; }

    /**
     * Set whether to render buildings in 3D extruded mode
     */
    void setShow3DBuildings(bool show) { show3DBuildings_ = show; }

    /**
     * Get whether 3D building extrusion is enabled
     */
    bool getShow3DBuildings() const { return show3DBuildings_; }

    /**
     * Tile debug information for visualization
     */
    struct TileDebugInfo {
        std::string key;          // Tile key (e.g., "10/512/341")
        glm::dvec3 centerECEF;    // Center position in ECEF coordinates
        bool hasTexture;          // Whether texture is loaded
        bool usingFallback;       // Whether using parent tile fallback
    };

    /**
     * Get debug info for all visible tiles
     * Centers are in ECEF coordinates (subtract scene origin for rendering)
     */
    std::vector<TileDebugInfo> getVisibleTileDebugInfo() const;

    /**
     * Set whether to use 3D terrain when available
     *
     * When enabled, terrain mesh tiles are loaded alongside satellite imagery.
     * The terrain mesh provides 3D elevation and satellite imagery is draped on top.
     * Terrain data is available for zoom levels 0-13.
     *
     * @param enabled true to enable 3D terrain
     */
    void setTerrainEnabled(bool enabled);

    /**
     * Get whether 3D terrain is enabled
     */
    bool getTerrainEnabled() const { return terrainEnabled_; }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * Get number of tiles currently visible
     */
    int visibleTileCount() const;

    /**
     * Get number of tiles currently loading
     */
    int loadingTileCount() const;

    /**
     * Get number of tiles in memory cache
     */
    int memoryCacheTileCount() const;

    /**
     * Get number of GPU textures
     */
    int gpuTextureCount() const;

    /**
     * Check if all visible tiles are loaded
     */
    bool isFullyLoaded() const;

    /**
     * Get tile coordinates and WGS84 bounding boxes for all visible tiles
     *
     * Use this to fetch external data (e.g., from a road API) for visible areas.
     * The returned tile coordinates can be used as cache keys.
     *
     * @return Vector of TileBounds with coordinates and bounds in degrees
     */
    std::vector<TileBounds> getVisibleTileBounds() const;

    // =========================================================================
    // Debug/ImGui
    // =========================================================================

    /**
     * Draw ImGui configuration panel
     *
     * Only available when GEODRAW_HAS_IMGUI is defined.
     *
     * @param app Reference to App for registering commands
     */
#ifdef GEODRAW_HAS_IMGUI
    void drawImGuiPanel(App& app);
#endif

private:

    // =========================================================================
    // Layer Configuration Helpers
    // =========================================================================

    /**
     * Get zoom limits for the current layer from the provider
     */
    LayerZoomLimits getZoomLimitsForLayer() const {
        if (config_.provider) {
            const auto* layer = config_.provider->getLayer(config_.layerId);
            if (layer) {
                return {layer->minZoom, layer->maxZoom};
            }
        }
        // Default fallback
        return {0, 19};
    }

    int getMinZoom() const { return getZoomLimitsForLayer().minZoom; }
    int getMaxZoom() const { return getZoomLimitsForLayer().maxZoom; }

    /**
     * Check if the current layer is a vector layer (PBF format)
     */
    bool isVectorLayer() const {
        if (config_.provider) {
            const auto* layer = config_.provider->getLayer(config_.layerId);
            if (layer) {
                return layer->isVectorLayer();
            }
        }
        return false;
    }

    // =========================================================================
    // Tile Loading (internal)
    // =========================================================================

    void loadMissingTiles();
    void processCompletedRequests();

    // =========================================================================
    // Rendering
    // =========================================================================

    void renderTile(Scene& scene, const VisibleTile& tile);
    void renderTileFlat(Scene& scene, const VisibleTile& tile);
    void renderTileSubdivided(Scene& scene, const VisibleTile& tile, int subdivs);
    void renderTileWithTerrain(Scene& scene, const VisibleTile& tile);

    std::array<glm::vec3, 4> getTileCorners(const TileCoord& coord) const;
    std::array<glm::vec2, 4> getTileUVs(const TileCoord& tile,
                                         const TileCoord& textureTile) const;

    // =========================================================================
    // Internal Components (Facade Pattern)
    // =========================================================================

    std::unique_ptr<CameraChangeDetector> cameraDetector_;
    std::unique_ptr<TileSelector> tileSelector_;
    std::unique_ptr<TileLoadManager> loadManager_;
    std::unique_ptr<TileGPUCache> gpuCache_;

    // =========================================================================
    // Core State
    // =========================================================================

    EarthLayerConfig config_;
    IRenderer& renderer_;
    TileCache cache_;
    TileRequestManager requestManager_;

    // Flag to track if configuration changed (layer type, geometry mode, etc.)
    // Forces hash recomputation on next tick()
    bool configChanged_ = true;  // Start true to force initial scene build

    // Rendering options
    bool showTextures_ = true;
    bool showWireframe_ = false;
    bool showTileDebug_ = false;
    bool showBuildings_ = true;
    bool showRoads_ = true;
    bool show3DBuildings_ = false;

    // Terrain state
    bool terrainEnabled_ = false;

    // Attribution text (required by MapTiler TOS)
    static constexpr const char* ATTRIBUTION = "© MapTiler © OpenStreetMap";
};

// =============================================================================
// Convenience Factory
// =============================================================================

/**
 * Create an EarthLayer with a provider and sensible defaults
 *
 * @param provider Tile provider (e.g., MapTilerProvider)
 * @param renderer Reference to renderer
 * @param layerId Layer ID from the provider (default: provider's default layer)
 * @param origin Scene origin in WGS84 (default: San Francisco)
 * @return Configured EarthLayer
 */
inline std::unique_ptr<EarthLayer> createEarthLayer(
    std::shared_ptr<TileProvider> provider,
    IRenderer& renderer,
    const std::string& layerId = "",
    const GeoCoord& origin = GeoCoord(37.7749, -122.4194, 0.0))
{
    EarthLayerConfig config;
    config.provider = provider;
    config.layerId = layerId.empty() ? provider->defaultLayerId() : layerId;
    config.reference = GeoReference(origin);
    return std::make_unique<EarthLayer>(config, renderer);
}

// =============================================================================
// Camera ECEF Utilities
// =============================================================================

/**
 * Configure camera for orbital/globe viewing
 *
 * Sets appropriate zoom limits and clipping planes for viewing Earth
 * from ground level up to orbital distances.
 *
 * @param camera Camera to configure
 * @param minAltitude Minimum viewing altitude in meters (default 10m)
 * @param maxAltitude Maximum viewing altitude in meters (default 50,000 km)
 */
inline void configureCameraForGlobe(Camera& camera,
                                     float minAltitude = 5.0f,
                                     float maxAltitude = 50000000.0f) {
    camera.minZoomDistance = minAltitude;
    camera.maxZoomDistance = maxAltitude;
    camera.nearPlane = minAltitude * 0.1f;  // Near plane at 10% of min altitude
    camera.farPlane = maxAltitude * 2.0f;   // Far plane at 2x max altitude
}

/**
 * Set camera ECEF position from WGS84 geodetic coordinates
 *
 * Convenience function to update camera's ECEF position for use with
 * camera-relative rendering (Scene::setOrigin).
 *
 * @param camera Camera to update
 * @param geo WGS84 coordinates (latitude, longitude, altitude)
 */
inline void setCameraPositionFromGeo(Camera& camera, const GeoCoord& geo) {
    ECEFCoord ecef = geoToECEF(geo.toRadians());
    camera.setECEFPosition(glm::dvec3(ecef.x, ecef.y, ecef.z));
}

/**
 * Get camera ECEF position from current ENU position and reference
 *
 * Converts camera's local ENU position (relative to a reference point)
 * to global ECEF coordinates.
 *
 * @param camera Camera with local ENU position
 * @param ref GeoReference defining the ENU origin
 * @return ECEF position as glm::dvec3
 */
inline glm::dvec3 getCameraECEFFromENU(const Camera& camera, const GeoReference& ref) {
    // Get camera position in local coordinates
    glm::vec3 localPos = camera.position;
    if (camera.mode == Camera::CameraMode::ORBIT) {
        float cy = std::cos(camera.orbitYaw);
        float sy = std::sin(camera.orbitYaw);
        float cp = std::cos(camera.orbitPitch);
        float sp = std::sin(camera.orbitPitch);
        localPos = camera.target + glm::vec3(
            camera.distance * cy * cp,
            camera.distance * sy * cp,
            camera.distance * sp
        );
    }

    // Convert ENU position to GPS coordinates
    ENUCoord enu(localPos.x, localPos.y, localPos.z);
    WGS84 geo = enuToGeo(enu, ref);

    // Convert to ECEF
    ECEFCoord ecef = geoToECEF(geo);
    return glm::dvec3(ecef.x, ecef.y, ecef.z);
}

} // namespace earth
} // namespace geodraw
