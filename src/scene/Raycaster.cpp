// =============================================================================
// BimCore/scene/Raycaster.cpp
// =============================================================================
#include "Raycaster.h"
#include <limits>

namespace BimCore {

    bool Raycaster::RayTriangle(const glm::vec3& ro, const glm::vec3& rd,
                                const float* v0, const float* v1, const float* v2, float& tOut)
    {
        const float EPSILON = 1e-7f;
        glm::vec3 edge1(v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]);
        glm::vec3 edge2(v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]);
        glm::vec3 h = glm::cross(rd, edge2);
        float a = glm::dot(edge1, h);

        if (a > -EPSILON && a < EPSILON) return false;

        float f = 1.0f / a;
        glm::vec3 s(ro.x - v0[0], ro.y - v0[1], ro.z - v0[2]);
        float u = f * glm::dot(s, h);

        if (u < 0.0f || u > 1.0f) return false;

        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(rd, q);

        if (v < 0.0f || u + v > 1.0f) return false;

        float t = f * glm::dot(edge2, q);
        if (t > EPSILON) {
            tOut = t;
            return true;
        }

        return false;
    }

    HitResult Raycaster::CastRay(const Ray& ray, const RenderMesh& mesh,
                                 float cXMin, float cXMax, float cYMin, float cYMax, float cZMin, float cZMax,
                                 const std::unordered_set<std::string>& hidden,
                                 bool skipOpenings)
    {
        HitResult best;

        for (const auto& sub : mesh.subMeshes) {
            if (hidden.count(sub.guid)) continue;
            if (skipOpenings && (sub.type == "IfcOpeningElement" || sub.type == "IfcSpace")) continue;

            const uint32_t end = sub.startIndex + sub.indexCount;
            for (uint32_t i = sub.startIndex; i + 2 < end; i += 3) {
                const float* v0 = mesh.vertices[mesh.indices[i]].position;
                const float* v1 = mesh.vertices[mesh.indices[i+1]].position;
                const float* v2 = mesh.vertices[mesh.indices[i+2]].position;

                if (v0[0] > cXMax && v1[0] > cXMax && v2[0] > cXMax) continue;
                if (v0[0] < cXMin && v1[0] < cXMin && v2[0] < cXMin) continue;
                if (v0[1] > cYMax && v1[1] > cYMax && v2[1] > cYMax) continue;
                if (v0[1] < cYMin && v1[1] < cYMin && v2[1] < cYMin) continue;
                if (v0[2] > cZMax && v1[2] > cZMax && v2[2] > cZMax) continue;
                if (v0[2] < cZMin && v1[2] < cZMin && v2[2] < cZMin) continue;

                float t = 0.0f;
                if (RayTriangle(ray.origin, ray.direction, v0, v1, v2, t) && t < best.distance) {

                    float hX = ray.origin[0] + ray.direction[0] * t;
                    float hY = ray.origin[1] + ray.direction[1] * t;
                    float hZ = ray.origin[2] + ray.direction[2] * t;

                    if (hX > cXMax || hX < cXMin || hY > cYMax || hY < cYMin || hZ > cZMax || hZ < cZMin) {
                        continue;
                    }

                    best.hit = true;
                    best.distance = t;
                    best.hitPoint = glm::vec3(hX, hY, hZ);
                    best.hitV0 = glm::vec3(v0[0], v0[1], v0[2]); // --- NEW ---
                    best.hitV1 = glm::vec3(v1[0], v1[1], v1[2]);
                    best.hitV2 = glm::vec3(v2[0], v2[1], v2[2]);
                    best.hitGuid = sub.guid;
                    best.hitType = sub.type;
                    best.hitStartIndex = sub.startIndex;
                    best.hitIndexCount = sub.indexCount;
                }
            }
        }
        return best;
    }

} // namespace BimCore
