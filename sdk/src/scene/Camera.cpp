// =============================================================================
// BimCore/scene/Camera.cpp
// =============================================================================
#include "Core.h"
#include "Camera.h"
#include <algorithm> 

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
        m_isFocusing = false; 

        float velocity = m_movementSpeed * deltaTime;

        m_position += m_front * direction.z * velocity;
        m_position += m_right * direction.x * velocity;
        m_position += m_up * direction.y * velocity;

        m_pivot = m_position + (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::ProcessMouseMovement(float xoffset, float yoffset) {
        m_isFocusing = false; 

        xoffset *= m_mouseSensitivity;
        yoffset *= m_mouseSensitivity;

        m_yaw += xoffset;
        m_pitch += yoffset;

        UpdateCameraVectors();

        m_pivot = m_position + (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::SetPivot(const glm::vec3& newPivot) {
        float jumpDist = glm::length(m_pivot - newPivot);

        if (jumpDist > m_pivotJumpThreshold) { 
            m_startPivot = m_pivot;
            m_targetPivot = newPivot;
            m_startDistance = m_orbitDistance;

            m_targetDistance = glm::length(m_position - newPivot);
            if (m_targetDistance < m_minOrbitDistance * 5.0f) m_targetDistance = m_minOrbitDistance * 5.0f;

            m_isFocusing = true;
            m_isResettingAngles = false;
            m_focusProgress = 0.0f;
        } else {
            m_pivot = newPivot;
            m_orbitDistance = glm::length(m_position - m_pivot);
            m_dirty = true;
        }
    }

    void Camera::ProcessPan(float deltaX, float deltaY) {
        m_isFocusing = false; 

        float panSpeed = (m_orbitDistance * std::tan(m_fov / 2.0f) * 2.0f) / m_panReferenceHeight;
        panSpeed = std::max(0.005f, panSpeed); 

        glm::vec3 panDelta = (m_right * deltaX * panSpeed) + (m_up * deltaY * panSpeed);
        m_position -= panDelta;
        m_pivot -= panDelta;
        m_dirty = true;
    }

    void Camera::ProcessOrbit(float deltaX, float deltaY) {
        m_isFocusing = false; 

        m_yaw -= deltaX * m_mouseSensitivity;
        m_pitch += deltaY * m_mouseSensitivity;

        UpdateCameraVectors();

        m_position = m_pivot - (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::ProcessZoom(float zoomDelta) {
        m_isFocusing = false; 

        float flySpeed = m_zoomSpeed * m_movementSpeed * m_zoomFlyMultiplier;
        m_orbitDistance -= (zoomDelta * flySpeed);

        if (m_orbitDistance < m_minOrbitDistance) {
            float pushAmount = m_minOrbitDistance - m_orbitDistance;
            m_pivot += m_front * pushAmount;
            m_orbitDistance = m_minOrbitDistance;
        }

        m_position = m_pivot - (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::FocusOn(const glm::vec3& center, float radius) {
        m_startPivot = m_pivot;
        m_startDistance = m_orbitDistance;

        m_targetPivot = center;
        m_targetDistance = (radius * m_focusPadding) / std::sin(m_fov / 2.0f);
        if (m_targetDistance < m_minOrbitDistance * 5.0f) m_targetDistance = m_minOrbitDistance * 5.0f;

        m_isFocusing = true;
        m_isResettingAngles = false;
        m_focusProgress = 0.0f;
    }

    void Camera::ResetView(const glm::vec3& center, float radius, float targetYaw, float targetPitch) {
        FocusOn(center, radius);

        m_isResettingAngles = true;
        m_startYaw = m_yaw;
        m_startPitch = m_pitch;

        float deltaYaw = targetYaw - m_yaw;
        while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
        while (deltaYaw < -180.0f) deltaYaw += 360.0f;
        m_targetYaw = m_yaw + deltaYaw;

        m_targetPitch = targetPitch;
    }

    void Camera::Update(float deltaTime) {
        if (!m_isFocusing) return;

        m_focusProgress += deltaTime * m_focusSpeed;

        if (m_focusProgress >= 1.0f) {
            m_focusProgress = 1.0f;
            m_isFocusing = false;
        }

        float t = m_focusProgress;
        t = t * t * (3.0f - 2.0f * t);

        m_pivot = glm::mix(m_startPivot, m_targetPivot, t);
        m_orbitDistance = glm::mix(m_startDistance, m_targetDistance, t);

        if (m_isResettingAngles) {
            m_yaw = glm::mix(m_startYaw, m_targetYaw, t);
            m_pitch = glm::mix(m_startPitch, m_targetPitch, t);
            UpdateCameraVectors(); 
        }

        m_position = m_pivot - (m_front * m_orbitDistance);
        m_dirty = true;
    }

    void Camera::UpdateCameraVectors() {
        glm::vec3 front;
        
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_pitch));

        m_front = glm::normalize(front);

        float pMod = fmod(m_pitch, 360.0f);
        if (pMod < 0.0f) pMod += 360.0f;
        glm::vec3 worldUp = (pMod > 90.0f && pMod < 270.0f) ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 0.0f, 1.0f);

        m_right = glm::normalize(glm::cross(m_front, worldUp));
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