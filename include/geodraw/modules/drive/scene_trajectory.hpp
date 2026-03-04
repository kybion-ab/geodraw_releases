/*******************************************************************************
 * File : scene_trajectory.hpp
 *
 * Description : Free functions to add drive-domain types (RoadLine, RoadEdge)
 * to a Scene. This header exists to keep scene.hpp free of drive-module
 * dependencies. Include this header when you need to render trajectory data.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-02-22
 *
 *******************************************************************************/

#pragma once

#include "geodraw/scene/scene.hpp"
#include "geodraw/modules/drive/trajectory_data.hpp"
#include "geodraw/geometry/colors.hpp"

namespace geodraw {

TooltipBuilder Add(Scene& scene, const RoadLine& roadLine,
                   float thickness = 2.0f,
                   LineStyle style = LineStyle::Dashed,
                   float alpha = 1.0f,
                   const glm::vec3& color = Colors::WHITE,
                   bool depthTest = true);

TooltipBuilder Add(Scene& scene, const RoadEdge& roadEdge,
                   float thickness = 2.0f,
                   LineStyle style = LineStyle::Solid,
                   float alpha = 1.0f,
                   const glm::vec3& color = Colors::RED,
                   bool depthTest = true);

} // namespace geodraw
