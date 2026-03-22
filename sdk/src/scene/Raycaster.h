// =============================================================================
// BimCore/scene/Raycaster.h
// =============================================================================
#pragma once
#include "BimDocument.h"
#include <glm/glm.hpp>
#include <unordered_set>

namespace BimCore {

    struct Ray {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct HitResult {
        bool        hit = false;
        float       distance = 1e9f;
        glm::vec3   hitPoint;
        glm::vec3   hitV0; // --- NEW: Exact triangle vertices ---
        glm::vec3   hitV1;
        glm::vec3   hitV2;
        std::string hitGuid;
        std::string hitType;
        uint32_t    hitStartIndex = 0;
        uint32_t    hitIndexCount = 0;
    };

    class Raycaster {
    public:
        static HitResult CastRay(const Ray& ray, const RenderMesh& mesh,
                                 float clipXMin, float clipXMax,
                                 float clipYMin, float clipYMax,
                                 float clipZMin, float clipZMax,
                                 const std::unordered_set<std::string>& hiddenObjects,
                                 bool skipOpeningsAndSpaces);

    private:
        static bool RayTriangle(const glm::vec3& ro, const glm::vec3& rd,
                                const float* v0, const float* v1, const float* v2, float& tOut);
    };

} // namespace BimCore
