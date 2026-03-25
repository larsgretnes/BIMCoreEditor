// =============================================================================
// BimCore/scene/Raycaster.cpp
// =============================================================================
#include "Raycaster.h"
#include <limits>
#include <algorithm>

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
    
    float Raycaster::RayAABB(const glm::vec3& ro, const glm::vec3& invRd, const float* bmin, const float* bmax, float maxT) {
        float tx1 = (bmin[0] - ro.x) * invRd.x;
        float tx2 = (bmax[0] - ro.x) * invRd.x;
        float tmin = std::min(tx1, tx2);
        float tmax = std::max(tx1, tx2);

        float ty1 = (bmin[1] - ro.y) * invRd.y;
        float ty2 = (bmax[1] - ro.y) * invRd.y;
        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));

        float tz1 = (bmin[2] - ro.z) * invRd.z;
        float tz2 = (bmax[2] - ro.z) * invRd.z;
        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));

        if (tmax >= tmin && tmin < maxT && tmax > 0.0f) {
            return tmin > 0.0f ? tmin : 0.0f;
        }
        return 1e9f;
    }

    HitResult Raycaster::CastRay(const Ray& ray, const RenderMesh& mesh,
                                 float cXMin, float cXMax, float cYMin, float cYMax, float cZMin, float cZMax,
                                 const std::unordered_set<std::string>& hidden,
                                 bool skipOpenings)
    {
        HitResult best;

        glm::vec3 invRd(
            ray.direction.x == 0.0f ? 1e15f : 1.0f / ray.direction.x,
            ray.direction.y == 0.0f ? 1e15f : 1.0f / ray.direction.y,
            ray.direction.z == 0.0f ? 1e15f : 1.0f / ray.direction.z
        );

        for (const auto& sub : mesh.subMeshes) {
            if (hidden.count(sub.guid)) continue;
            if (skipOpenings && (sub.type == "IfcOpeningElement" || sub.type == "IfcSpace")) continue;
            if (sub.indexCount == 0) continue;

            if (mesh.bvhNodes.empty() || sub.bvhRootIndex >= mesh.bvhNodes.size()) {
                // Fallback to O(N) if no BVH is available (e.g., glTF models)
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
                        best.hitV0 = glm::vec3(v0[0], v0[1], v0[2]);
                        best.hitV1 = glm::vec3(v1[0], v1[1], v1[2]);
                        best.hitV2 = glm::vec3(v2[0], v2[1], v2[2]);
                        best.hitGuid = sub.guid;
                        best.hitType = sub.type;
                        best.hitStartIndex = sub.startIndex;
                        best.hitIndexCount = sub.indexCount;
                    }
                }
                continue;
            }

            // Traverse the Flat BVH (Iterative)
            uint32_t stack[64];
            int stackPtr = 0;
            stack[stackPtr++] = sub.bvhRootIndex;

            while (stackPtr > 0) {
                uint32_t nodeIdx = stack[--stackPtr];
                const BVHNode& node = mesh.bvhNodes[nodeIdx];

                float tBox = RayAABB(ray.origin, invRd, node.aabbMin, node.aabbMax, best.distance);
                if (tBox >= best.distance) continue;

                if (node.triCount > 0) {
                    // Leaf Node
                    for (uint32_t i = 0; i < node.triCount; ++i) {
                        uint32_t idxStart = node.leftFirst + i * 3;
                        const float* v0 = mesh.vertices[mesh.indices[idxStart]].position;
                        const float* v1 = mesh.vertices[mesh.indices[idxStart+1]].position;
                        const float* v2 = mesh.vertices[mesh.indices[idxStart+2]].position;

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
                            best.hitV0 = glm::vec3(v0[0], v0[1], v0[2]);
                            best.hitV1 = glm::vec3(v1[0], v1[1], v1[2]);
                            best.hitV2 = glm::vec3(v2[0], v2[1], v2[2]);
                            best.hitGuid = sub.guid;
                            best.hitType = sub.type;
                            best.hitStartIndex = sub.startIndex;
                            best.hitIndexCount = sub.indexCount;
                        }
                    }
                } else {
                    // Inner Node
                    uint32_t child0 = node.leftFirst;
                    uint32_t child1 = node.leftFirst + 1;
                    
                    float d0 = RayAABB(ray.origin, invRd, mesh.bvhNodes[child0].aabbMin, mesh.bvhNodes[child0].aabbMax, best.distance);
                    float d1 = RayAABB(ray.origin, invRd, mesh.bvhNodes[child1].aabbMin, mesh.bvhNodes[child1].aabbMax, best.distance);
                    
                    if (d0 > d1) {
                        if (d0 < best.distance) stack[stackPtr++] = child0;
                        if (d1 < best.distance) stack[stackPtr++] = child1;
                    } else {
                        if (d1 < best.distance) stack[stackPtr++] = child1;
                        if (d0 < best.distance) stack[stackPtr++] = child0;
                    }
                }
            }
        }
        return best;
    }

} // namespace BimCore
