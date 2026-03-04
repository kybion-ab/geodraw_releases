/*******************************************************************************
 * File: maptiler_provider.hpp
 *
 * Description: MapTiler implementation of the TileProvider interface.
 * Provides access to MapTiler's tile APIs including satellite, streets,
 * hybrid, and terrain layers.
 *
 * MapTiler API Documentation:
 * https://docs.maptiler.com/cloud/api/maps/
 *
 * Rate Limits (Free Tier):
 * - Requests per second: ~8 sustained, bursts to ~12
 * - Concurrent requests: 6 recommended
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/modules/earth/tile_provider.hpp"
#include <string>
#include <vector>

namespace geodraw {
namespace earth {

/**
 * MapTiler tile provider implementation
 *
 * Provides access to MapTiler's Cloud tile APIs:
 * - satellite: High-resolution satellite/aerial imagery (JPG)
 * - streets: Vector tile map data (PBF, OpenMapTiles schema)
 * - hybrid: Satellite with road/label overlay (JPG)
 * - terrain: RGB-encoded elevation data (PNG)
 *
 * Usage:
 *   auto provider = std::make_shared<MapTilerProvider>("YOUR_API_KEY");
 *   EarthLayerConfig config;
 *   config.provider = provider;
 *   config.layerId = "satellite";
 *
 * API Key:
 *   Get a free API key at https://cloud.maptiler.com/
 *   Free tier includes generous limits for development.
 *
 * Attribution:
 *   MapTiler requires attribution: "MapTiler" and "OpenStreetMap contributors"
 *   The EarthLayer class includes this attribution automatically.
 */
class MapTilerProvider : public TileProvider {
public:
    /**
     * Create a MapTiler provider with API key
     *
     * @param apiKey Your MapTiler Cloud API key
     */
    explicit MapTilerProvider(std::string apiKey)
        : apiKey_(std::move(apiKey))
    {}

    /**
     * Get provider name
     */
    std::string name() const override {
        return "MapTiler";
    }

    /**
     * Get available tile layers
     *
     * MapTiler layers:
     * - satellite: Global satellite imagery at up to zoom 20
     * - streets: Vector tiles in OpenMapTiles schema, zoom 0-14
     * - hybrid: Satellite with roads/labels, zoom 0-19
     * - terrain: RGB-encoded DEM for terrain visualization, zoom 0-12
     */
    std::vector<TileLayerConfig> availableLayers() const override {
        return {
            {"satellite", "Satellite Imagery", "jpg", 256, 0, 20},
            {"streets", "Streets (Vector)", "pbf", 512, 0, 14},
            {"hybrid", "Hybrid (Satellite + Labels)", "jpg", 256, 0, 19},
            {"terrain", "Terrain RGB", "png", 256, 0, 12},
            {"terrain-mesh", "Terrain 3D", "terrain", 256, 0, 13}
        };
    }

    /**
     * Build URL for a MapTiler tile request
     *
     * URL format varies by layer:
     * - satellite: https://api.maptiler.com/maps/satellite/256/{z}/{x}/{y}.jpg?key=...
     * - streets:   https://api.maptiler.com/tiles/v3/{z}/{x}/{y}.pbf?key=...
     * - hybrid:    https://api.maptiler.com/maps/hybrid/256/{z}/{x}/{y}.jpg?key=...
     * - terrain:   https://api.maptiler.com/tiles/terrain-rgb/{z}/{x}/{y}.png?key=...
     */
    std::string buildTileUrl(const TileCoord& coord,
                              const std::string& layerId) const override {
        std::string base;
        std::string ext;

        if (layerId == "satellite") {
            base = "https://api.maptiler.com/maps/satellite/256/";
            ext = "jpg";
        } else if (layerId == "streets") {
            base = "https://api.maptiler.com/tiles/v3/";
            ext = "pbf";
        } else if (layerId == "hybrid") {
            base = "https://api.maptiler.com/maps/hybrid/256/";
            ext = "jpg";
        } else if (layerId == "terrain") {
            base = "https://api.maptiler.com/tiles/terrain-rgb/";
            ext = "png";
        } else if (layerId == "terrain-mesh") {
            // Quantized mesh terrain uses TMS Y-flip
            int tmsY = (1 << coord.z) - 1 - coord.y;
            return "https://api.maptiler.com/tiles/terrain-quantized-mesh-v2/" +
                   std::to_string(coord.z) + "/" +
                   std::to_string(coord.x) + "/" +
                   std::to_string(tmsY) + ".terrain?key=" + apiKey_;
        } else {
            // Default to satellite for unknown layer IDs
            base = "https://api.maptiler.com/maps/satellite/256/";
            ext = "jpg";
        }

        // Standard XYZ URL format: /{z}/{x}/{y}.{ext}?key={apiKey}
        return base +
               std::to_string(coord.z) + "/" +
               std::to_string(coord.x) + "/" +
               std::to_string(coord.y) + "." +
               ext + "?key=" + apiKey_;
    }

    /**
     * Get MapTiler rate limits
     *
     * These values are tuned for MapTiler's free tier:
     * - 8 requests/second sustained rate
     * - Burst capacity of 12 tokens
     * - 6 concurrent requests to avoid connection limits
     */
    ProviderRateLimits rateLimits() const override {
        return ProviderRateLimits::mapTilerFree();
    }

    /**
     * Get cache prefix
     *
     * Uses "maptiler" to create isolated cache directory
     */
    std::string cachePrefix() const override {
        return "maptiler";
    }

    /**
     * Get HTTP headers required for specific tile layers
     *
     * The terrain-mesh (quantized mesh) layer requires an Accept header
     * so MapTiler's CDN returns the binary .terrain format instead of
     * a JSON metadata document.
     */
    std::vector<std::pair<std::string, std::string>>
    buildTileHeaders(const std::string& layerId) const override {
        if (layerId == "terrain-mesh") {
            return {{"Accept",
                "application/vnd.quantized-mesh,application/octet-stream;q=0.9"}};
        }
        return {};
    }

    /**
     * Get the API key (for debugging/logging)
     */
    const std::string& apiKey() const {
        return apiKey_;
    }

private:
    std::string apiKey_;
};

// =============================================================================
// Layer ID Constants (for type safety)
// =============================================================================

namespace maptiler {

/**
 * MapTiler layer IDs as constants
 *
 * Usage:
 *   config.layerId = maptiler::SATELLITE;
 */
constexpr const char* SATELLITE = "satellite";
constexpr const char* STREETS = "streets";
constexpr const char* HYBRID = "hybrid";
constexpr const char* TERRAIN = "terrain";
constexpr const char* TERRAIN_MESH = "terrain-mesh";

} // namespace maptiler

} // namespace earth
} // namespace geodraw
