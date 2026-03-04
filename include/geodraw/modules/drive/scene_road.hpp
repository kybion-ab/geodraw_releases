/*******************************************************************************
 * File : scene_road.hpp
 *
 * Description :
 *
 * Author  : Magnus Nilsson
 * Date    : 2026-01-29
 *
 *******************************************************************************/

#pragma once

#include "geodraw/modules/drive/geometry_road.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/colors.hpp"

namespace geodraw {

void addLane(Scene& scene, const Lane& lane,
                    float thickness = 1.0f,
                    LineStyle style = LineStyle::Solid,
                    float alpha = 1.0f,
                    const glm::vec3& color = Colors::WHITE,
                    bool depthTest = true,
                    bool filled = true,
             GLuint textureHandle = 0);

void addRoad(Scene& scene, const Road& road,
                    float thickness = 1.0f,
                    LineStyle style = LineStyle::Solid,
                    float alpha = 1.0f,
                    const glm::vec3& color = glm::vec3(1.0f, 1.0f, 1.0f),
                    bool depthTest = true,
                    bool filled = true,
             GLuint textureHandle = 0);

} // namespace geodraw
