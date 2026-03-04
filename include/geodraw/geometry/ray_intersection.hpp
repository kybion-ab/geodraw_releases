/*******************************************************************************
 * File : ray_intersection.hpp
 *
 * Description: Ray intersection algorithms for picking support.
 * Contains Möller-Trumbore ray-triangle intersection and ray-AABB tests.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-01-17
 *
 *******************************************************************************/

#pragma once
#include <glm/glm.hpp>
#include <optional>
#include <cmath>

#include "geodraw/geometry/ray.hpp"
#include "geodraw/geometry/geometry.hpp"

namespace geodraw {

/**
 * Möller-Trumbore ray-triangle intersection algorithm.
 *
 * Fast ray-triangle intersection test that avoids computing the plane equation.
 * Returns the distance along the ray if hit, nullopt otherwise.
 *
 * @param ray The ray to test (origin and direction)
 * @param v0, v1, v2 Triangle vertices in CCW order
 * @return Distance along ray if hit, nullopt if miss
 */
inline std::optional<float> rayTriangleIntersect(
    const Ray& ray,
    const glm::vec3& v0,
    const glm::vec3& v1,
    const glm::vec3& v2)
{
    constexpr float EPSILON = 1e-7f;

    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;

    // Calculate determinant - also used for u parameter
    glm::vec3 pvec = glm::cross(ray.direction, edge2);
    float det = glm::dot(edge1, pvec);

    // Ray is parallel to triangle plane if det is near zero
    if (std::abs(det) < EPSILON) {
        return std::nullopt;
    }

    float invDet = 1.0f / det;

    // Calculate distance from v0 to ray origin
    glm::vec3 tvec = ray.origin - v0;

    // Calculate u parameter and test bounds
    float u = glm::dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }

    // Prepare to test v parameter
    glm::vec3 qvec = glm::cross(tvec, edge1);

    // Calculate v parameter and test bounds
    float v = glm::dot(ray.direction, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }

    // Calculate t (distance along ray)
    float t = glm::dot(edge2, qvec) * invDet;

    // Only return hits in front of the ray origin
    if (t > EPSILON) {
        return t;
    }

    return std::nullopt;
}

/**
 * Ray-AABB (Axis-Aligned Bounding Box) intersection test.
 *
 * Uses the slab method for efficient ray-box intersection.
 * Useful for fast rejection before testing individual triangles.
 *
 * @param ray The ray to test
 * @param box The axis-aligned bounding box
 * @param tMin Output: distance to near intersection point (if hit)
 * @return true if ray intersects box, false otherwise
 */
inline bool rayAABBIntersect(
    const Ray& ray,
    const BBox3& box,
    float& tMin)
{
    glm::vec3 boxMin(box.min.pos);
    glm::vec3 boxMax(box.max.pos);

    float t1 = (boxMin.x - ray.origin.x) / ray.direction.x;
    float t2 = (boxMax.x - ray.origin.x) / ray.direction.x;
    float t3 = (boxMin.y - ray.origin.y) / ray.direction.y;
    float t4 = (boxMax.y - ray.origin.y) / ray.direction.y;
    float t5 = (boxMin.z - ray.origin.z) / ray.direction.z;
    float t6 = (boxMax.z - ray.origin.z) / ray.direction.z;

    float tNear = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    float tFar = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    // If tFar < 0, ray intersects AABB but behind origin
    // If tNear > tFar, ray doesn't intersect AABB
    if (tFar < 0 || tNear > tFar) {
        return false;
    }

    tMin = tNear;
    return true;
}

/**
 * Ray-sphere intersection test (float precision).
 *
 * Uses the quadratic formula to find ray-sphere intersection points.
 * Returns the nearest intersection distance (t > 0), or -1 if no hit.
 *
 * @param ray The ray to test (origin and direction)
 * @param center The center of the sphere
 * @param radius The radius of the sphere
 * @return Distance along ray if hit (nearest positive t), -1 if no intersection
 */
inline float raySphereIntersect(
    const Ray& ray,
    const glm::vec3& center,
    float radius)
{
    glm::vec3 oc = ray.origin - center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0) return -1.0f;  // No intersection

    float sqrtD = std::sqrt(discriminant);
    float t1 = (-b - sqrtD) / (2.0f * a);
    float t2 = (-b + sqrtD) / (2.0f * a);

    // Return nearest positive t (in front of ray origin)
    if (t1 > 0) return t1;
    if (t2 > 0) return t2;
    return -1.0f;  // Both behind camera
}

/**
 * Ray-sphere intersection test (double precision).
 *
 * Essential for GLOBE mode where coordinates are in ECEF (millions of meters).
 * Uses double precision throughout to avoid numerical issues at large distances.
 *
 * @param rayOrigin The ray origin point
 * @param rayDirection The ray direction (should be normalized)
 * @param center The center of the sphere
 * @param radius The radius of the sphere
 * @return Distance along ray if hit (nearest positive t), -1 if no intersection
 */
inline double raySphereIntersectDouble(
    const glm::dvec3& rayOrigin,
    const glm::dvec3& rayDirection,
    const glm::dvec3& center,
    double radius)
{
    glm::dvec3 oc = rayOrigin - center;
    double a = glm::dot(rayDirection, rayDirection);
    double b = 2.0 * glm::dot(oc, rayDirection);
    double c = glm::dot(oc, oc) - radius * radius;
    double discriminant = b * b - 4.0 * a * c;

    if (discriminant < 0) return -1.0;  // No intersection

    double sqrtD = std::sqrt(discriminant);
    double t1 = (-b - sqrtD) / (2.0 * a);
    double t2 = (-b + sqrtD) / (2.0 * a);

    // Return nearest positive t (in front of ray origin)
    if (t1 > 0) return t1;
    if (t2 > 0) return t2;
    return -1.0;  // Both behind camera
}

} // namespace geodraw
