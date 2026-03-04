"""
GeoDraw Python bindings for debugging and visualization.

QuickDraw API (simple):
    import geodraw
    scene = geodraw.Scene()
    scene.add_point((0, 0, 0), radius=0.5, color=(1, 0, 0))
    geodraw.show(scene)

App API (full event loop):
    import geodraw

    app = geodraw.App(1280, 720, "My App")

    toggle = app.add_toggle("show-grid", "Show grid", key=geodraw.Key.G)

    @app.add_update_callback()
    def update(dt):
        s = app.scene()
        s.clear()
        if toggle.value:
            s.add_axes((0, 0, 0), scale=5.0)

    @app.set_imgui_callback()
    def draw_ui(gui):
        gui.begin("Controls")
        gui.text(f"Grid: {toggle.value}")
        gui.end()

    app.run()

Controls (in viewer window):
    - Mouse drag: Rotate camera (orbit)
    - Mouse scroll: Zoom in/out
    - Right-click drag: Pan camera
    - Q / ESC: Close window
    - H: Show all commands
"""

from ._geodraw import (
    Scene,
    show,
    App,
    Toggle,
    MinorMode,
    LoadedModel,
    TooltipBuilder,
    TooltipContext,
    Key,
    Mod,
    Priority,
)

# Gui is only available when ImGui support is compiled in
try:
    from ._geodraw import Gui  # noqa: F401
    __all__ = ['Scene', 'show', 'App', 'Toggle', 'MinorMode', 'Gui', 'LoadedModel',
               'TooltipBuilder', 'TooltipContext', 'Key', 'Mod', 'Priority']
except ImportError:
    __all__ = ['Scene', 'show', 'App', 'Toggle', 'MinorMode', 'LoadedModel',
               'TooltipBuilder', 'TooltipContext', 'Key', 'Mod', 'Priority']
__version__ = '0.1.0'
