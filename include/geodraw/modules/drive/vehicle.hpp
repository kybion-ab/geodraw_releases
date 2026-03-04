/*******************************************************************************
 * File : vehicle.hpp
 *
 * Description : Vehicle representation and rendering utilities.
 *
 * Author  : Magnus Nilsson
 * Date    : 2026-01-29
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/geometry/geometry_transforms.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/modules/gltf/gltf_loader.hpp"

namespace geodraw {

// Vehicle representation with pose, dynamics, and dimensions
struct Vehicle {
    Pos3 position;
    glm::mat3 orientation;  // Rotation matrix (columns: right, forward, up)
    glm::dvec3 velocity;
    double length;
    double width;
    double height;
    glm::vec3 color;

    RBox3 toRBox3() const {
        BBox3 localBox{
            Pos3{-length/2, -width/2, 0},
            Pos3{length/2, width/2, height}
        };
        Pose3 pose;
        pose.t = position;
        pose.R = orientation;
        return RBox3{pose, localBox};
    }
};

GEODRAW_API void addVehicle(Scene &scene,
                       const Vehicle &vehicle,
                       const gltf::LoadedModel *glbModel = nullptr,
                       float thickness = 2.0f,
                       LineStyle style = LineStyle::Solid,
                       float alpha = 1.0f,
                       bool depthTest = true,
                       uint64_t objectId = 0,
                       const std::string& objectType = "");


} // namespace geodraw
