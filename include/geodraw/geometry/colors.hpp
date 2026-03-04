/*******************************************************************************
 * File : colors.hpp
 *
 * Description : Provides a convenient way to specify common colors while still
 * supporting glm::vec3 internally.
 *
 * Author  : Magnus Nilsson
 * Date    : 2026-01-27
 *
 *******************************************************************************/

#pragma once
#include <glm/vec3.hpp>
#include <unordered_map>
#include <string>

namespace Colors {

    // Basic colors
    inline constexpr glm::vec3 WHITE       {1.0f, 1.0f, 1.0f};
    inline constexpr glm::vec3 BLACK       {0.0f, 0.0f, 0.0f};
    inline constexpr glm::vec3 RED         {0.8f, 0.2f, 0.2f};
    inline constexpr glm::vec3 GREEN       {0.2f, 0.8f, 0.2f};
    inline constexpr glm::vec3 BLUE        {0.2f, 0.2f, 0.8f};
    inline constexpr glm::vec3 YELLOW      {0.9f, 0.9f, 0.2f};
    inline constexpr glm::vec3 CYAN        {0.2f, 0.8f, 0.8f};
    inline constexpr glm::vec3 MAGENTA     {0.8f, 0.2f, 0.8f};
    inline constexpr glm::vec3 ORANGE      {0.95f, 0.5f, 0.0f};
    inline constexpr glm::vec3 PURPLE      {0.6f, 0.2f, 0.8f};
    inline constexpr glm::vec3 PINK        {0.95f, 0.5f, 0.7f};
    inline constexpr glm::vec3 BROWN       {0.6f, 0.4f, 0.2f};
    inline constexpr glm::vec3 LIGHT_GRAY  {0.8f, 0.8f, 0.8f};
    inline constexpr glm::vec3 DARK_GRAY   {0.3f, 0.3f, 0.3f};
    inline constexpr glm::vec3 OLIVE       {0.5f, 0.5f, 0.2f};
    inline constexpr glm::vec3 TEAL        {0.2f, 0.5f, 0.5f};
    inline constexpr glm::vec3 NAVY        {0.2f, 0.2f, 0.5f};
    inline constexpr glm::vec3 LIME        {0.5f, 1.0f, 0.2f};
    inline constexpr glm::vec3 GOLD        {0.9f, 0.8f, 0.2f};
    inline constexpr glm::vec3 SILVER      {0.8f, 0.8f, 0.9f};

    // Optional string lookup for dynamic use
    inline const std::unordered_map<std::string, glm::vec3> MAP = {
        {"white", WHITE},   {"black", BLACK},
        {"red", RED},       {"green", GREEN}, {"blue", BLUE},
        {"yellow", YELLOW}, {"cyan", CYAN},   {"magenta", MAGENTA},
        {"orange", ORANGE}, {"purple", PURPLE},{"pink", PINK},
        {"brown", BROWN},   {"light_gray", LIGHT_GRAY}, {"dark_gray", DARK_GRAY},
        {"olive", OLIVE},   {"teal", TEAL}, {"navy", NAVY},
        {"lime", LIME},     {"gold", GOLD}, {"silver", SILVER}
    };
}
