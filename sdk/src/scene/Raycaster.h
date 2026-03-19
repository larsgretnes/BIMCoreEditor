#pragma once
// =============================================================================
// BimCore/scene/Raycaster.h
// CPU-side ray-triangle intersection against the render mesh.
// =============================================================================
#include <string>
#include <unordered_set>
#include "BimDocument.h"
#include "Core.h"

namespace BimCore {

struct Ray {
    float origin[3];
    float direction[3];
};

struct HitResult {
    bool        hit          = false;
    float       distance     = kFloatMax;
    std::string hitGuid;
    std::string hitType;
    uint32_t    hitStartIndex = 0;
    uint32_t    hitIndexCount = 0;
};

class Raycaster {
public:
    // hiddenGuids uses unordered_set — O(1) lookup instead of O(n) per triangle
    static HitResult CastRay(
        const Ray&                        ray,
        const RenderMesh&                 mesh,
        float                             clipX       = kFloatMax,
        float                             clipY       = kFloatMax,
        float                             clipZ       = kFloatMax,
        const std::unordered_set<std::string>& hidden = {}
    );
};

} // namespace BimCore
