/*******************************************************************************
 * File : ray.hpp
 *
 * Description : Standalone Ray struct for picking/intersection.
 * Defined here so scene.hpp and ray_intersection.hpp do not need to pull in
 * the full camera.hpp header.
 *
 * Camera::Ray is a type alias for this struct (see camera.hpp).
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-02-22
 *
 *******************************************************************************/

#pragma once

#include <glm/glm.hpp>

namespace geodraw {

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

} // namespace geodraw
