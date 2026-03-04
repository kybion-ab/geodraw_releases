/*******************************************************************************
 * File: image_test.cpp
 *
 * Description: Simple test of the geodraw image module.
 * Tests load, resize, format conversion, and save operations.
 *
 * Uses CATCH_CONFIG_RUNNER so the caller can optionally pass an image path
 * as the first non-flag argument: ./image_test /path/to/image.png
 *
 *******************************************************************************/

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include "geodraw/image/image.hpp"
#include <filesystem>

using namespace geodraw::image;

#ifndef GEODRAW_TEST_IMAGE_PATH
#define GEODRAW_TEST_IMAGE_PATH "../data/images/baboon.png"
#endif

static std::string g_imagePath = GEODRAW_TEST_IMAGE_PATH;

int main(int argc, char** argv) {
    // Accept a custom image path as argv[1] only if the file actually exists
    // (prevents CTest test-name filters from being misinterpreted as paths)
    if (argc >= 2 && argv[1][0] != '-' && std::filesystem::exists(argv[1])) {
        g_imagePath = argv[1];
        for (int i = 1; i < argc - 1; ++i) argv[i] = argv[i + 1];
        --argc;
    }
    return Catch::Session().run(argc, argv);
}

TEST_CASE("Image load") {
    auto img = Image::load(g_imagePath);
    REQUIRE(img.has_value());
    CHECK(img->width() > 0);
    CHECK(img->height() > 0);
    CHECK(img->channels() > 0);
}

TEST_CASE("Image resize") {
    auto img = Image::load(g_imagePath);
    REQUIRE(img.has_value());

    auto resized = img->resize(256, 256);
    CHECK(resized.width() == 256);
    CHECK(resized.height() == 256);
}

TEST_CASE("Image convert to grayscale") {
    auto img = Image::load(g_imagePath);
    REQUIRE(img.has_value());

    auto resized = img->resize(256, 256);
    auto gray = resized.toGrayscale();
    CHECK(gray.channels() == 1);
}

TEST_CASE("Image convert to float RGB") {
    auto img = Image::load(g_imagePath);
    REQUIRE(img.has_value());

    auto resized = img->resize(256, 256);
    auto floatImg = resized.toFloatRGB();
    CHECK(floatImg.format() == PixelFormat::FLOAT_RGB);

    // Average color values should be in [0, 1]
    const float* data = floatImg.dataFloat();
    REQUIRE(data != nullptr);

    int pixels = floatImg.width() * floatImg.height();
    float avgR = 0, avgG = 0, avgB = 0;
    for (int i = 0; i < pixels; ++i) {
        avgR += data[i * 3 + 0];
        avgG += data[i * 3 + 1];
        avgB += data[i * 3 + 2];
    }
    avgR /= pixels; avgG /= pixels; avgB /= pixels;
    CHECK(avgR >= 0.0f); CHECK(avgR <= 1.0f);
    CHECK(avgG >= 0.0f); CHECK(avgG <= 1.0f);
    CHECK(avgB >= 0.0f); CHECK(avgB <= 1.0f);
}

TEST_CASE("Image save PNG and JPG") {
    auto img = Image::load(g_imagePath);
    REQUIRE(img.has_value());

    auto resized = img->resize(256, 256);
    CHECK(resized.save("output_resized.png"));

    auto gray = resized.toGrayscale();
    CHECK(gray.save("output_gray.png"));

    CHECK(resized.save("output_resized.jpg", ImageFormat::JPG));
}
