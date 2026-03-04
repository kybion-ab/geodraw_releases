/*******************************************************************************
 * File: geometry_coords.hpp
 *
 * Description: Coordinate system conversion functions for geometric primitives.
 * Provides conversions between LOCAL, WGS84, ENU, ECEF, and WEB_MERCATOR
 * coordinate systems.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/modules/earth/earth_coords.hpp"
#include <stdexcept>

namespace geodraw {

// =============================================================================
// Conversion Context
// =============================================================================

/**
 * Context for coordinate conversions
 *
 * Holds reference frames needed for conversions:
 * - localFrame: For LOCAL <-> ECEF conversions
 * - geoRef: For ENU <-> WGS84 conversions
 */
struct ConversionContext {
    LocalFrame localFrame;           // For LOCAL <-> ECEF conversions
    earth::GeoReference geoRef;      // For ENU <-> WGS84 conversions

    ConversionContext() = default;
    ConversionContext(const LocalFrame& lf) : localFrame(lf) {}
    ConversionContext(const earth::GeoReference& gr) : geoRef(gr) {}
    ConversionContext(const LocalFrame& lf, const earth::GeoReference& gr)
        : localFrame(lf), geoRef(gr) {}
};

// =============================================================================
// Low-level Point Conversions
// =============================================================================

/**
 * Convert LOCAL coordinates to ECEF using LocalFrame
 */
GEODRAW_API glm::dvec3 localToECEF(const glm::dvec3& local, const LocalFrame& frame);

/**
 * Convert ECEF coordinates to LOCAL using LocalFrame
 */
GEODRAW_API glm::dvec3 ecefToLocal(const glm::dvec3& ecef, const LocalFrame& frame);

/**
 * Convert ENU coordinates to ECEF using GeoReference
 *
 * Composes the ENU-to-GeoCoord conversion with GeoCoord-to-ECEF.
 */
GEODRAW_API glm::dvec3 enuToECEF(const glm::dvec3& enu, const earth::GeoReference& ref);

/**
 * Convert ECEF coordinates to ENU using GeoReference
 *
 * Composes the ECEF-to-GeoCoord conversion with GeoCoord-to-ENU.
 */
GEODRAW_API glm::dvec3 ecefToENU(const glm::dvec3& ecef, const earth::GeoReference& ref);

/**
 * Create LocalFrame from GeoReference
 *
 * Creates an ENU frame at the GeoReference location, expressed in ECEF.
 * The LocalFrame's rotation matrix columns are the ENU axes in ECEF.
 */
GEODRAW_API LocalFrame fromGeoReference(const earth::GeoReference& ref);

// =============================================================================
// Point Conversion (Generic)
// =============================================================================

/**
 * Convert a single point between coordinate systems
 *
 * All conversions route through ECEF (except WGS84 <-> WEB_MERCATOR).
 *
 * @param point Input point
 * @param from Source coordinate system
 * @param to Target coordinate system
 * @param ctx Conversion context with reference frames
 * @return Converted point
 * @throws std::invalid_argument if source is UNDEFINED
 */
GEODRAW_API Pos3 convertPoint(const Pos3& point, CoordSystem from, CoordSystem to,
                              const ConversionContext& ctx);

// =============================================================================
// Shape/Mesh/BBox Conversion Functions
// =============================================================================

/**
 * Convert Shape3 to target coordinate system
 */
GEODRAW_API Shape3 convertShape(const Shape3& shape, CoordSystem target,
                                const ConversionContext& ctx);

/**
 * Convert Mesh3 to target coordinate system
 */
GEODRAW_API Mesh3 convertMesh(const Mesh3& mesh, CoordSystem target,
                              const ConversionContext& ctx);

/**
 * Convert BBox3 to target coordinate system
 *
 * Note: For coordinate systems with rotation (LOCAL, ENU), this converts the
 * corner points and creates a new axis-aligned bounding box in the target
 * system. This may result in a larger bounding box than the original.
 */
GEODRAW_API BBox3 convertBBox(const BBox3& bbox, CoordSystem target,
                              const ConversionContext& ctx);

} // namespace geodraw
