#include "Core.h"
#include "Camera.h"
#include <algorithm> // for std::clamp

namespace BimCore {

    Camera::Camera(float aspectRatio, float fovDegrees)
    : m_aspect(aspectRatio), m_fov(glm::radians(fovDegrees))
    {
        UpdateCameraVectors();
    }

    void Camera::SetPosition(const glm::vec3& position) {
        m_position = position;
        m_dirty = true;
    }

    void Camera::SetAspectRatio(float aspect) {
        m_aspect = aspect;
        m_dirty = true;
    }

    void Camera::ProcessKeyboard(const glm::vec3& direction, float deltaTime) {
        float velocity = m_movementSpeed * deltaTime;

        m_position += m_front * direction.z * velocity;
        m_position += m_right * direction.x * velocity;
        m_position += m_up * direction.y * velocity;

        // --- NEW: Sync the CAD pivot so Flight Mode doesn't break Orbiting! ---
        m_pivot = m_position + (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::ProcessMouseMovement(float xoffset, float yoffset) {
        xoffset *= m_mouseSensitivity;
        yoffset *= m_mouseSensitivity;

        m_yaw += xoffset;
        m_pitch += yoffset;

        // Constrain the pitch so the screen doesn't flip upside down
        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;

        UpdateCameraVectors();

        // --- NEW: Sync the CAD pivot here too! ---
        m_pivot = m_position + (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::ProcessPan(float deltaX, float deltaY) {
        // --- NEW: True CAD Panning Math ---
        float panSpeed = (m_orbitDistance * std::tan(m_fov / 2.0f) * 2.0f) / 1080.0f;
        panSpeed = std::max(0.005f, panSpeed); // Ensure it never completely stops

        glm::vec3 panDelta = (m_right * deltaX * panSpeed) + (m_up * deltaY * panSpeed);
        m_position -= panDelta;
        m_pivot -= panDelta;
        m_dirty = true;
    }

    void Camera::ProcessOrbit(float deltaX, float deltaY) {
        m_yaw += deltaX * m_mouseSensitivity;
        m_pitch += deltaY * m_mouseSensitivity;

        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;

        UpdateCameraVectors();

        // The magic CAD formula: Position is derived from the Pivot, pointing backward by the Distance
        m_position = m_pivot - (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::ProcessZoom(float zoomDelta) {
        // --- FIX: Restored full zoom speed so it flies! ---
        // A minimal buffer of 0.5f ensures it never permanently halts
        float dynamicZoomSpeed = std::max(0.5f, m_orbitDistance * 0.1f) * m_zoomSpeed;
        m_orbitDistance -= (zoomDelta * dynamicZoomSpeed);

        // The Dolly Zoom Upgrade
        if (m_orbitDistance < 0.1f) {
            float pushAmount = 0.1f - m_orbitDistance;
            m_pivot += m_front * pushAmount;
            m_orbitDistance = 0.1f;
        }

        m_position = m_pivot - (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::FocusOn(const glm::vec3& center, float radius) {
        // Save where we are starting from
        m_startPivot = m_pivot;
        m_startDistance = m_orbitDistance;

        // Save where we want to end up
        m_targetPivot = center;
        m_targetDistance = (radius * 1.2f) / std::sin(m_fov / 2.0f);
        if (m_targetDistance < 0.5f) m_targetDistance = 0.5f;

        // Trigger the animation lock
        m_isFocusing = true;
        m_isResettingAngles = false; 
        m_focusProgress = 0.0f;
    }

    void Camera::ResetView(const glm::vec3& center, float radius, float targetYaw, float targetPitch) {
        FocusOn(center, radius); // Re-use the position math!

        m_isResettingAngles = true;
        m_startYaw = m_yaw;
        m_startPitch = m_pitch;

        // --- MAGIC: Find the shortest rotational path! ---
        float deltaYaw = targetYaw - m_yaw;
        while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
        while (deltaYaw < -180.0f) deltaYaw += 360.0f;
        m_targetYaw = m_yaw + deltaYaw;

        m_targetPitch = targetPitch;
    }

    void Camera::Update(float deltaTime) {
        if (!m_isFocusing) return;

        // Speed multiplier
        m_focusProgress += deltaTime * 2.5f;

        if (m_focusProgress >= 1.0f) {
            m_focusProgress = 1.0f;
            m_isFocusing = false;
        }

        // Smoothstep formula for a beautiful ease-in / ease-out curve
        float t = m_focusProgress;
        t = t * t * (3.0f - 2.0f * t);

        m_pivot = glm::mix(m_startPivot, m_targetPivot, t);
        m_orbitDistance = glm::mix(m_startDistance, m_targetDistance, t);

        if (m_isResettingAngles) {
            m_yaw = glm::mix(m_startYaw, m_targetYaw, t);
            m_pitch = glm::mix(m_startPitch, m_targetPitch, t);
            UpdateCameraVectors(); // Recalculate where we are looking
        }

        m_position = m_pivot - (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::UpdateCameraVectors() {
        // Calculate the new Front vector using Euler angles
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

        m_front = glm::normalize(front);

        // Recalculate Right and Up vectors
        m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
        m_up    = glm::normalize(glm::cross(m_right, m_front));

        m_dirty = true;
    }

    void Camera::UpdateMatrices() {
        if (!m_dirty) return;
        m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
        m_projMatrix = glm::perspective(m_fov, m_aspect, m_near, m_far);
        m_dirty = false;
    }

    const glm::mat4& Camera::GetViewMatrix() { UpdateMatrices(); return m_viewMatrix; }
    const glm::mat4& Camera::GetProjectionMatrix() { UpdateMatrices(); return m_projMatrix; }
    glm::mat4 Camera::GetViewProjectionMatrix() { UpdateMatrices(); return m_projMatrix * m_viewMatrix; }

} // namespace BimCore