// =============================================================================
// BimCore/graphics/ShaderLibrary.h
// =============================================================================
#pragma once

namespace BimCore {
    namespace Shaders {

        // -------------------------------------------------------------------------
        // Main Opaque & Transparent Pipeline Shader
        // -------------------------------------------------------------------------
        static constexpr const char* kMainWGSL = R"(
struct SceneUniforms {
    viewProjection : mat4x4<f32>,
    sunDirection   : vec4<f32>,
    highlightColor : vec4<f32>,
    clipMin        : vec4<f32>,
    clipMax        : vec4<f32>,
    clipActiveMin  : vec4<f32>,
    clipActiveMax  : vec4<f32>,
    lightingMode   : u32,
    _pad1 : u32, _pad2 : u32, _pad3 : u32,
};
@group(0) @binding(0) var<uniform> scene : SceneUniforms;

struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) nor : vec3<f32>, @location(1) wpos : vec3<f32>, @location(2) col : vec3<f32> };

@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut;
    o.wpos = v.pos;
    o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0);
    o.nor  = v.nor;
    o.col  = v.col;
    return o;
}

fn shade(in : VertOut) -> vec3<f32> {
    let dx = dpdx(in.wpos);
    let dy = dpdy(in.wpos);
    let faceNormal = normalize(cross(dx, dy));
    let b  = in.col;
    if (scene.lightingMode == 0u) {
        let l1 = normalize(vec3<f32>( 0.7,  0.8,  1.0));
        let l2 = normalize(vec3<f32>(-0.5, -0.2, -1.0));
        let d  = abs(dot(faceNormal, l1)) + abs(dot(faceNormal, l2)) * 0.3 + 0.2;
        return b * clamp(d, 0.0, 1.0);
    } else {
        return b * (abs(dot(faceNormal, normalize(scene.sunDirection.xyz))) + 0.1);
    }
}

fn clip_check(wpos : vec3<f32>) -> bool {
    if (scene.clipActiveMax.x > 0.5 && wpos.x > scene.clipMax.x) { return false; }
    if (scene.clipActiveMin.x > 0.5 && wpos.x < scene.clipMin.x) { return false; }
    if (scene.clipActiveMax.y > 0.5 && wpos.y > scene.clipMax.y) { return false; }
    if (scene.clipActiveMin.y > 0.5 && wpos.y < scene.clipMin.y) { return false; }
    if (scene.clipActiveMax.z > 0.5 && wpos.z > scene.clipMax.z) { return false; }
    if (scene.clipActiveMin.z > 0.5 && wpos.z < scene.clipMin.z) { return false; }
    return true;
}

@fragment fn fs_opaque(in : VertOut) -> @location(0) vec4<f32> {
    if (!clip_check(in.wpos)) { discard; }
    return vec4<f32>(shade(in), 1.0);
}

@fragment fn fs_transparent(in : VertOut) -> @location(0) vec4<f32> {
    if (!clip_check(in.wpos)) { discard; }
    return vec4<f32>(shade(in), 0.35);
}
)";

// -------------------------------------------------------------------------
// Selection Highlight Shader
// -------------------------------------------------------------------------
static constexpr const char* kHighlightWGSL = R"(
struct SceneUniforms {
    viewProjection : mat4x4<f32>, sunDirection : vec4<f32>, highlightColor : vec4<f32>,
    clipMin : vec4<f32>, clipMax : vec4<f32>, clipActiveMin : vec4<f32>, clipActiveMax : vec4<f32>,
    lightingMode : u32, _p1 : u32, _p2 : u32, _p3 : u32,
};
@group(0) @binding(0) var<uniform> scene : SceneUniforms;
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) wpos : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; o.wpos = v.pos; o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    if (scene.clipActiveMax.x > 0.5 && in.wpos.x > scene.clipMax.x) { discard; }
    if (scene.clipActiveMin.x > 0.5 && in.wpos.x < scene.clipMin.x) { discard; }
    if (scene.clipActiveMax.y > 0.5 && in.wpos.y > scene.clipMax.y) { discard; }
    if (scene.clipActiveMin.y > 0.5 && in.wpos.y < scene.clipMin.y) { discard; }
    if (scene.clipActiveMax.z > 0.5 && in.wpos.z > scene.clipMax.z) { discard; }
    if (scene.clipActiveMin.z > 0.5 && in.wpos.z < scene.clipMin.z) { discard; }
    return scene.highlightColor;
}
)";

// -------------------------------------------------------------------------
// AABB Bounds Shader
// -------------------------------------------------------------------------
static constexpr const char* kAABBWGSL = R"(
struct SceneUniforms {
    viewProjection : mat4x4<f32>, sunDirection : vec4<f32>, highlightColor : vec4<f32>,
    clipMin : vec4<f32>, clipMax : vec4<f32>, clipActiveMin : vec4<f32>, clipActiveMax : vec4<f32>,
    lightingMode : u32, _p1 : u32, _p2 : u32, _p3 : u32,
};
@group(0) @binding(0) var<uniform> scene : SceneUniforms;
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) wpos : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; o.wpos = v.pos; o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    if (scene.clipActiveMax.x > 0.5 && in.wpos.x > scene.clipMax.x) { discard; }
    if (scene.clipActiveMin.x > 0.5 && in.wpos.x < scene.clipMin.x) { discard; }
    if (scene.clipActiveMax.y > 0.5 && in.wpos.y > scene.clipMax.y) { discard; }
    if (scene.clipActiveMin.y > 0.5 && in.wpos.y < scene.clipMin.y) { discard; }
    if (scene.clipActiveMax.z > 0.5 && in.wpos.z > scene.clipMax.z) { discard; }
    if (scene.clipActiveMin.z > 0.5 && in.wpos.z < scene.clipMin.z) { discard; }
    return vec4<f32>(1.0, 0.65, 0.0, 1.0);
}
)";

// -------------------------------------------------------------------------
// Glass Clipping Planes Shader
// -------------------------------------------------------------------------
static constexpr const char* kGlassWGSL = R"(
struct SceneUniforms {
    viewProjection : mat4x4<f32>, sunDirection : vec4<f32>, highlightColor : vec4<f32>,
    clipMin : vec4<f32>, clipMax : vec4<f32>, clipActiveMin : vec4<f32>, clipActiveMax : vec4<f32>,
    lightingMode : u32, _p1 : u32, _p2 : u32, _p3 : u32,
};
@group(0) @binding(0) var<uniform> scene : SceneUniforms;
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) col : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut;
    o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0);
    o.col  = v.col;
    return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    return vec4<f32>(in.col, 0.25);
}
)";

    } // namespace Shaders
} // namespace BimCore
