/*******************************************************************************
 * File : geometry_road_markings.hpp
 *
 * Description : See header.
 *
 * Author  : Magnus Nilsson
 * Date    : 2026-01-29
 *
 *******************************************************************************/

#pragma once

#include "geodraw/geometry/geometry.hpp"

#include <vector>
#include <glm/glm.hpp>

namespace geodraw {

// Result of ray-triangle intersection test
struct RayHit {
    bool hit = false;
    double z = 0.0;
    double distance = 0.0;
    size_t triangleIdx = 0;
};

std::optional<double> rayTriangleIntersection(
    const glm::dvec2& rayXY,
    double rayZmin,
    double rayZmax,
    const glm::dvec3& v0,
    const glm::dvec3& v1,
    const glm::dvec3& v2);

RayHit castVerticalRay(
    const glm::dvec3& origin,
    const std::vector<Mesh3>& laneMeshes,
    double searchDistance);

Mesh3 drapeMeshOntoLanes(
    const Mesh3& markerMesh,
    const std::vector<Mesh3>& laneMeshes,
    double searchDistance = 1.0,
    double zOffset = 0.02);

} // namespace geodraw
