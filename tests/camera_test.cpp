/*******************************************************************************
 * File: camera_test.cpp
 *
 * Description: Unit tests for the Camera class.
 * Tests orbit mode, globe mode, ray casting, and projection/unprojection.
 *
 ******************************************************************************/

#include "geodraw/camera/camera.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <glm/gtc/constants.hpp>

using namespace geodraw;
using Catch::Approx;

static bool approxEqual(float a, float b, float tolerance = 1e-4f) {
    return std::abs(a - b) < tolerance;
}

static bool approxEqual(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) < tolerance;
}

static bool approxEqual(const glm::vec3& a, const glm::vec3& b, float tolerance = 1e-4f) {
    return approxEqual(a.x, b.x, tolerance) && approxEqual(a.y, b.y, tolerance) &&
           approxEqual(a.z, b.z, tolerance);
}

static bool approxEqual(const glm::dvec3& a, const glm::dvec3& b, double tolerance = 1e-3) {
    return approxEqual(a.x, b.x, tolerance) && approxEqual(a.y, b.y, tolerance) &&
           approxEqual(a.z, b.z, tolerance);
}

TEST_CASE("Default camera construction") {
    Camera cam;
    CHECK(cam.getMode() == Camera::CameraMode::ORBIT);
    CHECK(approxEqual(cam.getTarget(), glm::vec3(0.0f)));
}

TEST_CASE("Orbit mode - view matrix looks at target") {
    Camera cam;
    cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    cam.distance = 10.0f;
    cam.orbitYaw = 0.0f;
    cam.orbitPitch = 0.0f;

    glm::mat4 view = cam.getViewMatrix();
    glm::vec4 targetInView = view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    CHECK(targetInView.z < 0.0f); // Target in front of camera (negative Z in view space)
    CHECK(approxEqual(targetInView.x, 0.0f, 0.1f));
    CHECK(approxEqual(targetInView.y, 0.0f, 0.1f));
}

TEST_CASE("Orbit rotation changes yaw/pitch") {
    Camera cam;
    cam.orbitYaw = glm::radians(30.0f);
    cam.orbitPitch = glm::radians(30.0f);
    cam.rotationSpeed = 1.0f;

    float initialYaw = cam.orbitYaw;
    float initialPitch = cam.orbitPitch;

    cam.orbitRotate(10.0f, 5.0f);

    CHECK(cam.orbitYaw < initialYaw);   // Yaw should decrease
    CHECK(cam.orbitPitch < initialPitch); // Pitch should decrease
}

TEST_CASE("Orbit pitch clamping at +-89 degrees") {
    Camera cam;
    cam.rotationSpeed = 1.0f;

    cam.orbitPitch = glm::radians(80.0f);
    cam.orbitRotate(0.0f, -20.0f); // Push pitch toward 100
    CHECK(cam.orbitPitch <= glm::radians(89.0f));

    cam.orbitPitch = glm::radians(-80.0f);
    cam.orbitRotate(0.0f, 20.0f); // Push pitch toward -100
    CHECK(cam.orbitPitch >= glm::radians(-89.0f));
}

TEST_CASE("Orbit zoom changes distance with clamping") {
    Camera cam;
    cam.distance = 10.0f;
    cam.minZoomDistance = 1.0f;
    cam.maxZoomDistance = 100.0f;
    cam.zoomSpeed = 0.01f;

    float initialDistance = cam.distance;
    cam.orbitZoom(100.0f);
    CHECK(cam.distance < initialDistance); // Zoomed in

    cam.orbitZoom(-10000.0f);
    CHECK(cam.distance <= cam.maxZoomDistance); // Clamped at max

    cam.orbitZoom(100000.0f);
    CHECK(cam.distance >= cam.minZoomDistance); // Clamped at min
}

TEST_CASE("Orbit pan moves target") {
    Camera cam;
    cam.distance = 10.0f;
    cam.orbitYaw = glm::radians(30.0f);
    cam.orbitPitch = glm::radians(30.0f);
    cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    cam.panSpeed = 0.1f;

    glm::vec3 initialTarget = cam.getTarget();
    cam.orbitPan(10.0f, 10.0f, 768.0f);
    glm::vec3 newTarget = cam.getTarget();

    CHECK(!approxEqual(initialTarget, newTarget, 0.001f));
    CHECK(!std::isnan(newTarget.x));
    CHECK(!std::isnan(newTarget.y));
    CHECK(!std::isnan(newTarget.z));
}

TEST_CASE("Auto-frame computes appropriate distance") {
    Camera cam;

    glm::vec3 bboxMin(-5.0f, -5.0f, -5.0f);
    glm::vec3 bboxMax(5.0f, 5.0f, 5.0f);

    cam.autoFrame(bboxMin, bboxMax, glm::radians(45.0f), glm::radians(30.0f));

    glm::vec3 expectedCenter = (bboxMin + bboxMax) * 0.5f;
    CHECK(approxEqual(cam.getTarget(), expectedCenter));
    CHECK(cam.distance > 1.0f);
    CHECK(cam.distance < 100.0f);
    CHECK(approxEqual(cam.orbitYaw, glm::radians(45.0f)));
    CHECK(approxEqual(cam.orbitPitch, glm::radians(30.0f)));
}

TEST_CASE("Mode switching") {
    Camera cam;
    CHECK(cam.getMode() == Camera::CameraMode::ORBIT);

    cam.setMode(Camera::CameraMode::GLOBE);
    CHECK(cam.getMode() == Camera::CameraMode::GLOBE);

    cam.setMode(Camera::CameraMode::ORBIT);
    CHECK(cam.getMode() == Camera::CameraMode::ORBIT);
}

TEST_CASE("Globe mode - ECEF position at equator/prime meridian") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);
    cam.setGlobeTarget(0.0, 0.0);
    cam.setGlobeAltitude(1000000.0);
    cam.globePitch = glm::half_pi<float>();

    glm::dvec3 targetECEF = cam.getGlobeTargetECEF();
    glm::dvec3 cameraECEF = cam.getGlobeCameraECEF();

    double WGS84_A = 6378137.0;
    CHECK(std::abs(targetECEF.x - WGS84_A) < 100.0);
    CHECK(std::abs(targetECEF.y) < 100.0);
    CHECK(std::abs(targetECEF.z) < 100.0);
    CHECK(glm::length(cameraECEF) > glm::length(targetECEF)); // Camera above target
}

TEST_CASE("Globe mode - altitude clamping") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);
    cam.minGlobeAltitude = 100.0;
    cam.maxGlobeAltitude = 50000000.0;

    cam.setGlobeAltitude(10.0); // Below minimum
    CHECK(cam.globeAltitude >= cam.minGlobeAltitude);

    cam.setGlobeAltitude(100000000.0); // Above maximum
    CHECK(cam.globeAltitude <= cam.maxGlobeAltitude);
}

TEST_CASE("Globe mode - latitude clamping") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);

    cam.setGlobeTarget(100.0, 0.0); // Beyond 85
    CHECK(cam.globeTargetLat <= 85.0);

    cam.setGlobeTarget(-100.0, 0.0); // Below -85
    CHECK(cam.globeTargetLat >= -85.0);
}

TEST_CASE("Globe mode - longitude normalization") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);

    cam.setGlobeTarget(0.0, 200.0); // Should wrap to -160
    CHECK(cam.globeTargetLon >= -180.0);
    CHECK(cam.globeTargetLon <= 180.0);

    cam.setGlobeTarget(0.0, -200.0); // Should wrap to 160
    CHECK(cam.globeTargetLon >= -180.0);
    CHECK(cam.globeTargetLon <= 180.0);
}

TEST_CASE("Globe pitch clamping (5 to 90 degrees)") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);
    cam.rotationSpeed = 1.0f;

    cam.globePitch = glm::radians(10.0f);
    cam.globeRotate(0.0f, 10.0f); // Push toward horizon
    CHECK(cam.globePitch >= glm::radians(5.0f));

    cam.globePitch = glm::radians(85.0f);
    cam.globeRotate(0.0f, -10.0f); // Push toward zenith
    CHECK(cam.globePitch <= glm::half_pi<float>());
}

TEST_CASE("Ray from screen center points toward target") {
    Camera cam;
    cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    cam.distance = 10.0f;
    cam.orbitYaw = 0.0f;
    cam.orbitPitch = 0.0f;

    float width = 800.0f, height = 600.0f;
    Camera::Ray ray = cam.getRayFromScreenPos(width / 2.0f, height / 2.0f, width, height);

    glm::vec3 toTarget = glm::normalize(cam.getTarget() - ray.origin);
    float dot = glm::dot(ray.direction, toTarget);
    CHECK(dot > 0.99f);
}

TEST_CASE("Project/unproject round-trip") {
    Camera cam;
    cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    cam.distance = 10.0f;
    cam.orbitYaw = glm::radians(30.0f);
    cam.orbitPitch = glm::radians(30.0f);

    float width = 800.0f, height = 600.0f;
    glm::vec3 worldPoint(1.0f, 1.0f, 1.0f);

    auto projected = cam.projectToScreen(worldPoint, width, height);
    REQUIRE(projected.has_value());

    glm::vec3 screenPos = *projected;
    glm::vec3 unprojected = cam.unprojectScreenPoint(screenPos.x, screenPos.y, screenPos.z, width, height);
    CHECK(approxEqual(worldPoint, unprojected, 0.01f));
}

TEST_CASE("Project returns nullopt for point behind camera") {
    Camera cam;
    cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    cam.distance = 10.0f;
    cam.orbitYaw = 0.0f;
    cam.orbitPitch = 0.0f;

    float width = 800.0f, height = 600.0f;
    // With yaw=0, pitch=0, camera is at positive X; point at X=20 is behind camera
    glm::vec3 behindCamera(20.0f, 0.0f, 0.0f);
    CHECK(!cam.projectToScreen(behindCamera, width, height).has_value());
}

TEST_CASE("Projection matrix aspect ratio") {
    Camera cam;

    glm::mat4 proj16_9 = cam.getProjectionMatrix(16.0f / 9.0f);
    glm::mat4 proj4_3 = cam.getProjectionMatrix(4.0f / 3.0f);

    CHECK(!approxEqual(proj16_9[0][0], proj4_3[0][0])); // Different X scaling
    CHECK(approxEqual(proj16_9[1][1], proj4_3[1][1]));  // Same Y scaling
}

TEST_CASE("Globe mode ray from screen center") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);
    cam.setGlobeTarget(0.0, 0.0);
    cam.setGlobeAltitude(10000000.0);
    cam.globePitch = glm::half_pi<float>(); // Looking straight down

    float width = 800.0f, height = 600.0f;
    Camera::Ray ray = cam.getRayFromScreenPos(width / 2.0f, height / 2.0f, width, height);

    glm::dvec3 cameraECEF = cam.getGlobeCameraECEF();
    glm::vec3 toCenter = glm::normalize(-glm::vec3(cameraECEF));
    float dot = glm::dot(ray.direction, toCenter);
    CHECK(dot > 0.9f);
}

TEST_CASE("Globe view matrix puts target in front") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);
    cam.setGlobeTarget(45.0, 90.0);
    cam.setGlobeAltitude(1000000.0);
    cam.globePitch = glm::radians(45.0f);

    glm::mat4 view = cam.getGlobeViewMatrix();
    glm::vec4 targetInView = view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    CHECK(targetInView.z < 0.0f); // Target in front of camera
}

TEST_CASE("Camera vectors are orthonormal (orbit mode)") {
    Camera cam;
    cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    cam.distance = 10.0f;
    cam.orbitYaw = glm::radians(45.0f);
    cam.orbitPitch = glm::radians(30.0f);
    cam.orbitRotate(0.0f, 0.0f); // Force update

    CHECK(std::abs(glm::dot(cam.front, cam.right)) < 0.001f);
    CHECK(std::abs(glm::dot(cam.front, cam.up)) < 0.001f);
    CHECK(std::abs(glm::dot(cam.right, cam.up)) < 0.001f);

    CHECK(approxEqual(glm::length(cam.front), 1.0f));
    CHECK(approxEqual(glm::length(cam.right), 1.0f));
    CHECK(approxEqual(glm::length(cam.up), 1.0f));
}

TEST_CASE("setTarget updates camera position") {
    Camera cam;
    cam.distance = 10.0f;
    cam.orbitYaw = 0.0f;
    cam.orbitPitch = 0.0f;

    cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    glm::vec3 pos1 = cam.position;

    cam.setTarget(glm::vec3(10.0f, 10.0f, 0.0f));
    glm::vec3 pos2 = cam.position;

    CHECK(!approxEqual(pos1, pos2, 0.001f));

    float dist1 = glm::length(pos1 - glm::vec3(0.0f));
    float dist2 = glm::length(pos2 - glm::vec3(10.0f, 10.0f, 0.0f));
    CHECK(approxEqual(dist1, 10.0f, 0.01f));
    CHECK(approxEqual(dist2, 10.0f, 0.01f));
}

TEST_CASE("Globe zoom changes altitude") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);
    cam.setGlobeAltitude(1000000.0);
    double initialAlt = cam.globeAltitude;

    cam.globeZoom(100.0f);
    CHECK(cam.globeAltitude < initialAlt);

    cam.setGlobeAltitude(1000000.0);
    cam.globeZoom(-100.0f);
    CHECK(cam.globeAltitude > initialAlt);
}

TEST_CASE("Globe pan moves target lat/lon") {
    Camera cam;
    cam.setMode(Camera::CameraMode::GLOBE);
    cam.setGlobeTarget(45.0, 90.0);
    cam.setGlobeAltitude(1000000.0);
    cam.panSpeed = 0.1f;

    double initialLat = cam.globeTargetLat;
    double initialLon = cam.globeTargetLon;

    cam.globePan(10.0f, 10.0f, 728);

    CHECK(!approxEqual(cam.globeTargetLat, initialLat, 0.001));
    CHECK(!approxEqual(cam.globeTargetLon, initialLon, 0.001));
}

TEST_CASE("ECEF position getter/setter") {
    Camera cam;
    glm::dvec3 ecef(1000000.0, 2000000.0, 3000000.0);

    cam.setECEFPosition(ecef);
    glm::dvec3 retrieved = cam.getECEFPosition();

    CHECK(approxEqual(ecef, retrieved));
}
