// =============================================================================
// BimCore/graphics/ShaderLibrary.h
// =============================================================================
#pragma once

namespace BimCore {
    namespace Shaders {

        // --- Global Uniforms ---
        static constexpr const char* kUniformsWGSL = R"(
struct SceneUniforms {
    viewProjection : mat4x4<f32>,
    invViewProjection : mat4x4<f32>,
    sunDirection : vec4<f32>,
    highlightColor : vec4<f32>,
    clipMin : vec4<f32>,
    clipMax : vec4<f32>,
    clipActiveMin : vec4<f32>,
    clipActiveMax : vec4<f32>,
    lightingMode : u32,
    screenWidth : u32,
    screenHeight : u32,
    _pad : u32,
};
@group(0) @binding(0) var<uniform> scene : SceneUniforms;

fn clip_check(wpos : vec3<f32>) -> bool {
    if (scene.clipActiveMax.x > 0.5 && wpos.x > scene.clipMax.x) { return false; }
    if (scene.clipActiveMin.x > 0.5 && wpos.x < scene.clipMin.x) { return false; }
    if (scene.clipActiveMax.y > 0.5 && wpos.y > scene.clipMax.y) { return false; }
    if (scene.clipActiveMin.y > 0.5 && wpos.y < scene.clipMin.y) { return false; }
    if (scene.clipActiveMax.z > 0.5 && wpos.z > scene.clipMax.z) { return false; }
    if (scene.clipActiveMin.z > 0.5 && wpos.z < scene.clipMin.z) { return false; }
    return true;
}
)";

// -------------------------------------------------------------------------
// Main Opaque & Transparent Pipeline Shader
// -------------------------------------------------------------------------
static constexpr const char* kMainWGSL = R"(
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
// Selection Solid Overlay Shader
// -------------------------------------------------------------------------
static constexpr const char* kHighlightSolidWGSL = R"(
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) wpos : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; o.wpos = v.pos; o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    if (!clip_check(in.wpos)) { discard; }
    return vec4<f32>(scene.highlightColor.rgb, scene.highlightColor.a);
}
)";

// -------------------------------------------------------------------------
// Selection Wireframe Outline Shader
// -------------------------------------------------------------------------
static constexpr const char* kHighlightOutlineWGSL = R"(
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) wpos : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; o.wpos = v.pos; o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    if (!clip_check(in.wpos)) { discard; }
    return vec4<f32>(scene.highlightColor.rgb, scene.highlightColor.a);
}
)";

// -------------------------------------------------------------------------
// AABB Bounds Shader
// -------------------------------------------------------------------------
static constexpr const char* kAABBWGSL = R"(
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) wpos : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; o.wpos = v.pos; o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    if (!clip_check(in.wpos)) { discard; }
    return vec4<f32>(1.0, 0.65, 0.0, 1.0);
}
)";

// -------------------------------------------------------------------------
// Glass Clipping Planes Shader
// -------------------------------------------------------------------------
static constexpr const char* kGlassWGSL = R"(
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) col : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); o.col = v.col; return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    return vec4<f32>(in.col, 0.25);
}
)";

// -------------------------------------------------------------------------
// Stencil Capping Shaders
// -------------------------------------------------------------------------
static constexpr const char* kMaskWGSL = R"(
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) wpos : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; o.wpos = v.pos; o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    if (!clip_check(in.wpos)) { discard; }
    return vec4<f32>(0.0);
}
)";

static constexpr const char* kCapWGSL = R"(
struct VertIn  { @location(0) pos : vec3<f32>, @location(1) nor : vec3<f32>, @location(2) col : vec3<f32> };
struct VertOut { @builtin(position) clip : vec4<f32> };
@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    return vec4<f32>(0.25, 0.26, 0.27, 1.0);
}
)";

// -------------------------------------------------------------------------
// Post-Processing SSAO Shader (FIXED)
// -------------------------------------------------------------------------
static constexpr const char* kSSAOWGSL = R"(
@group(0) @binding(1) var colorTex : texture_2d<f32>;
@group(0) @binding(2) var depthTex : texture_depth_2d;

@vertex fn vs_main(@builtin(vertex_index) vIdx : u32) -> @builtin(position) vec4<f32> {
    let x = f32((vIdx << 1u) & 2u);
    let y = f32(vIdx & 2u);
    return vec4<f32>(x * 2.0 - 1.0, y * -2.0 + 1.0, 0.0, 1.0);
}

fn getPosition(uv: vec2<f32>, depth: f32) -> vec3<f32> {
    let ndc = vec4<f32>(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    let world = scene.invViewProjection * ndc;
    return world.xyz / world.w;
}

@fragment fn fs_main(@builtin(position) pos : vec4<f32>) -> @location(0) vec4<f32> {
    let iCoords = vec2<i32>(pos.xy);
    let baseColor = textureLoad(colorTex, iCoords, 0);
    let depth = textureLoad(depthTex, iCoords, 0);

    // Skip SSAO on the skybox/background
    if (depth >= 1.0) { return baseColor; }

    let uv = pos.xy / vec2<f32>(f32(scene.screenWidth), f32(scene.screenHeight));
    let origin = getPosition(uv, depth);

    // Reconstruct surface normals perfectly from depth
    let dx = dpdx(origin);
    let dy = dpdy(origin);
    var normal = normalize(cross(dx, dy));
    if (length(normal) < 0.01) { normal = vec3<f32>(0.0, 1.0, 0.0); }

    let camPos = getPosition(vec2<f32>(0.5, 0.5), 0.0);
    let viewDir = normalize(camPos - origin);

    // --- FIXED: Prevent INVERTED NORMALS! ---
    // If the normal is facing away from the camera (into the wall), flip it!
    if (dot(normal, viewDir) < 0.0) {
        normal = -normal;
    }

    var occlusion = 0.0;

    // --- SSAO TUNING PARAMS ---
    let samples = 32;
    let radius = 0.3;
    let bias = 0.05; // Increased bias to completely eliminate flat-surface dirt

    // Subtle rotation to hide the spiral structure
    let noiseAngle = fract(sin(dot(uv, vec2<f32>(12.9898, 78.233))) * 43758.5453) * 6.2831853;

    for (var i = 0; i < samples; i = i + 1) {
        let fi = f32(i);

        // Vogel / Fibonacci Spiral Distribution
        let r = sqrt((fi + 0.5) / f32(samples));
        let angle = fi * 2.3999632 + noiseAngle;

        var sampleDir = normalize(vec3<f32>(cos(angle)*r, sin(angle)*r, sqrt(max(0.0, 1.0 - r*r))));

        // Align sample direction to the CORRECTED normal hemisphere
        if (dot(sampleDir, normal) < 0.0) { sampleDir = -sampleDir; }

        let samplePos = origin + normal * bias + sampleDir * radius;

        let sampleNDC = scene.viewProjection * vec4<f32>(samplePos, 1.0);
        let sampleUV = vec2<f32>(sampleNDC.x / sampleNDC.w * 0.5 + 0.5, 1.0 - (sampleNDC.y / sampleNDC.w * 0.5 + 0.5));

        if (sampleUV.x >= 0.0 && sampleUV.x <= 1.0 && sampleUV.y >= 0.0 && sampleUV.y <= 1.0) {
            let sCoords = vec2<i32>(sampleUV * vec2<f32>(f32(scene.screenWidth), f32(scene.screenHeight)));
            let sampleDepth = textureLoad(depthTex, sCoords, 0);
            let actualPos = getPosition(sampleUV, sampleDepth);

            let actualDist = length(actualPos - camPos);
            let checkDist = length(samplePos - camPos);

            if (actualDist < checkDist - 0.01) {
                let depthDiff = length(origin - actualPos);

                // Smoothly fade out the shadow if the blocking object is too far away
                let rangeCheck = 1.0 - smoothstep(radius * 0.5, radius * 1.5, depthDiff);
                occlusion += rangeCheck;
            }
        }
    }

    // Average the occlusion and increase the contrast slightly
    occlusion = 1.0 - (occlusion / f32(samples));
    occlusion = clamp(occlusion, 0.0, 1.0);
    occlusion = pow(occlusion, 1.8);

    // Warm, realistic charcoal tint for the deep shadows
    let aoColor = mix(vec3<f32>(0.2, 0.2, 0.25), vec3<f32>(1.0), occlusion);
    return vec4<f32>(baseColor.rgb * aoColor, baseColor.a);
}
)";

    } // namespace Shaders
} // namespace BimCore
