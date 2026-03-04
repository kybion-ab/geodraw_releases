/*******************************************************************************
 * File: marker_clusterer.hpp
 *
 * Description: Screen-space marker clustering for scenario pins.
 * Clusters nearby markers when zoomed out to continent scale, showing count
 * badges. Clicking a cluster zooms in to reveal individual pins.
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include <glm/glm.hpp>
#include <optional>
#include <vector>

namespace geodraw::clustering {

/**
 * Configuration for marker clustering behavior
 */
struct ClusterConfig {
    float pinScreenRadius = 12.0f;          // Rendered radius of each pin in pixels
    // Pins cluster when centers are < 2*pinScreenRadius apart (i.e., when circles intersect)
};

/**
 * Represents a cluster of one or more markers
 */
struct Cluster {
    std::vector<size_t> pinIndices;     // Indices into original pin array
    glm::vec2 screenCenter;             // Average screen position
    glm::dvec3 ecefCenter;              // Geographic center for camera navigation
    float screenRadius;                 // Computed from count

    size_t count() const { return pinIndices.size(); }
    bool isSingle() const { return pinIndices.size() == 1; }
};

/**
 * Screen-space marker clustering engine
 *
 * Clusters nearby screen-space markers using a grid-based algorithm.
 * At low altitudes, each pin is its own cluster. Above a threshold altitude,
 * nearby pins merge into clusters with count badges.
 */
class GEODRAW_API MarkerClusterer {
public:
    explicit MarkerClusterer(const ClusterConfig& config = {});

    /**
     * Update clusters from pin screen positions
     *
     * @param screenPositions Screen positions of pins (nullopt if off-screen/backfacing)
     * @param ecefPositions ECEF world positions of pins
     * @param altitude Current camera altitude in meters
     * @return true if clustering changed from previous update
     */
    bool update(const std::vector<std::optional<glm::vec2>>& screenPositions,
                const std::vector<glm::dvec3>& ecefPositions,
                double altitude);

    /**
     * Get computed clusters
     */
    const std::vector<Cluster>& getClusters() const { return clusters_; }

    /**
     * Check if clustering is active (based on altitude threshold)
     */
    bool isClusteringActive() const { return clusteringActive_; }

    /**
     * Get the current configuration
     */
    const ClusterConfig& getConfig() const { return config_; }

private:
    // Create single-pin clusters (no merging)
    void createSinglePinClusters(
        const std::vector<std::optional<glm::vec2>>& screenPositions,
        const std::vector<glm::dvec3>& ecefPositions);

    // Perform clustering using grid-based BFS
    void performClustering(
        const std::vector<std::optional<glm::vec2>>& screenPositions,
        const std::vector<glm::dvec3>& ecefPositions);

    // Compute cluster screen radius based on count
    float computeClusterRadius(size_t count) const;

    ClusterConfig config_;
    std::vector<Cluster> clusters_;
    bool clusteringActive_ = false;
};

/**
 * Get cluster color based on count
 *
 * @param count Number of pins in cluster
 * @return RGB color
 *   - 2-5:  Green  (#55CC55)
 *   - 6-20: Yellow (#FFCC33)
 *   - 21+:  Red    (#FF5533)
 */
GEODRAW_API glm::vec3 getClusterColor(size_t count);

} // namespace geodraw::clustering
