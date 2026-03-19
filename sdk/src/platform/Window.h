#pragma once
// =============================================================================
// BimCore/platform/Window.h
// OS window + input abstraction around GLFW.
// Intentionally keeps GLFW out of the engine-wide include graph.
// =============================================================================
#include <string>
#include <cstdint>

struct GLFWwindow; // forward-declare to avoid leaking GLFW into the engine

namespace BimCore {

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    // Non-copyable / non-movable (owns an OS window handle)
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&)                 = delete;
    Window& operator=(Window&&)      = delete;

    // --- Main loop interface ---
    [[nodiscard]] bool ShouldClose() const;
    void               PollEvents();

    // --- Native handle (required by WebGPU surface creation) ---
    [[nodiscard]] GLFWwindow* GetNativeWindow() const { return m_window; }

    // --- Dimensions ---
    [[nodiscard]] int GetWidth()  const { return m_width; }
    [[nodiscard]] int GetHeight() const { return m_height; }

    // --- Resize tracking ---
    [[nodiscard]] bool WasWindowResized() const { return m_wasResized; }
    void               ResetWindowResizedFlag()  { m_wasResized = false; }

    // --- Keyboard ---
    [[nodiscard]] bool IsKeyPressed(int glfwKey) const;

    // --- Mouse ---
    [[nodiscard]] bool IsMouseButtonPressed(int glfwButton) const;
    void               GetMousePosition(double& outX, double& outY) const;
    double             ConsumeScrollDelta();   // returns accumulated delta, then resets

    // --- Cursor ---
    void DisableCursor(); // grabs + hides cursor (for FPS flight mode)
    void EnableCursor();  // restores normal cursor

    // --- Title ---
    void SetTitle(const std::string& title);

private:
    // GLFW static callbacks (need access to our members via user pointer)
    static void OnFramebufferResize(GLFWwindow* win, int w, int h);
    static void OnScroll(GLFWwindow* win, double xoff, double yoff);

    GLFWwindow* m_window    = nullptr;
    int         m_width     = 0;
    int         m_height    = 0;
    double      m_scrollAcc = 0.0;  // accumulated scroll — read by ConsumeScrollDelta()
    bool        m_wasResized = false;
};

} // namespace BimCore
