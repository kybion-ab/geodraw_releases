/*******************************************************************************
 * Demo: GLB Model Viewer
 *
 * Interactive viewer for glTF 2.0 binary (.glb) 3D models.
 * Loads all .glb files from data/glb_files/ and lets you switch between them.
 *
 * Controls:
 *   LEFT/RIGHT ARROW - Switch between models
 *   1-8              - Select model directly
 *   W                - Toggle wireframe mode
 *   B                - Toggle bounding box display
 *   +/-              - Adjust model transparency
 *
 * Run from the build directory:
 *   ./bin/demo_glb_viewer
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/colors.hpp"
#include "geodraw/modules/gltf/gltf_loader.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>

using namespace geodraw;

/**
 * GlbViewerState - Application state for the GLB model viewer
 *
 * Manages collection of loaded models, display settings, and the scene queue.
 */
struct GlbViewerState {
    std::vector<gltf::LoadedModel> models;
    std::vector<std::string> modelPaths;
    int currentModelIndex = 0;

    // Display options
    bool wireframeMode = false;
    bool showBounds = true;
    float modelAlpha = 1.0f;

    // Scene queue owned here (not static)
    Scene mainQueue;

    bool hasModels() const { return !models.empty(); }
    gltf::LoadedModel& currentModel() { return models[currentModelIndex]; }
    const gltf::LoadedModel& currentModel() const { return models[currentModelIndex]; }
};

std::vector<std::string> findGlbFiles(const std::string& dirPath) {
    std::vector<std::string> glbFiles;

    try {
        if (!std::filesystem::exists(dirPath)) {
            std::cerr << "Directory not found: " << dirPath << "\n";
            return glbFiles;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                if (path.size() >= 4 && path.substr(path.size() - 4) == ".glb") {
                    glbFiles.push_back(path);
                }
            }
        }

        std::sort(glbFiles.begin(), glbFiles.end());

    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << "\n";
    }

    return glbFiles;
}

void loadAllModels(GlbViewerState& state, const std::string& dirPath, Renderer& renderer) {
    state.modelPaths = findGlbFiles(dirPath);

    if (state.modelPaths.empty()) {
        std::cerr << "No .glb files found in: " << dirPath << "\n";
        return;
    }

    std::cout << "Found " << state.modelPaths.size() << " .glb files\n";
    std::cout << "Loading models...\n";

    for (const auto& path : state.modelPaths) {
        auto model = gltf::loadGLB(path, renderer);
        if (model) {
            state.models.push_back(std::move(*model));
        } else {
            std::cerr << "Failed to load: " << path << "\n";
            state.models.push_back(gltf::LoadedModel{});
        }
    }

    std::cout << "Successfully loaded " << state.models.size() << " models\n";
}

void cleanupAllModels(GlbViewerState& state, Renderer& renderer) {
    for (auto& model : state.models) {
        if (model.isValid()) {
            gltf::unloadModel(model, renderer);
        }
    }
    state.models.clear();
    state.modelPaths.clear();
}

void updateGeometry(const GlbViewerState& state, Scene& queue) {
    queue.clear();

    if (!state.hasModels()) return;

    const auto& model = state.currentModel();
    if (!model.isValid()) return;

    for (size_t i = 0; i < model.meshes.size(); ++i) {
        GLuint tex = i < model.textureHandles.size() ? model.textureHandles[i] : 0;
        glm::vec3 color = i < model.colors.size() ? model.colors[i] : glm::vec3(1.0f);

        queue.Add(model.meshes[i],
                 /*thickness=*/1.0f,
                 LineStyle::Solid,
                 /*alpha=*/state.modelAlpha,
                 /*color=*/color,
                 /*depthTest=*/true,
                 /*filled=*/!state.wireframeMode,
                 /*textureHandle=*/tex);
    }

    if (state.showBounds) {
        queue.Add(model.bounds,
                  /*thickness=*/2.0f,
                 LineStyle::Solid,
                 /*alpha=*/1.0f,
                 /*color=*/Colors::GREEN,
                 /*depthTest=*/true, /*filled=*/false);
    }
}

void nextModel(GlbViewerState& state) {
    if (!state.hasModels()) return;
    state.currentModelIndex = (state.currentModelIndex + 1) % state.models.size();
    std::cout << "Switched to model: " << state.currentModel().name << "\n";
}

void prevModel(GlbViewerState& state) {
    if (!state.hasModels()) return;
    state.currentModelIndex--;
    if (state.currentModelIndex < 0) {
        state.currentModelIndex = state.models.size() - 1;
    }
    std::cout << "Switched to model: " << state.currentModel().name << "\n";
}

void selectModel(GlbViewerState& state, int index) {
    if (!state.hasModels()) return;
    int idx = index - 1;
    if (idx >= 0 && idx < static_cast<int>(state.models.size())) {
        state.currentModelIndex = idx;
        std::cout << "Selected model " << index << ": " << state.currentModel().name << "\n";
    }
}

void adjustAlpha(GlbViewerState& state, float delta) {
    state.modelAlpha = std::clamp(state.modelAlpha + delta, 0.0f, 1.0f);
    std::cout << "Model alpha: " << state.modelAlpha << "\n";
}

int main() {
    App app(1280, 720, "Demo GLB Viewer");

    GlbViewerState state;

    const char *root = getenv("GEODRAW_ROOT");
    if(!root){
      throw std::runtime_error("GEODRAW_ROOT not set");
    }
    std::string glb_path = std::string(root) + "/data/glb_files";

    loadAllModels(state, glb_path, app.getRenderer());

    if (!state.hasModels()) {
      std::cerr << "Error: No .glb models could be loaded from " << glb_path << std::endl;
        std::cerr << "Please ensure .glb files exist in " << glb_path << std::endl;
        return 1;
    }

    app.addCmd("next-model", [&](){ nextModel(state); },
               "Switch to next model", GLFW_KEY_RIGHT);
    app.addCmd("prev-model", [&](){ prevModel(state); },
               "Switch to previous model", GLFW_KEY_LEFT);

    app.addCmd("select-model-1", [&](){ selectModel(state, 1); }, "Select model 1", GLFW_KEY_1);
    app.addCmd("select-model-2", [&](){ selectModel(state, 2); }, "Select model 2", GLFW_KEY_2);
    app.addCmd("select-model-3", [&](){ selectModel(state, 3); }, "Select model 3", GLFW_KEY_3);
    app.addCmd("select-model-4", [&](){ selectModel(state, 4); }, "Select model 4", GLFW_KEY_4);
    app.addCmd("select-model-5", [&](){ selectModel(state, 5); }, "Select model 5", GLFW_KEY_5);
    app.addCmd("select-model-6", [&](){ selectModel(state, 6); }, "Select model 6", GLFW_KEY_6);
    app.addCmd("select-model-7", [&](){ selectModel(state, 7); }, "Select model 7", GLFW_KEY_7);
    app.addCmd("select-model-8", [&](){ selectModel(state, 8); }, "Select model 8", GLFW_KEY_8);

    app.addToggle("", state.wireframeMode, "Toggle wireframe mode", GLFW_KEY_W);
    app.addToggle("", state.showBounds,    "Toggle bounding box",   GLFW_KEY_B);

    app.addCmd("increase-alpha", [&](){ adjustAlpha(state,  0.1f); }, "Increase transparency", GLFW_KEY_EQUAL);
    app.addCmd("decrease-alpha", [&](){ adjustAlpha(state, -0.1f); }, "Decrease transparency", GLFW_KEY_MINUS);

    // Frame camera to show the first model
    {
        const auto& bounds = state.currentModel().bounds;
        glm::vec3 minPt(bounds.min.pos.x, bounds.min.pos.y, bounds.min.pos.z);
        glm::vec3 maxPt(bounds.max.pos.x, bounds.max.pos.y, bounds.max.pos.z);
        app.camera.autoFrame(minPt, maxPt, glm::radians(-60.0f), glm::radians(30.0f));
    }

    app.requestContinuousUpdate("glb_viewer.display");

    app.addUpdateCallback([&](float /*dt*/) {
        updateGeometry(state, state.mainQueue);
    });

    app.addDrawCallback([&]() {
        app.getRenderer().render(state.mainQueue, app.camera);

        if (state.hasModels()) {
            const auto& model = state.currentModel();
            std::string info = "Model: " + model.name + " (" +
                             std::to_string(state.currentModelIndex + 1) + "/" +
                             std::to_string(state.models.size()) + ")";
            app.getRenderer().renderText(info, 10, 690, glm::vec3(1.0f, 1.0f, 1.0f));

            std::string meshInfo = "Meshes: " + std::to_string(model.meshes.size()) +
                                 "  Textures: " + std::to_string(model.textureHandles.size());
            app.getRenderer().renderText(meshInfo, 10, 670, glm::vec3(0.8f, 0.8f, 0.8f));

            if (state.wireframeMode) {
                app.getRenderer().renderText("Wireframe Mode", 10, 650, glm::vec3(1.0f, 0.5f, 0.0f));
            }
        }
    });

    app.run();

    cleanupAllModels(state, app.getRenderer());

    return 0;
}
