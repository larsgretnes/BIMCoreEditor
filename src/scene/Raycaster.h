// =============================================================================
// BimCore/scene/Raycaster.h
// =============================================================================
#pragma once
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>
#include "SceneModel.h"

namespace BimCore {

    struct Ray {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct HitResult {
        bool hit = false;
        float distance = 1e9f;
        glm::vec3 hitPoint = glm::vec3(0.0f);
        glm::vec3 hitV0 = glm::vec3(0.0f);
        glm::vec3 hitV1 = glm::vec3(0.0f);
        glm::vec3 hitV2 = glm::vec3(0.0f);
        std::string hitGuid = "";
        std::string hitType = "";
        uint32_t hitStartIndex = 0;
        uint32_t hitIndexCount = 0;
    };

    class Raycaster {
    public:
        static HitResult CastRay(const Ray& ray, SceneModel& doc,
                                 float cXMin, float cXMax, float cYMin, float cYMax, float cZMin, float cZMax,
                                 const std::unordered_set<std::string>& hidden,
                                 bool skipOpenings,
                                 float explodeFactor = 0.0f);

    private:
        static bool RayTriangle(const glm::vec3& ro, const glm::vec3& rd,
                                const float* v0, const float* v1, const float* v2, float& tOut);
        
        static float RayAABB(const glm::vec3& ro, const glm::vec3& invRd, 
                             const float* bmin, const float* bmax, float maxT);
    };

} // namespace BimCore