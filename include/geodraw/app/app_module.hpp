/*******************************************************************************
 * File: app_module.hpp
 *
 * Description: IAppModule interface for composable application modules.
 *
 * Modules implement attach() to register update/draw callbacks, keybindings,
 * and minor modes with the App. This allows app logic to be composed without
 * duplication:
 *
 *   App app;
 *   EarthModule earth;
 *   ScenarioModule scenario;
 *   app.addModule(earth);
 *   app.addModule(scenario);
 *   app.run();
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-02-20
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

namespace geodraw {

// Forward declaration
class App;

/// Priority ordering for multi-callback execution (lower = earlier).
namespace Priority {
    constexpr int BASE      = 0;   ///< Scene owner: clears and adds base geometry
    constexpr int OVERLAY   = 10;  ///< Secondary layers: add geometry on top
    constexpr int BEFORE_UI = 15;  ///< After overlays, before ImGui (e.g. video capture without ImGui)
    constexpr int UI        = 20;  ///< 2D / ImGui overlays: last to execute
    constexpr int AFTER_UI  = 25;  ///< After ImGui (e.g. video capture with ImGui compositing)
}

/**
 * Interface for app modules that register callbacks and keybindings.
 *
 * Implement attach() to register update/draw callbacks, keybindings, and minor
 * modes with the App.  A module should be attached exactly once, before
 * App::run() is called.
 *
 * Example:
 * @code
 *   App app(1280, 720, "My App");
 *   ScenarioPlugin mod;
 *   app.addModule(mod);   // calls mod.attach(app)
 *   app.run();
 * @endcode
 */
class GEODRAW_API IAppModule {
public:
    virtual ~IAppModule() = default;

    /// Register callbacks, keybindings, and minor modes with \p app.
    /// Called automatically by App::addModule().
    virtual void attach(App& app) = 0;

    /// Unregister everything previously registered in attach().
    /// Default implementation is a no-op.
    virtual void detach(App& app) { (void)app; }
};

} // namespace geodraw
