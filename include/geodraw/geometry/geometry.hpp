/*******************************************************************************
 * File : geometry.hpp
 *
 * Description: Header-only library containing type-safe geometric primitives
 * for both 3D and 2D space. All types are pure data structures with no OpenGL
 * dependencies.
 *
 * Why required: Provides the fundamental building blocks for describing
 * geometry in geodraw. Users construct scenes by creating and combining these
 * primitives, which are then passed to Scene for rendering.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2025-12-21
 *
 *******************************************************************************/

// geometry.hpp
#pragma once
#include <cmath>
#include <limits>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <array>
#include <optional>
#include "../external/earcut/earcut.hpp"

namespace geodraw {

// ==========================
// Coordinate Systems
// ==========================

enum class CoordSystem {
    UNDEFINED,    // Unknown or unspecified
    LOCAL,        // Local coordinate system (origin+rotation defined in ECEF)
    WGS84,        // GPS coordinates (lat/lon/alt)
    ENU,          // East-North-Up relative to a GeoReference
    ECEF,         // Earth-Centered Earth-Fixed (meters)
    WEB_MERCATOR  // Web Mercator projection (x/y in radians)
};

// Defines origin and orientation for LOCAL coordinate system
// Origin and rotation are expressed in ECEF coordinates
struct LocalFrame {
    glm::dvec3 origin = glm::dvec3(0.0);   // Origin position in ECEF (meters)
    glm::dmat3 rotation = glm::dmat3(1.0); // Rotation matrix (columns are local X,Y,Z axes in ECEF)

    LocalFrame() = default;
    LocalFrame(const glm::dvec3& o, const glm::dmat3& r) : origin(o), rotation(r) {}
};

// Forward declaration for ConversionContext (defined in geometry_coords.hpp)
struct ConversionContext;

// ==========================
// 3D Primitives
// ==========================

// 3D position
struct Pos3 {
  glm::dvec3 pos = glm::dvec3(0.0); // x, y, z

  Pos3() = default;
  Pos3(double x, double y, double z) : pos(x, y, z) {}

  // Rotate this point around a bound vector (anchor + direction) by angle (radians).
  Pos3 rotateAround(const glm::dvec3& anchor, const glm::dvec3& axis, double angle) const {
    glm::dmat4 rot = glm::rotate(glm::dmat4(1.0), angle, glm::normalize(axis));
    glm::dvec3 local = pos - anchor;
    glm::dvec3 rotated = glm::dvec3(rot * glm::dvec4(local, 0.0));
    glm::dvec3 result = rotated + anchor;
    return Pos3(result.x, result.y, result.z);
  }
};

// Line segment in 3D
struct Line3 {
  Pos3 start;
  Pos3 end;

  Line3() = default;
  Line3(const Pos3 &s, const Pos3 &e) : start(s), end(e) {}
};

// Polyline in 3D
struct Polyline3 {
  std::vector<Pos3> points;

  void addPoint(const Pos3 &p) { points.push_back(p); }
  size_t size() const { return points.size(); }
};

// Triangle in 3D (3 vertices forming a face)
struct Triangle3 {
  Pos3 p0, p1, p2;
  glm::vec2 uv0, uv1, uv2; // UV texture coordinates (default 0,0)

  Triangle3() : uv0(0.0, 0.0), uv1(0.0, 0.0), uv2(0.0, 0.0) {}
  Triangle3(const Pos3 &v0, const Pos3 &v1, const Pos3 &v2)
      : p0(v0), p1(v1), p2(v2), uv0(0.0, 0.0), uv1(0.0, 0.0), uv2(0.0, 0.0) {}
  Triangle3(const Pos3 &v0, const Pos3 &v1, const Pos3 &v2, const glm::vec2 &t0,
            const glm::vec2 &t1, const glm::vec2 &t2)
      : p0(v0), p1(v1), p2(v2), uv0(t0), uv1(t1), uv2(t2) {}

  bool hasUVs() const {
    return uv0 != glm::vec2(0.0, 0.0) || uv1 != glm::vec2(0.0, 0.0) ||
           uv2 != glm::vec2(0.0, 0.0);
  }
};

// Quadrilateral in 3D (4 vertices forming a face)
struct Quad3 {
  Pos3 p0, p1, p2, p3;

  Quad3() = default;
  Quad3(const Pos3 &v0, const Pos3 &v1, const Pos3 &v2, const Pos3 &v3)
      : p0(v0), p1(v1), p2(v2), p3(v3) {}
};

// Pose expressed with ypr
struct Pos6 {
  double x;
  double y;
  double z;
  double yaw;
  double pitch;
  double roll;
};

// Pose expressed as rotation and translation
struct Pose3 {
  Pos3 t;
  glm::mat3 R{glm::mat3(1.0f)};
};

// ==========================
// Conversion functions between Pose3 and Pos6
// ==========================

// Convert Pos6 (x,y,z,yaw,pitch,roll) to Pose3 (translation + rotation matrix)
// Convention: R = Rz(yaw) * Ry(pitch) * Rx(roll)
inline Pose3 toPose3(const Pos6 &pos6) {
  Pose3 pose;

  // Translation
  pose.t = Pos3(pos6.x, pos6.y, pos6.z);

  // Rotation matrix from Euler angles (ZYX convention)
  double cy = std::cos(pos6.yaw);
  double sy = std::sin(pos6.yaw);
  double cp = std::cos(pos6.pitch);
  double sp = std::sin(pos6.pitch);
  double cr = std::cos(pos6.roll);
  double sr = std::sin(pos6.roll);

  // R = Rz(yaw) * Ry(pitch) * Rx(roll)
  pose.R[0][0] = cy * cp;
  pose.R[0][1] = cy * sp * sr - sy * cr;
  pose.R[0][2] = cy * sp * cr + sy * sr;

  pose.R[1][0] = sy * cp;
  pose.R[1][1] = sy * sp * sr + cy * cr;
  pose.R[1][2] = sy * sp * cr - cy * sr;

  pose.R[2][0] = -sp;
  pose.R[2][1] = cp * sr;
  pose.R[2][2] = cp * cr;

  return pose;
}

// Convert Pose3 (translation + rotation matrix) to Pos6 (x,y,z,yaw,pitch,roll)
// Extracts Euler angles from rotation matrix (ZYX convention)
// Note: Returns angles in radians
inline Pos6 toPos6(const Pose3 &pose) {
  Pos6 pos6;

  // Translation
  pos6.x = pose.t.pos.x;
  pos6.y = pose.t.pos.y;
  pos6.z = pose.t.pos.z;

  // Extract Euler angles from rotation matrix
  // Assuming R = Rz(yaw) * Ry(pitch) * Rx(roll)

  // Pitch: from R[2][0] = -sin(pitch)
  pos6.pitch = std::asin(-pose.R[2][0]);

  // Check for gimbal lock
  double cp = std::cos(pos6.pitch);
  if (std::abs(cp) > 1e-6) {
    // Normal case
    pos6.yaw = std::atan2(pose.R[1][0] / cp, pose.R[0][0] / cp);
    pos6.roll = std::atan2(pose.R[2][1] / cp, pose.R[2][2] / cp);
  } else {
    // Gimbal lock case (pitch = ±90°)
    pos6.yaw = std::atan2(-pose.R[0][1], pose.R[1][1]);
    pos6.roll = 0.0; // Set roll to zero by convention
  }

  return pos6;
}

// A "quad" that represents a ground.
struct Ground {
  Pos6 pose;     // Center
  double width;  // Along x
  double height; // Along y
  CoordSystem coordSystem = CoordSystem::UNDEFINED;
};

inline constexpr double inf = std::numeric_limits<double>::infinity();

// Axis-aligned bounding box (3D)
struct BBox3 {
  Pos3 min{inf, inf, inf};
  Pos3 max{-inf, -inf, -inf};
  CoordSystem coordSystem = CoordSystem::UNDEFINED;

  BBox3() = default;
  BBox3(const Pos3 &min_, const Pos3 &max_) : min(min_), max(max_) {}

  glm::dvec3 center() const { return (min.pos + max.pos) * 0.5; }
  glm::dvec3 size() const { return max.pos - min.pos; }

  void include(Pos3 point){
    max.pos = glm::max(max.pos, point.pos);
    min.pos = glm::min(min.pos, point.pos);
  };

  // Convert to target coordinate system (implemented in geometry_coords.hpp)
  BBox3 convertTo(CoordSystem target, const ConversionContext& ctx) const;
};

/**
 * Plane in 3D space defined by normal and distance from origin
 *
 * The plane equation is: normal.x * x + normal.y * y + normal.z * z + d = 0
 * Points where the equation > 0 are on the positive (front) side.
 */
struct Plane3 {
  glm::dvec3 normal = glm::dvec3(0, 0, 1);  // Unit normal vector
  double d = 0.0;                            // Distance from origin (negative of dot(normal, pointOnPlane))

  Plane3() = default;
  Plane3(const glm::dvec3& n, double dist) : normal(glm::normalize(n)), d(dist) {}

  /**
   * Create plane from normal and a point on the plane
   */
  static Plane3 fromNormalAndPoint(const glm::dvec3& normal, const glm::dvec3& point) {
    glm::dvec3 n = glm::normalize(normal);
    return Plane3(n, -glm::dot(n, point));
  }

  /**
   * Create plane from three points (CCW winding = normal points toward viewer)
   */
  static Plane3 fromPoints(const glm::dvec3& p0, const glm::dvec3& p1, const glm::dvec3& p2) {
    glm::dvec3 v1 = p1 - p0;
    glm::dvec3 v2 = p2 - p0;
    glm::dvec3 normal = glm::normalize(glm::cross(v1, v2));
    return fromNormalAndPoint(normal, p0);
  }

  /**
   * Signed distance from point to plane (positive = front side, negative = back)
   */
  double signedDistance(const glm::dvec3& point) const {
    return glm::dot(normal, point) + d;
  }

  /**
   * Test which side of the plane a point is on
   * @return 1 = front (positive), -1 = back (negative), 0 = on plane
   */
  int classify(const glm::dvec3& point, double epsilon = 1e-6) const {
    double dist = signedDistance(point);
    if (dist > epsilon) return 1;
    if (dist < -epsilon) return -1;
    return 0;
  }
};

/**
 * View frustum defined by 6 planes
 *
 * The frustum is a truncated pyramid representing the visible volume of a camera.
 * Each plane's normal points inward (toward the visible volume).
 *
 * Planes are indexed as:
 *   0 = near, 1 = far, 2 = left, 3 = right, 4 = top, 5 = bottom
 */
struct Frustum3 {
  std::array<Plane3, 6> planes;
  CoordSystem coordSystem = CoordSystem::UNDEFINED;

  // Plane indices for clarity
  static constexpr int NEAR = 0;
  static constexpr int FAR = 1;
  static constexpr int LEFT = 2;
  static constexpr int RIGHT = 3;
  static constexpr int TOP = 4;
  static constexpr int BOTTOM = 5;

  Frustum3() = default;

  /**
   * Create frustum from view-projection matrix
   *
   * Extracts the 6 frustum planes from a combined view-projection matrix.
   * This is the standard method used in real-time graphics.
   *
   * @param viewProj Combined view * projection matrix
   */
  static Frustum3 fromViewProjection(const glm::dmat4& viewProj) {
    Frustum3 f;

    // Extract planes from the view-projection matrix columns
    // Reference: "Fast Extraction of Viewing Frustum Planes from the WorldView-Projection Matrix"
    glm::dvec4 row0(viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]);
    glm::dvec4 row1(viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]);
    glm::dvec4 row2(viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]);
    glm::dvec4 row3(viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]);

    // Left:   row3 + row0
    glm::dvec4 left = row3 + row0;
    f.planes[LEFT] = Plane3(glm::dvec3(left.x, left.y, left.z), left.w);

    // Right:  row3 - row0
    glm::dvec4 right = row3 - row0;
    f.planes[RIGHT] = Plane3(glm::dvec3(right.x, right.y, right.z), right.w);

    // Bottom: row3 + row1
    glm::dvec4 bottom = row3 + row1;
    f.planes[BOTTOM] = Plane3(glm::dvec3(bottom.x, bottom.y, bottom.z), bottom.w);

    // Top:    row3 - row1
    glm::dvec4 top = row3 - row1;
    f.planes[TOP] = Plane3(glm::dvec3(top.x, top.y, top.z), top.w);

    // Near:   row3 + row2
    glm::dvec4 near = row3 + row2;
    f.planes[NEAR] = Plane3(glm::dvec3(near.x, near.y, near.z), near.w);

    // Far:    row3 - row2
    glm::dvec4 far = row3 - row2;
    f.planes[FAR] = Plane3(glm::dvec3(far.x, far.y, far.z), far.w);

    // Normalize all planes
    for (auto& plane : f.planes) {
      double len = glm::length(plane.normal);
      if (len > 1e-10) {
        plane.normal /= len;
        plane.d /= len;
      }
    }

    return f;
  }

  /**
   * Test if a point is inside the frustum
   */
  bool containsPoint(const glm::dvec3& point) const {
    for (const auto& plane : planes) {
      if (plane.signedDistance(point) < 0) {
        return false;  // Point is behind this plane
      }
    }
    return true;
  }

  /**
   * Test if an axis-aligned bounding box intersects the frustum
   *
   * Uses the "positive vertex" approach for efficient AABB-frustum testing.
   *
   * @return true if box is at least partially inside frustum
   */
  bool intersectsAABB(const glm::dvec3& boxMin, const glm::dvec3& boxMax) const {
    for (const auto& plane : planes) {
      // Find the "positive vertex" (corner furthest along plane normal)
      glm::dvec3 pVertex(
        plane.normal.x >= 0 ? boxMax.x : boxMin.x,
        plane.normal.y >= 0 ? boxMax.y : boxMin.y,
        plane.normal.z >= 0 ? boxMax.z : boxMin.z
      );

      // If positive vertex is behind the plane, box is completely outside
      if (plane.signedDistance(pVertex) < 0) {
        return false;
      }
    }
    return true;
  }

  /**
   * Test if a BBox3 intersects the frustum
   */
  bool intersects(const BBox3& box) const {
    return intersectsAABB(box.min.pos, box.max.pos);
  }
};

// Oriented bounding box (3D).
struct RBox3 {
  Pose3 pose;
  BBox3 bbox;
  CoordSystem coordSystem = CoordSystem::UNDEFINED;
};

// Triangle mesh in 3D
struct Mesh3 {
  std::vector<Pos3> vertices;
  std::vector<glm::vec2>
      uvs; // Optional UV coords (must match vertices.size() if used)
  std::vector<unsigned int> indices; // Triangle indices (groups of 3)
  CoordSystem coordSystem = CoordSystem::UNDEFINED;

  Mesh3() = default;
  Mesh3(const std::vector<Pos3> &verts, const std::vector<unsigned int> &inds)
      : vertices(verts), indices(inds) {}

  size_t vertexCount() const { return vertices.size(); }
  size_t triangleCount() const { return indices.size() / 3; }
  bool hasUVs() const { return uvs.size() == vertices.size(); }

  // Convert to target coordinate system (implemented in geometry_coords.hpp)
  Mesh3 convertTo(CoordSystem target, const ConversionContext& ctx) const;
};

/**
 * Shape3 - 3D planar shape with multiple polygons, lines, and points
 *
 * Supports:
 * - Multiple polygons with holes
 * - Each polygon is represented as a vector of rings (Polyline3)
 * - First ring in each polygon = outer boundary (CCW winding)
 * - Remaining rings = holes (CW winding)
 * - Isolated lines
 * - Isolated points
 *
 * Tesselation converts polygons to Mesh3 for rendering.
 */
struct Shape3 {
  std::vector<std::vector<Polyline3>> polygons;  // Array of polygons, each is [outer, hole1, hole2, ...]
  std::vector<Pos3> points;                      // Isolated points
  std::vector<Polyline3> lines;                  // Isolated polylines
  std::string user_data;                         // Application-specific metadata (JSON, etc.)
  BBox3 bbox;                                    // Bounding box for shape.
  CoordSystem coordSystem = CoordSystem::UNDEFINED;


  bool isEmpty() const {
    return polygons.empty() && points.empty() && lines.empty();
  }

  bool hasPolygons() const {
    return !polygons.empty();
  }

  size_t numPolygons() const {
    return polygons.size();
  }

  // Get number of holes in a specific polygon (rings after the first one)
  size_t numHoles(size_t polyIndex) const {
    if (polyIndex >= polygons.size() || polygons[polyIndex].empty()) {
      return 0;
    }
    return polygons[polyIndex].size() - 1;  // All rings except the first
  }

  BBox3 bounds() const {
    BBox3 box;
    box.coordSystem = coordSystem;
    for(auto polygon : polygons){
      for(auto rings : polygon){
        for(auto p : rings.points){
          box.include(p);
        }
      }
    }
    for(auto l : lines){
      for(auto p : l.points){
        box.include(p);
      }
    }
    for(auto p : points){
      box.include(p);
    }
    return box;
  }

  // Convert to target coordinate system (implemented in geometry_coords.hpp)
  Shape3 convertTo(CoordSystem target, const ConversionContext& ctx) const;
};

/**
 * Tesselate Shape3 polygons to triangle mesh
 *
 * Converts all polygons (with their holes) into a single Mesh3 using earcut
 * triangulation algorithm. Points and lines are NOT included in mesh.
 *
 * Each polygon is an array of rings where:
 * - First ring = outer boundary (CCW winding)
 * - Remaining rings = holes (CW winding)
 *
 * Returns empty Mesh3 if:
 * - Shape has no polygons
 * - All polygons are invalid
 * - Tesselation fails for all polygons
 *
 * @param shape The Shape3 to tesselate
 * @return Mesh3 with triangulated polygons (no UVs)
 */
inline Mesh3 tesselate(const Shape3& shape) {
  if (!shape.hasPolygons()) {
    return Mesh3{};
  }

  Mesh3 result;

  // Tesselate each polygon and combine into single mesh
  for (const auto& rings : shape.polygons) {
    if (rings.empty() || rings[0].points.size() < 3) {
      continue;  // Skip invalid polygons
    }

    // Build earcut input format: vector of rings
    // Each ring is vector of [x, y, z] arrays
    using Point = std::array<double, 3>;
    std::vector<std::vector<Point>> polygon;

    // Add all rings (first = outer, rest = holes)
    for (const auto& ring : rings) {
      if (ring.points.size() < 3) continue;  // Skip invalid rings

      std::vector<Point> earcutRing;
      for (const auto& p : ring.points) {
        earcutRing.push_back({p.pos.x, p.pos.y, p.pos.z});
      }
      polygon.push_back(earcutRing);
    }

    if (polygon.empty()) {
      continue;  // No valid rings in this polygon
    }

    // Run earcut triangulation for this polygon
    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);

    if (indices.empty()) {
      continue;  // Tesselation failed for this polygon
    }

    // Track the vertex offset for this polygon
    size_t vertexOffset = result.vertices.size();

    // Collect all vertices from all rings
    for (const auto& ring : rings) {
      result.vertices.insert(result.vertices.end(),
                            ring.points.begin(),
                            ring.points.end());
    }

    // Add indices with offset
    for (uint32_t idx : indices) {
      result.indices.push_back(static_cast<unsigned int>(idx + vertexOffset));
    }
  }

  return result;
}

  // ================
  // 2D primitives
  // ================

  // 2D position
struct Pos2 {
  glm::dvec2 pos = glm::dvec2(0.0); // x, y

  Pos2() = default;
  Pos2(double x, double y) : pos(x, y) {}
};

// Axis-aligned bounding box (2D)
struct BBox2 {
  Pos2 min{inf, inf};
  Pos2 max{-inf, -inf};
  CoordSystem coordSystem = CoordSystem::UNDEFINED;

  BBox2() = default;
  BBox2(const Pos2 &min_, const Pos2 &max_) : min(min_), max(max_) {}

  bool intersects(const BBox2& other) const {
    return !(glm::any(glm::lessThan(max.pos, other.min.pos)) ||
             glm::any(glm::greaterThan(min.pos, other.max.pos)));
  }

  bool contains(const BBox2& other) const {
    return (glm::all(glm::lessThanEqual(min.pos, other.min.pos)) &&
            glm::all(glm::greaterThanEqual(max.pos, other.max.pos)));
  }

  bool containsPoint(Pos2 point) const {
    return (glm::all(glm::greaterThanEqual(point.pos, min.pos)) &&
            glm::all(glm::lessThanEqual(point.pos, max.pos)));
  }

  glm::dvec2 center() const {
    return (min.pos + max.pos) * 0.5;
  }

  glm::dvec2 size() const {
    return max.pos - min.pos;
  }
};

// ==========================
// 2D Geometric Algorithms
// ==========================

/**
 * 2D line segment intersection test
 *
 * Tests if two line segments intersect in the XY plane.
 * Uses parametric line equations to find intersection point.
 *
 * @param a0 First endpoint of segment A
 * @param a1 Second endpoint of segment A
 * @param b0 First endpoint of segment B
 * @param b1 Second endpoint of segment B
 * @return Intersection point if segments intersect within [0,1] parameter range,
 *         std::nullopt otherwise
 */
inline std::optional<glm::dvec2> edgeIntersection2D(
    const glm::dvec2& a0, const glm::dvec2& a1,
    const glm::dvec2& b0, const glm::dvec2& b1)
{
    glm::dvec2 aDir = a1 - a0;
    glm::dvec2 bDir = b1 - b0;

    // Compute denominator (cross product in 2D)
    double denom = aDir.x * bDir.y - aDir.y * bDir.x;

    // Check if lines are parallel or coincident
    if (std::abs(denom) < 1e-10) {
        return std::nullopt;
    }

    glm::dvec2 diff = b0 - a0;

    // Compute parameters s and t
    double s = (diff.x * bDir.y - diff.y * bDir.x) / denom;
    double t = (diff.x * aDir.y - diff.y * aDir.x) / denom;

    // Small epsilon to handle edge cases
    const double epsilon = -1e-6;

    // Check if intersection point is within both segments
    if (s >= epsilon && s <= 1.0 + epsilon &&
        t >= epsilon && t <= 1.0 + epsilon) {
        return a0 + s * aDir;
    }

    return std::nullopt;
}

/**
 * Point-in-polygon test using ray casting algorithm
 *
 * Projects point and polygon to XY plane and counts ray-edge intersections.
 * Odd number of intersections = point inside, even = outside.
 *
 * @param point Test point
 * @param ring Polygon boundary (closed loop)
 * @return true if point is inside polygon, false otherwise
 */
inline bool pointInPolygon2D(const Pos3& point, const Polyline3& ring) {
    if (ring.points.size() < 3) {
        return false;  // Degenerate polygon
    }

    int crossings = 0;
    glm::dvec2 p(point.pos.x, point.pos.y);

    for (size_t i = 0; i < ring.points.size(); ++i) {
        size_t j = (i + 1) % ring.points.size();

        glm::dvec2 vi(ring.points[i].pos.x, ring.points[i].pos.y);
        glm::dvec2 vj(ring.points[j].pos.x, ring.points[j].pos.y);

        // Check if horizontal ray from p crosses edge vi-vj
        if ((vi.y > p.y) != (vj.y > p.y)) {
            // Compute X coordinate of intersection
            double xIntersect = vi.x + (p.y - vi.y) * (vj.x - vi.x) / (vj.y - vi.y);

            if (p.x < xIntersect) {
                crossings++;
            }
        }
    }

    return (crossings % 2) == 1;  // Odd = inside
}

/**
 * Test if polygon ring self-intersects
 *
 * Checks all non-adjacent edge pairs for intersections.
 *
 * @param ring Polygon ring to test
 * @return true if ring self-intersects (invalid), false if simple polygon
 */
inline bool ringSelfIntersects2D(const Polyline3& ring) {
    if (ring.points.size() < 4) {
        return false;  // Triangle cannot self-intersect
    }

    for (size_t i = 0; i < ring.points.size(); ++i) {
        size_t j = (i + 1) % ring.points.size();
        glm::dvec2 a0(ring.points[i].pos.x, ring.points[i].pos.y);
        glm::dvec2 a1(ring.points[j].pos.x, ring.points[j].pos.y);

        // Check against non-adjacent edges (skip i-1, i, i+1)
        for (size_t k = i + 2; k < ring.points.size(); ++k) {
            // Skip last edge vs first edge (they share a vertex)
            if (i == 0 && k == ring.points.size() - 1) continue;

            size_t l = (k + 1) % ring.points.size();
            glm::dvec2 b0(ring.points[k].pos.x, ring.points[k].pos.y);
            glm::dvec2 b1(ring.points[l].pos.x, ring.points[l].pos.y);

            if (edgeIntersection2D(a0, a1, b0, b1).has_value()) {
                return true;  // Self-intersection found
            }
        }
    }

    return false;
}

/**
 * Test if two polygon rings have intersecting edges
 *
 * @param ring1 First polygon ring
 * @param ring2 Second polygon ring
 * @return true if any edges intersect, false otherwise
 */
inline bool ringsIntersect2D(const Polyline3& ring1, const Polyline3& ring2) {
    if (ring1.points.size() < 2 || ring2.points.size() < 2) {
        return false;
    }

    // Check all edge pairs
    for (size_t i = 0; i < ring1.points.size(); ++i) {
        size_t j = (i + 1) % ring1.points.size();
        glm::dvec2 a0(ring1.points[i].pos.x, ring1.points[i].pos.y);
        glm::dvec2 a1(ring1.points[j].pos.x, ring1.points[j].pos.y);

        for (size_t k = 0; k < ring2.points.size(); ++k) {
            size_t l = (k + 1) % ring2.points.size();
            glm::dvec2 b0(ring2.points[k].pos.x, ring2.points[k].pos.y);
            glm::dvec2 b1(ring2.points[l].pos.x, ring2.points[l].pos.y);

            if (edgeIntersection2D(a0, a1, b0, b1).has_value()) {
                return true;
            }
        }
    }

    return false;
}

/**
 * Compute winding order of polygon ring using signed area
 *
 * Uses shoelace formula to compute signed area.
 * Positive area = counter-clockwise (CCW)
 * Negative area = clockwise (CW)
 *
 * @param ring Polygon ring
 * @return Positive for CCW, negative for CW, zero for degenerate
 */
inline double computeWindingOrder2D(const Polyline3& ring) {
    if (ring.points.size() < 3) {
        return 0.0;  // Degenerate
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

    return signedArea * 0.5;  // Actual signed area
}

/**
 * Reverse point order in polygon ring
 *
 * Reverses the winding order (CCW ↔ CW)
 *
 * @param ring Polygon ring to reverse (modified in place)
 */
inline void reverseRing(Polyline3& ring) {
    std::reverse(ring.points.begin(), ring.points.end());
}

// ==========================
// Distance Calculations
// ==========================

/**
 * Compute distance from point to line segment
 *
 * Finds the closest point on segment [a,b] to point p and returns the distance.
 * Handles degenerate segments (a == b) by returning distance to point a.
 *
 * @param p Query point
 * @param a Segment start point
 * @param b Segment end point
 * @return Distance from p to closest point on segment
 */
inline float distanceToSegment(const Pos3& p, const Pos3& a, const Pos3& b) {
    glm::dvec3 ab = b.pos - a.pos;
    glm::dvec3 ap = p.pos - a.pos;

    float abLen = static_cast<float>(glm::length(ab));
    if (abLen < 1e-6f) {
        // Degenerate segment - distance to point a
        return static_cast<float>(glm::length(ap));
    }

    // Project p onto line, clamp to segment
    float t = std::clamp(static_cast<float>(glm::dot(ap, ab) / (abLen * abLen)), 0.0f, 1.0f);
    glm::dvec3 closest = a.pos + ab * static_cast<double>(t);

    return static_cast<float>(glm::length(p.pos - closest));
}

/**
 * Compute minimum distance from point to Shape3
 *
 * Tests distance to all components of a Shape3:
 * - Polygons: Returns 0 if point is inside outer ring, otherwise distance to edges
 * - Polylines: Distance to all segments
 * - Points: Direct distance
 *
 * @param point Query point
 * @param shape Shape to test against
 * @return Minimum distance (0 if inside polygon)
 */
inline float computeDistanceToShape(const Pos3& point, const Shape3& shape) {
    float minDist = std::numeric_limits<float>::max();

    // Test polygons
    for (const auto& rings : shape.polygons) {
        if (rings.empty()) continue;
        const auto& outerRing = rings[0];

        // Check if point is inside polygon (2D test in XY plane)
        if (pointInPolygon2D(point, outerRing)) {
            return 0.0f;  // Inside polygon
        }

        // Compute distance to polygon edges
        for (size_t i = 0; i < outerRing.points.size(); ++i) {
            size_t j = (i + 1) % outerRing.points.size();
            float dist = distanceToSegment(point, outerRing.points[i], outerRing.points[j]);
            minDist = std::min(minDist, dist);
        }
    }

    // Test isolated polylines (Polyline3, not Line3)
    for (const auto& polyline : shape.lines) {
        for (size_t i = 0; i + 1 < polyline.points.size(); ++i) {
            float dist = distanceToSegment(point, polyline.points[i], polyline.points[i + 1]);
            minDist = std::min(minDist, dist);
        }
    }

    // Test isolated points
    for (const auto& pt : shape.points) {
        float dist = static_cast<float>(glm::length(pt.pos - point.pos));
        minDist = std::min(minDist, dist);
    }

    return minDist;
}

} // namespace geodraw
