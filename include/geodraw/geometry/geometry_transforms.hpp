/*******************************************************************************
 * File: geometry_transforms.hpp
 *
 * Description: Geometric transformation utilities for heading, orientation,
 * and polyline sampling. Consolidates common operations used across modules.
 *
 *
 ******************************************************************************/

#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <vector>

namespace geodraw {

// =============================================================================
// Heading / Tangent Utilities
// =============================================================================

/**
 * Calculate heading angle from a 2D tangent vector
 *
 * @param tangent Direction vector (only x,y components used)
 * @return Heading in radians, 0 = +X axis, pi/2 = +Y axis
 */
inline double headingFromTangent(const glm::dvec3& tangent) {
    return std::atan2(tangent.y, tangent.x);
}

/**
 * Calculate heading angle from a 2D tangent vector (vec2 overload)
 */
inline double headingFromTangent(const glm::dvec2& tangent) {
    return std::atan2(tangent.y, tangent.x);
}

/**
 * Create unit tangent vector from heading angle
 *
 * @param heading Angle in radians, 0 = +X axis
 * @return Unit vector in the heading direction (z = 0)
 */
inline glm::dvec3 tangentFromHeading(double heading) {
    return glm::dvec3(std::cos(heading), std::sin(heading), 0.0);
}

// =============================================================================
// Orientation Matrix from Heading
// =============================================================================

/**
 * Create orientation matrix from heading (ENU mode, rotation around Z)
 *
 * For local ENU coordinate systems where Z is up.
 * Heading convention: 0 = +Y (North), pi/2 = +X (East)
 *
 * @param heading Angle in radians
 * @return 3x3 rotation matrix with columns: right, forward, up
 */
inline glm::mat3 orientationFromHeading(double heading) {
    float c = std::cos(static_cast<float>(heading));
    float s = std::sin(static_cast<float>(heading));
    // Columns: right (+X rotated), forward (+Y rotated), up (Z unchanged)
    return glm::mat3(
        glm::vec3(c, s, 0),   // right
        glm::vec3(-s, c, 0),  // forward
        glm::vec3(0, 0, 1)    // up
    );
}

/**
 * Create orientation matrix from heading in ECEF coordinates (globe mode)
 *
 * Rotates around local "up" (radial from Earth center) instead of ECEF Z-axis.
 * Uses same rotation direction as orientationFromHeading (counterclockwise from North).
 *
 * @param heading Angle in radians, 0 = North, pi/2 = West (counterclockwise)
 * @param ecefPos ECEF position used to determine local up direction
 * @return 3x3 rotation matrix with columns: right, forward, up
 */
inline glm::mat3 orientationFromHeadingECEF(double heading, const glm::dvec3& ecefPos) {
    glm::dvec3 localUp = glm::normalize(ecefPos);
    glm::dvec3 northPole(0.0, 0.0, 1.0);
    glm::dvec3 localEast = glm::normalize(glm::cross(northPole, localUp));
    glm::dvec3 localNorth = glm::normalize(glm::cross(localUp, localEast));

    // ENU heading: 0 = North (+Y), pi/2 = West (-X) - counterclockwise rotation
    double c = std::cos(heading);
    double s = std::sin(heading);
    glm::dvec3 forward = localNorth * c - localEast * s;
    glm::dvec3 right = localEast * c + localNorth * s;

    return glm::mat3(glm::vec3(right), glm::vec3(forward), glm::vec3(localUp));
}

// =============================================================================
// Polyline Utilities
// =============================================================================

/**
 * Result of sampling a polyline at a given arc-length
 */
struct PolylineSample {
    glm::dvec3 position;  // Interpolated position
    glm::dvec3 tangent;   // Unit vector in direction of travel
    double heading;       // Angle in radians (atan2 of tangent)

    PolylineSample()
        : position(0.0), tangent(1.0, 0.0, 0.0), heading(0.0) {}
};

/**
 * Calculate total arc length of a polyline
 *
 * @param points Polyline vertices
 * @return Total length in same units as input coordinates
 */
inline double polylineLength(const std::vector<glm::dvec3>& points) {
    double length = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        length += glm::length(points[i] - points[i - 1]);
    }
    return length;
}

/**
 * Sample position, tangent, and heading at a given arc-length along a polyline
 *
 * @param points Polyline vertices
 * @param arcLength Distance along the polyline from the start
 * @return Interpolated sample with position, tangent, and heading
 */
inline PolylineSample samplePolyline(
    const std::vector<glm::dvec3>& points,
    double arcLength)
{
    PolylineSample sample;

    if (points.empty()) {
        return sample;
    }

    sample.position = points.front();

    if (points.size() < 2) {
        return sample;
    }

    double accumulated = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        glm::dvec3 segStart = points[i - 1];
        glm::dvec3 segEnd = points[i];
        glm::dvec3 diff = segEnd - segStart;
        double segLength = glm::length(diff);

        if (accumulated + segLength >= arcLength) {
            // Interpolate within this segment
            double t = (segLength > 1e-9) ? (arcLength - accumulated) / segLength : 0.0;
            sample.position = segStart + diff * t;
            if (segLength > 1e-9) {
                sample.tangent = diff / segLength;
            }
            sample.heading = headingFromTangent(sample.tangent);
            return sample;
        }
        accumulated += segLength;
    }

    // Past end - return last point with last segment's tangent
    sample.position = points.back();
    glm::dvec3 lastDiff = points.back() - points[points.size() - 2];
    double lastLen = glm::length(lastDiff);
    if (lastLen > 1e-9) {
        sample.tangent = lastDiff / lastLen;
    }
    sample.heading = headingFromTangent(sample.tangent);
    return sample;
}

} // namespace geodraw
