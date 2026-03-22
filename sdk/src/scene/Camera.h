#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace BimCore {

    class Camera {
    public:
        Camera(float aspectRatio, float fovDegrees = 45.0f);

        void SetPosition(const glm::vec3& position);
        void SetAspectRatio(float aspect);

        void ProcessKeyboard(const glm::vec3& direction, float deltaTime);
        void ProcessMouseMovement(float xoffset, float yoffset);

        void Update(float deltaTime);

        const glm::mat4& GetViewMatrix();
        const glm::mat4& GetProjectionMatrix();
        glm::mat4 GetViewProjectionMatrix();

        void ProcessPan(float deltaX, float deltaY);
        void ProcessOrbit(float deltaX, float deltaY);
        void ProcessZoom(float zoomDelta);
        void SetZoomSpeed(float speed) { m_zoomSpeed = speed; }

        void SetMovementSpeed(float speed) { m_movementSpeed = speed; }
        void SetMouseSensitivity(float sens) { m_mouseSensitivity = sens; }

        glm::vec3 GetPosition() const { return m_position; }

        void FocusOn(const glm::vec3& center, float radius);
        void ResetView(const glm::vec3& center, float radius, float targetYaw, float targetPitch);
        void SetPivot(const glm::vec3& newPivot);

    private:
        void UpdateCameraVectors();
        void UpdateMatrices();

        // --- FIXED: Z-Up Starting Positions ---
        // Look down the Y axis (green), slightly above the Z (blue) ground plane
        glm::vec3 m_position{0.0f, -20.0f, 5.0f};
        glm::vec3 m_front{0.0f, 1.0f, 0.0f};
        glm::vec3 m_up{0.0f, 0.0f, 1.0f};
        glm::vec3 m_right{1.0f, 0.0f, 0.0f};

        // Euler Angles for Z-Up: Yaw 90 faces +Y. Pitch -15 looks slightly down.
        float m_yaw = 90.0f;
        float m_pitch = -15.0f;

        glm::vec3 m_pivot{0.0f, 0.0f, 0.0f};
        float m_orbitDistance = 20.0f;

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

        float m_movementSpeed = 5.0f;
        float m_mouseSensitivity = 0.1f;
        float m_zoomSpeed = 1.0f;

        float m_aspect;
        float m_fov;
        float m_near = 0.1f;
        float m_far = 100000.0f;

        glm::mat4 m_viewMatrix{1.0f};
        glm::mat4 m_projMatrix{1.0f};

        bool m_dirty = true;
    };

} // namespace BimCore
