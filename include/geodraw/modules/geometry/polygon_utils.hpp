/*******************************************************************************
 * File: polygon_utils.hpp
 *
 * Description: Utility functions for polygon operations including area
 * computation, point-in-polygon testing, hexagonal grid generation, and
 * circle packing within polygons.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/geometry/geometry.hpp"
#include <cmath>
#include <vector>

namespace geodraw {
namespace geometry {

/**
 * Compute the absolute area of a polygon ring using the shoelace formula.
 *
 * The signed area is computed and then the absolute value is returned.
 * Works in any 2D coordinate system (uses X and Y components).
 *
 * @param ring Polygon ring (outer boundary or hole)
 * @return Absolute area of the polygon
 */
inline double computePolygonArea(const Polyline3& ring) {
    if (ring.points.size() < 3) {
        return 0.0;
    }

    double signedArea = 0.0;

    for (size_t i = 0; i < ring.points.size(); ++i) {
        size_t j = (i + 1) % ring.points.size();

        double x_i = ring.points[i].pos.x;
        double y_i = ring.points[i].pos.y;
        double x_j = ring.points[j].pos.x;
        double y_j = ring.points[j].pos.y;

        signedArea += (x_i * y_j - x_j * y_i);
    }

    return std::abs(signedArea * 0.5);
}

/**
 * Generate a hexagonal grid of points within a bounding box.
 *
 * Creates points arranged in a hexagonal pattern which provides optimal
 * circle packing. The spacing parameter determines the distance between
 * adjacent point centers.
 *
 * For circle packing with radius r:
 *   - horizontalSpacing = 2.0 * r (distance between column centers)
 *   - verticalSpacing = r * sqrt(3.0) (distance between row centers)
 *   - Odd rows are offset by r horizontally
 *
 * @param bounds Bounding box to fill with grid points
 * @param spacing Spacing between adjacent circles (2 * radius for packing)
 * @return Vector of grid point positions
 */
inline std::vector<Pos3> generateHexagonalGrid(const BBox3& bounds, double spacing) {
    std::vector<Pos3> points;

    if (spacing <= 0.0) {
        return points;
    }

    double horizontalSpacing = spacing;
    double verticalSpacing = spacing * std::sqrt(3.0) / 2.0;

    double minX = bounds.min.pos.x;
    double maxX = bounds.max.pos.x;
    double minY = bounds.min.pos.y;
    double maxY = bounds.max.pos.y;

    int row = 0;
    for (double y = minY; y <= maxY; y += verticalSpacing) {
        double xOffset = (row % 2 == 1) ? (spacing / 2.0) : 0.0;

        for (double x = minX + xOffset; x <= maxX; x += horizontalSpacing) {
            points.push_back(Pos3(x, y, 0.0));
        }
        row++;
    }

    return points;
}

/**
 * Filter points to keep only those inside a polygon.
 *
 * Uses ray casting algorithm for point-in-polygon testing.
 *
 * @param points Vector of candidate points
 * @param outerRing Outer boundary of the polygon
 * @return Vector of points that are inside the polygon
 */
inline std::vector<Pos3> filterPointsInPolygon(
    const std::vector<Pos3>& points,
    const Polyline3& outerRing)
{
    std::vector<Pos3> inside;

    for (const auto& pt : points) {
        if (pointInPolygon2D(pt, outerRing)) {
            inside.push_back(pt);
        }
    }

    return inside;
}

/**
 * Pack non-overlapping circles within a polygon.
 *
 * Generates a hexagonal grid with spacing = 2*radius (so circles touch but
 * don't overlap), then filters to keep only points inside the polygon.
 *
 * @param polygon Shape containing polygon(s). Uses first polygon's outer ring.
 * @param radius Circle radius for packing
 * @return Vector of circle center positions
 */
inline std::vector<Pos3> packCirclesInPolygon(const Shape3& polygon, double radius) {
    if (polygon.polygons.empty() || polygon.polygons[0].empty()) {
        return {};
    }

    const Polyline3& outerRing = polygon.polygons[0][0];

    // Compute bounding box
    BBox3 bounds;
    for (const auto& pt : outerRing.points) {
        bounds.min.pos = glm::min(bounds.min.pos, pt.pos);
        bounds.max.pos = glm::max(bounds.max.pos, pt.pos);
    }

    // Generate hexagonal grid with spacing = 2*radius for non-overlapping circles
    double spacing = 2.0 * radius;
    std::vector<Pos3> gridPoints = generateHexagonalGrid(bounds, spacing);

    // Filter points to those inside the polygon
    return filterPointsInPolygon(gridPoints, outerRing);
}

/**
 * Distribute N points evenly within a polygon.
 *
 * Uses an adaptive grid approach: generates a dense hexagonal grid and
 * samples N points from it evenly. For small N, this provides reasonable
 * distribution. For larger N, points fill the polygon more densely.
 *
 * Algorithm:
 * 1. Estimate required spacing from polygon area and N
 * 2. Generate hexagonal grid at that spacing
 * 3. Filter to points inside polygon
 * 4. If we have more points than needed, sample evenly
 * 5. If we have fewer, reduce spacing and try again
 *
 * @param polygon Shape containing polygon(s). Uses first polygon's outer ring.
 * @param numPoints Target number of points to distribute
 * @return Vector of point positions (may be fewer than requested if polygon is small)
 */
inline std::vector<Pos3> distributePointsInPolygon(const Shape3& polygon, int numPoints) {
    if (numPoints <= 0 || polygon.polygons.empty() || polygon.polygons[0].empty()) {
        return {};
    }

    const Polyline3& outerRing = polygon.polygons[0][0];

    // Compute bounding box
    BBox3 bounds;
    for (const auto& pt : outerRing.points) {
        bounds.min.pos = glm::min(bounds.min.pos, pt.pos);
        bounds.max.pos = glm::max(bounds.max.pos, pt.pos);
    }

    // Compute polygon area
    double area = computePolygonArea(outerRing);
    if (area <= 0.0) {
        return {};
    }

    // Estimate initial spacing based on area and number of points
    // For hexagonal packing, area per point ≈ sqrt(3)/2 * spacing^2
    double areaPerPoint = area / numPoints;
    double initialSpacing = std::sqrt(areaPerPoint * 2.0 / std::sqrt(3.0));

    // Try to generate enough points, reducing spacing if needed
    std::vector<Pos3> candidates;
    double spacing = initialSpacing;

    for (int attempt = 0; attempt < 10; ++attempt) {
        std::vector<Pos3> gridPoints = generateHexagonalGrid(bounds, spacing);
        candidates = filterPointsInPolygon(gridPoints, outerRing);

        if (static_cast<int>(candidates.size()) >= numPoints) {
            break;
        }

        // Reduce spacing to get more points
        spacing *= 0.8;
    }

    // If we have exactly the right number or fewer, return all
    if (static_cast<int>(candidates.size()) <= numPoints) {
        return candidates;
    }

    // Sample N points evenly from the candidates
    std::vector<Pos3> result;
    result.reserve(numPoints);

    double step = static_cast<double>(candidates.size()) / numPoints;
    for (int i = 0; i < numPoints; ++i) {
        int index = static_cast<int>(i * step);
        if (index < static_cast<int>(candidates.size())) {
            result.push_back(candidates[index]);
        }
    }

    return result;
}

} // namespace geometry
} // namespace geodraw
