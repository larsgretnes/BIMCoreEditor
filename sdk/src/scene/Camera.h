#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace BimCore {

    class Camera {
    public:
        Camera(float aspectRatio, float fovDegrees = 45.0f);

        void SetPosition(const glm::vec3& position);
        void SetAspectRatio(float aspect);

        // --- NEW: Flight Controls ---
        void ProcessKeyboard(const glm::vec3& direction, float deltaTime);
        void ProcessMouseMovement(float xoffset, float yoffset);

        void Update(float deltaTime); // <-- NEW: Pump the animation!

        const glm::mat4& GetViewMatrix();
        const glm::mat4& GetProjectionMatrix();
        glm::mat4 GetViewProjectionMatrix();

        void ProcessPan(float deltaX, float deltaY);
        void ProcessOrbit(float deltaX, float deltaY);
        void ProcessZoom(float zoomDelta);
        void SetZoomSpeed(float speed) { m_zoomSpeed = speed; }
        // --- NEW: Dynamic config setters ---
        void SetMovementSpeed(float speed) { m_movementSpeed = speed; }
        void SetMouseSensitivity(float sens) { m_mouseSensitivity = sens; }

        glm::vec3 GetPosition() const { return m_position; }

        void SetPivot(const glm::vec3& pivot) { m_pivot = pivot; m_dirty = true; }
        // --- NEW: Frame Selection ---
        void FocusOn(const glm::vec3& center, float radius);
        void ResetView(const glm::vec3& center, float radius, float targetYaw, float targetPitch); // <-- NEW

    private:
        void UpdateCameraVectors();
        void UpdateMatrices();

        glm::vec3 m_position{0.0f, 0.0f, 20.0f};
        glm::vec3 m_front{0.0f, 0.0f, -1.0f}; // Looking straight down the -Z axis
        glm::vec3 m_up{0.0f, 1.0f, 0.0f};
        glm::vec3 m_right{1.0f, 0.0f, 0.0f};

        // Euler Angles for mouse look
        float m_yaw = -90.0f; // Pointing at -Z
        float m_pitch = 0.0f;

        glm::vec3 m_pivot{0.0f, 0.0f, 0.0f}; // What we are looking at
        float m_orbitDistance = 20.0f;       // How far back we are

        // --- NEW: Smooth Transition State ---
        bool m_isFocusing = false;
        float m_focusProgress = 0.0f;
        glm::vec3 m_startPivot{0.0f};
        glm::vec3 m_targetPivot{0.0f};
        float m_startDistance = 20.0f;
        float m_targetDistance = 20.0f;

        bool m_isResettingAngles = false;
        float m_startYaw = 0.0f;
        float m_targetYaw = 0.0f;
        float m_startPitch = 0.0f;
        float m_targetPitch = 0.0f;
        // -----------------------------------

        // Movement speeds
        float m_movementSpeed = 10.0f; // Meters per second
        float m_zoomSpeed = 10.0f;
        float m_mouseSensitivity = 0.1f;

        float m_fov;
        float m_aspect;

        float m_near = 0.5f;   // Push the near plane out slightly
        float m_far = 2000.0f; // Limit the far plane to 2 kilometers

        glm::mat4 m_viewMatrix{1.0f};
        glm::mat4 m_projMatrix{1.0f};
        bool m_dirty = true;
    };

} // namespace BimCore
