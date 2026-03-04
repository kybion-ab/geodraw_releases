/*******************************************************************************
 * File: marching_squares_test.cpp
 *
 * Description: Demonstrates marching squares algorithm for image to shape conversion
 * Creates synthetic test images and extracts polygons using contour tracing
 *
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/image/image.hpp"
#include <iostream>
#include <cmath>

using namespace geodraw;
using namespace geodraw::image;

// Helper function to create a test image with various shapes
Image createTestImage(int width, int height) {
  Image img(width, height, PixelFormat::GRAYSCALE);

  // Fill with black background
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      img.setPixel(x, y, 0, 0, 0);
    }
  }

  // Draw a large circle
  int cx1 = width / 4;
  int cy1 = height / 2;
  int r1 = width / 6;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int dx = x - cx1;
      int dy = y - cy1;
      if (dx*dx + dy*dy < r1*r1) {
        img.setPixel(x, y, 255, 255, 255);
      }
    }
  }

  // Draw a square with a circular hole (donut shape on right side)
  int cx2 = 3 * width / 4;
  int cy2 = height / 2;
  int outerR = width / 5;
  int innerR = width / 10;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int dx = x - cx2;
      int dy = y - cy2;
      int distSq = dx*dx + dy*dy;
      if (distSq < outerR*outerR && distSq > innerR*innerR) {
        img.setPixel(x, y, 255, 255, 255);
      }
    }
  }

  // Draw a triangle at the top
  int tx1 = width / 2 - width / 10;
  int tx2 = width / 2 + width / 10;
  int tx3 = width / 2;
  int ty1 = height / 6 + height / 10;
  int ty2 = height / 6 + height / 10;
  int ty3 = height / 6 - height / 15;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // Simple triangle fill using barycentric coordinates
      auto sign = [](int x1, int y1, int x2, int y2, int x3, int y3) {
        return (x1 - x3) * (y2 - y3) - (x2 - x3) * (y1 - y3);
      };

      int d1 = sign(x, y, tx1, ty1, tx2, ty2);
      int d2 = sign(x, y, tx2, ty2, tx3, ty3);
      int d3 = sign(x, y, tx3, ty3, tx1, ty1);

      bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
      bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

      if (!(hasNeg && hasPos)) {
        img.setPixel(x, y, 255, 255, 255);
      }
    }
  }

  return img;
}

struct MarchingSquaresGui {
  Image sourceImage;
  Shape3 extractedShape;
  MarchingSquaresOptions msOptions;
  bool showImage = true;
  bool showContours = true;
  bool showFilled = false;
  float threshold = 127.5f;
  float simplifyEpsilon = 1.0f;
  bool simplifyEnabled = true;

  void init() {
    // Create test image
    sourceImage = createTestImage(200, 150);

    // Initial marching squares extraction
    updateExtraction();
  }

  void updateExtraction() {
    msOptions.threshold = threshold;
    msOptions.pixelSize = 0.1;  // Scale down to reasonable world units
    msOptions.zPosition = 0.0;
    msOptions.simplify = simplifyEnabled;
    msOptions.simplifyEpsilon = simplifyEpsilon;

    extractedShape = imageToShape(sourceImage, msOptions);

    // Print statistics
    std::cout << "Extracted " << extractedShape.numPolygons() << " polygons\n";
    for (size_t i = 0; i < extractedShape.numPolygons(); ++i) {
      if (!extractedShape.polygons[i].empty()) {
        std::cout << "  Polygon " << i << ": "
                  << extractedShape.polygons[i][0].points.size() << " vertices\n";
      }
    }
  }

  void updateGeometry(Scene& queue) {
    queue.clear();

    // Draw extracted contours
    if (showContours && extractedShape.numPolygons() > 0) {
      // Offset to center the shape
      double offsetX = -sourceImage.width() * msOptions.pixelSize / 2.0;
      double offsetY = -sourceImage.height() * msOptions.pixelSize / 2.0;

      // Render the shape
      Shape3 centeredShape = extractedShape;
      for (auto& polygon : centeredShape.polygons) {
        for (auto& ring : polygon) {
          for (auto& pt : ring.points) {
            pt.pos.x += offsetX;
            pt.pos.y += offsetY;
          }
        }
      }

      glm::vec3 color = showFilled ? glm::vec3(0.3f, 0.7f, 1.0f) : glm::vec3(1.0f, 0.5f, 0.2f);
      queue.Add(centeredShape, 2.0f, LineStyle::Solid, 0.9f, color, true, showFilled);
    }
  }
};

int main() {
  try {
    App app(1200, 800, "Marching Squares Test");

    MarchingSquaresGui gui;
    gui.init();

    // Auto-frame camera to view the extracted shape
    app.camera.setMode(Camera::CameraMode::ORBIT);

    // Compute bounding box
    glm::dvec3 bboxMin(1e30);
    glm::dvec3 bboxMax(-1e30);

    double offsetX = -gui.sourceImage.width() * gui.msOptions.pixelSize / 2.0;
    double offsetY = -gui.sourceImage.height() * gui.msOptions.pixelSize / 2.0;

    for (const auto& polygon : gui.extractedShape.polygons) {
      for (const auto& ring : polygon) {
        for (const auto& pt : ring.points) {
          glm::dvec3 pos = pt.pos;
          pos.x += offsetX;
          pos.y += offsetY;
          bboxMin = glm::min(bboxMin, pos);
          bboxMax = glm::max(bboxMax, pos);
        }
      }
    }

    if (bboxMin.x < bboxMax.x) {
      app.camera.autoFrame(glm::vec3(bboxMin), glm::vec3(bboxMax), glm::radians(30.0f), glm::radians(45.0f));
    }

    // Register controls
    app.addToggle("", gui.showFilled, "Toggle filled/wireframe rendering", GLFW_KEY_F);
    app.addToggle("", gui.showContours, "Toggle contour visibility"      , GLFW_KEY_C);

    app.addCmd("increase-threshold", [&]() {
      gui.threshold = std::min(255.0f, gui.threshold + 10.0f);
      gui.updateExtraction();
      std::cout << "Threshold: " << gui.threshold << "\n";
    }, "Increase threshold (+10)", GLFW_KEY_UP);

    app.addCmd("decrease-threshold", [&]() {
      gui.threshold = std::max(0.0f, gui.threshold - 10.0f);
      gui.updateExtraction();
      std::cout << "Threshold: " << gui.threshold << "\n";
    }, "Decrease threshold (-10)", GLFW_KEY_DOWN);

    app.addCmd("increase-epsilon", [&]() {
      gui.simplifyEpsilon = std::min(10.0f, gui.simplifyEpsilon + 0.5f);
      gui.updateExtraction();
      std::cout << "Simplify epsilon: " << gui.simplifyEpsilon << "\n";
    }, "Increase simplification (+0.5)", GLFW_KEY_RIGHT);

    app.addCmd("decrease-epsilon", [&]() {
      gui.simplifyEpsilon = std::max(0.0f, gui.simplifyEpsilon - 0.5f);
      gui.updateExtraction();
      std::cout << "Simplify epsilon: " << gui.simplifyEpsilon << "\n";
    }, "Decrease simplification (-0.5)", GLFW_KEY_LEFT);

    app.addCmd("toggle-simplification", [&]() {
      gui.simplifyEnabled = !gui.simplifyEnabled;
      gui.updateExtraction();
      std::cout << "Simplification " << (gui.simplifyEnabled ? "enabled" : "disabled") << "\n";
    }, "Toggle Douglas-Peucker simplification", GLFW_KEY_S);

    app.setDocstring(R"(=== Marching Squares Test ===

Demonstrates image to polygon conversion using marching squares algorithm.
Test image contains: circle (solid), donut (with hole), and triangle.

Workflow:
  • Adjust threshold to control polygon extraction sensitivity
  • Toggle Douglas-Peucker simplification to reduce vertex count
  • Adjust epsilon to control simplification aggressiveness
  • Switch between filled/wireframe to visualize polygon structure
  • Toggle contours to see extracted boundary lines)");

    static Scene mainQueue;
    app.addUpdateCallback([&](float dt) {
      gui.updateGeometry(mainQueue);
    });

    app.addDrawCallback([&]() {
      app.getRenderer().render(mainQueue, app.camera);
    });

    app.run();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
