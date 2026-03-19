// =============================================================================
// BimCore/platform/Window.cpp
// =============================================================================
#include "Window.h"
#include "Core.h"
#include <stdexcept>
#include <iostream>

// Pull in native handle accessors based on detected platform
#if defined(BIM_PLATFORM_WINDOWS)
  #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(BIM_PLATFORM_MACOS)
  #define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(BIM_PLATFORM_LINUX)
  #define GLFW_EXPOSE_NATIVE_X11
  #define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3.h>

namespace BimCore {

// -----------------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------------

Window::Window(int width, int height, const std::string& title)
    : m_width(width), m_height(height)
{
    BIM_LOG("OS", "Initialising GLFW...");

    if (!glfwInit())
        throw std::runtime_error("[Window] glfwInit() failed");

    // We manage the GPU surface ourselves (WebGPU), so tell GLFW not to touch OpenGL
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);

    BIM_LOG("OS", "Creating OS window (" << width << "x" << height << ") \"" << title << "\"");
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("[Window] glfwCreateWindow() failed");
    }

    // Store 'this' so static callbacks can reach us
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, OnFramebufferResize);
    glfwSetScrollCallback(m_window, OnScroll);

    // Log which platform we actually got
#if defined(BIM_PLATFORM_LINUX)
    const int platform = glfwGetPlatform();
    if      (platform == GLFW_PLATFORM_WAYLAND) BIM_LOG("OS", "Display server: Wayland");
    else if (platform == GLFW_PLATFORM_X11)     BIM_LOG("OS", "Display server: X11");
    else                                         BIM_LOG("OS", "Display server: unknown (" << platform << ")");
#elif defined(BIM_PLATFORM_WINDOWS)
    BIM_LOG("OS", "Platform: Win32");
#elif defined(BIM_PLATFORM_MACOS)
    BIM_LOG("OS", "Platform: Cocoa (macOS)");
#endif

    // Force the OS to commit the window memory before we hand the handle to WebGPU
    glfwPollEvents();
}

Window::~Window() {
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::PollEvents() {
    glfwPollEvents();
}

// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------

bool Window::IsKeyPressed(int glfwKey) const {
    return glfwGetKey(m_window, glfwKey) == GLFW_PRESS;
}

bool Window::IsMouseButtonPressed(int glfwButton) const {
    return glfwGetMouseButton(m_window, glfwButton) == GLFW_PRESS;
}

void Window::GetMousePosition(double& outX, double& outY) const {
    glfwGetCursorPos(m_window, &outX, &outY);
}

double Window::ConsumeScrollDelta() {
    const double delta = m_scrollAcc;
    m_scrollAcc = 0.0;
    return delta;
}

// -----------------------------------------------------------------------------
// Cursor
// -----------------------------------------------------------------------------

void Window::DisableCursor() {
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Window::EnableCursor() {
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

// -----------------------------------------------------------------------------
// Title
// -----------------------------------------------------------------------------

void Window::SetTitle(const std::string& title) {
    glfwSetWindowTitle(m_window, title.c_str());
}

// -----------------------------------------------------------------------------
// Static GLFW callbacks
// -----------------------------------------------------------------------------

void Window::OnFramebufferResize(GLFWwindow* win, int w, int h) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    self->m_width     = w;
    self->m_height    = h;
    self->m_wasResized = true;
}

void Window::OnScroll(GLFWwindow* win, double /*xoff*/, double yoff) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    self->m_scrollAcc += yoff;
}

} // namespace BimCore
