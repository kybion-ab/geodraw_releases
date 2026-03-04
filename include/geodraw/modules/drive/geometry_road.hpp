/*******************************************************************************
 * File : geometry_road.hpp
 *
 * Description :
 *
 * Author  : Magnus Nilsson
 * Date    : 2026-01-29
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/geometry/geometry.hpp"

namespace geodraw {


// Lane represented by left and right boundary polylines
struct Lane {
    Polyline3 leftBoundary;    // Required: left edge of lane
    Polyline3 rightBoundary;   // Required: right edge of lane
    Polyline3 centerline;      // Optional: lane center (for visualization/reference)

    Lane() = default;
    Lane(const Polyline3& left, const Polyline3& right)
        : leftBoundary(left), rightBoundary(right) {}

    bool isValid() const {
        return leftBoundary.size() >= 2 && rightBoundary.size() >= 2;
    }
};

// Road represented by left and right boundary polylines
struct Road {
    Polyline3 leftBoundary;    // Required: left edge of road
    Polyline3 rightBoundary;   // Required: right edge of road

    Road() = default;
    Road(const Polyline3& left, const Polyline3& right)
        : leftBoundary(left), rightBoundary(right) {}

    bool isValid() const {
        return leftBoundary.size() >= 2 && rightBoundary.size() >= 2;
    }
};

GEODRAW_API std::vector<Pos3> resamplePolyline(const Polyline3& polyline, size_t targetCount);

GEODRAW_API Mesh3 createRibbonMesh(const Polyline3& leftBoundary, const Polyline3& rightBoundary);

}
