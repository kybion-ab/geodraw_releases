// STB and TinyGLTF implementation unit.
// Compile this file into any target that uses tinygltf or stb directly
// (i.e. not through the geodraw API).  These symbols have hidden visibility
// inside libgeodraw, so callers that use the headers directly must supply
// their own translation unit.
#include <nlohmann/json.hpp>
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "geodraw/external/tinygltf/tiny_gltf.h"
