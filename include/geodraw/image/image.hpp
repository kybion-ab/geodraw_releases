/*******************************************************************************
 * File: image/image.hpp
 *
 * Description: Image loading, saving, resizing, and format conversion utilities.
 * Provides type-safe RAII wrapper around stb_image family with modern C++ API.
 *
 * Why required: Abstracts low-level image I/O and manipulation into a clean,
 * memory-safe interface. Enables image preprocessing before texture upload,
 * batch processing, and format conversions for data analysis.
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/geometry/geometry.hpp"  // For Polyline3 and Shape3

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace geodraw {
namespace image {

// Pixel format enumeration
enum class PixelFormat {
  GRAYSCALE = 1,  // 1 channel, 8-bit
  RGB = 3,        // 3 channels, 8-bit per channel
  RGBA = 4,       // 4 channels, 8-bit per channel
  FLOAT_RGB = -3  // 3 channels, 32-bit float per channel
};

// Image file format enumeration
enum class ImageFormat {
  PNG,  // Lossless, supports alpha
  JPG,  // Lossy, no alpha, smaller files
  BMP   // Uncompressed, simple format
};

/**
 * Image class - RAII wrapper for image data
 *
 * Supports:
 * - Loading from disk (PNG, JPG, BMP, TGA, HDR, etc.)
 * - Saving to disk (PNG, JPG, BMP)
 * - High-quality resizing
 * - Format conversions (RGB ↔ RGBA ↔ Grayscale ↔ Float)
 * - Pixel access and manipulation
 *
 * Value semantics: Images are copyable and movable.
 * Immutable operations: Methods return new Images.
 */
class GEODRAW_API Image {
public:
  // Constructors
  Image() = default;
  Image(int width, int height, PixelFormat format);

  // Loading and saving
  static std::optional<Image> load(const std::string& path);
  bool save(const std::string& path, ImageFormat format = ImageFormat::PNG) const;

  // Image manipulation (returns new Image)
  Image resize(int newWidth, int newHeight) const;
  Image flipVertical() const;

  // Format conversions (returns new Image)
  Image toGrayscale() const;
  Image toRGB() const;
  Image toRGBA(uint8_t alpha = 255) const;
  Image toFloatRGB() const;  // Normalize to [0.0, 1.0]

  // Accessors
  int width() const { return width_; }
  int height() const { return height_; }
  PixelFormat format() const { return format_; }
  int channels() const;
  size_t sizeBytes() const { return data_.size(); }
  bool empty() const { return data_.empty(); }

  // Raw data access
  const uint8_t* data() const { return data_.data(); }
  const float* dataFloat() const;  // Only valid for FLOAT_RGB

  // Pixel manipulation
  void setPixel(int x, int y, uint8_t r, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255);

private:
  int width_ = 0;
  int height_ = 0;
  PixelFormat format_ = PixelFormat::RGB;
  std::vector<uint8_t> data_;  // Raw pixel data (uint8 or float as bytes)
};

// Helper functions

// Load image with explicit desired channels
GEODRAW_API std::optional<Image> load(const std::string& path,
                                  PixelFormat desiredFormat = PixelFormat::RGB);

// Save with quality parameter (for JPG)
inline bool save(const Image& img, const std::string& path,
                 ImageFormat format = ImageFormat::PNG, int quality = 90);

// Resize maintaining aspect ratio
inline Image resizeKeepAspect(const Image& img, int maxDimension);


GEODRAW_API bool save(const Image& img, const std::string& path,
                      ImageFormat format, int quality);

GEODRAW_API Image resizeKeepAspect(const Image& img, int maxDimension);


/**
 * Options for marching squares contour extraction
 */
typedef struct MarchingSquraresOptions {
  float threshold = 127.5f;       // Threshold value [0-255] for binary classification
  double pixelSize = 1.0;         // Size of each pixel in world units
  double zPosition = 0.0;         // Z coordinate for the resulting 3D polygons
  bool simplify = true;           // Apply Douglas-Peucker simplification
  double simplifyEpsilon = 1.0;   // Tolerance for Douglas-Peucker (in pixels)
} MarchingSquaresOptions;

GEODRAW_API std::pair<double, double> getEdgePoint(int edge, int x, int y);

GEODRAW_API void douglasPeucker(const std::vector<std::pair<double, double>>& points,
                                double epsilon,
                                std::vector<bool>& keep);

GEODRAW_API std::vector<std::pair<double, double>> simplifyPolyline(
                                const std::vector<std::pair<double, double>>& points,
                                double epsilon);

GEODRAW_API std::vector<std::vector<Polyline3>> marchingSquares(
    const Image& img,
    const MarchingSquaresOptions& options = {});

GEODRAW_API Shape3 imageToShape(const Image& img,
                    const MarchingSquaresOptions& options = {});

}} // namespace geodraw::image
