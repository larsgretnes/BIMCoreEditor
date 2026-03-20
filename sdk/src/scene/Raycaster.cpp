// =============================================================================
// BimCore/scene/Raycaster.cpp
// =============================================================================
#include "Raycaster.h"
#include <cmath>

namespace BimCore {

    // Möller–Trumbore algorithm
    static bool RayTriangle(const float orig[3], const float dir[3],
                            const float v0[3], const float v1[3], const float v2[3],
                            float& outT)
    {
        float e1[3] = { v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2] };
        float e2[3] = { v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2] };
        float h[3]  = { dir[1]*e2[2]-dir[2]*e2[1],
            dir[2]*e2[0]-dir[0]*e2[2],
            dir[0]*e2[1]-dir[1]*e2[0] };
            float a = e1[0]*h[0] + e1[1]*h[1] + e1[2]*h[2];
            if (a > -1e-6f && a < 1e-6f) return false;
            float f = 1.0f / a;
        float s[3] = { orig[0]-v0[0], orig[1]-v0[1], orig[2]-v0[2] };
        float u = f * (s[0]*h[0] + s[1]*h[1] + s[2]*h[2]);
        if (u < 0.0f || u > 1.0f) return false;
        float q[3] = { s[1]*e1[2]-s[2]*e1[1],
            s[2]*e1[0]-s[0]*e1[2],
            s[0]*e1[1]-s[1]*e1[0] };
            float v = f * (dir[0]*q[0] + dir[1]*q[1] + dir[2]*q[2]);
            if (v < 0.0f || u + v > 1.0f) return false;
            float t = f * (e2[0]*q[0] + e2[1]*q[1] + e2[2]*q[2]);
        if (t > 1e-5f) {
            outT = t;
            return true;
        }
        return false;
    }

    HitResult Raycaster::CastRay(const Ray& ray, const RenderMesh& mesh,
                                 float clipX, float clipY, float clipZ,
                                 const std::unordered_set<std::string>& hidden)
    {
        HitResult best;

        for (const auto& sub : mesh.subMeshes) {
            if (hidden.count(sub.guid)) continue;

            const uint32_t end = sub.startIndex + sub.indexCount;
            for (uint32_t i = sub.startIndex; i + 2 < end; i += 3) {
                const float* v0 = mesh.vertices[mesh.indices[i]].position;
                const float* v1 = mesh.vertices[mesh.indices[i+1]].position;
                const float* v2 = mesh.vertices[mesh.indices[i+2]].position;

                // Broad-phase rejection: if ALL vertices are clipped, skip the triangle entirely
                if (v0[0] > clipX && v1[0] > clipX && v2[0] > clipX) continue;
                if (v0[1] > clipY && v1[1] > clipY && v2[1] > clipY) continue;
                if (v0[2] > clipZ && v1[2] > clipZ && v2[2] > clipZ) continue;

                float t = 0.0f;
                if (RayTriangle(ray.origin, ray.direction, v0, v1, v2, t) && t < best.distance) {

                    // --- NEW: Exact Intersection Point Clipping ---
                    // Calculate the exact 3D coordinate where the ray hit the triangle
                    float hitX = ray.origin[0] + ray.direction[0] * t;
                    float hitY = ray.origin[1] + ray.direction[1] * t;
                    float hitZ = ray.origin[2] + ray.direction[2] * t;

                    // If the exact hit point is beyond the active clipping plane, ignore it!
                    if (hitX > clipX || hitY > clipY || hitZ > clipZ) {
                        continue;
                    }

                    best.hit = true;
                    best.distance = t;
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
