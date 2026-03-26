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
    lightSpaceMatrix : mat4x4<f32>,
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
// Shadow Map Depth Pass
// -------------------------------------------------------------------------
static constexpr const char* kShadowWGSL = R"(
struct VertIn  { @location(0) pos : vec3<f32> };
@vertex fn vs_main(v : VertIn) -> @builtin(position) vec4<f32> {
    return scene.lightSpaceMatrix * vec4<f32>(v.pos, 1.0);
}
)";

// -------------------------------------------------------------------------
// Main Opaque & Transparent Pipeline Shader (Shadows + PBR)
// -------------------------------------------------------------------------
static constexpr const char* kMainWGSL = R"(
struct VertIn  {
    @location(0) pos : vec3<f32>,
    @location(1) nor : vec3<f32>,
    @location(2) col : vec3<f32>,
    @location(3) uv  : vec2<f32>
};
struct VertOut {
    @builtin(position) clip : vec4<f32>,
    @location(0) nor : vec3<f32>,
    @location(1) wpos : vec3<f32>,
    @location(2) col : vec3<f32>,
    @location(3) uv  : vec2<f32>,
    @location(4) lightSpacePos : vec4<f32>
};

@group(1) @binding(0) var shadowMap: texture_depth_2d;
@group(1) @binding(1) var shadowSampler: sampler_comparison;

@group(2) @binding(0) var baseColorTex : texture_2d<f32>;
@group(2) @binding(1) var baseColorSamp : sampler;

@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut;
    o.wpos = v.pos;
    o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0);
    o.lightSpacePos = scene.lightSpaceMatrix * vec4<f32>(v.pos, 1.0);
    o.nor  = v.nor;
    o.col  = v.col;
    o.uv   = v.uv;
    return o;
}

// PCF Soft Shadows with Slope-Scaled Bias (Cures Shadow Acne!)
fn calculateShadow(lightSpacePos: vec4<f32>, normal: vec3<f32>, lightDir: vec3<f32>) -> f32 {
    let projCoords = lightSpacePos.xyz / lightSpacePos.w;
    let uv = vec2<f32>(projCoords.x * 0.5 + 0.5, 1.0 - (projCoords.y * 0.5 + 0.5));
    
    // If we are outside the shadow map bounds, the area is fully lit
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || projCoords.z > 1.0) { return 1.0; }

    // Dynamic Bias: Steeper angles get more bias. Prevents acne without disconnecting the shadow.
    let bias = max(0.003 * (1.0 - dot(normal, lightDir)), 0.0008);
    var shadow = 0.0;
    let texelSize = 1.0 / 4096.0;

    for (var x = -1; x <= 1; x++) {
        for (var y = -1; y <= 1; y++) {
            let offset = vec2<f32>(f32(x), f32(y)) * texelSize;
            shadow += textureSampleCompare(shadowMap, shadowSampler, uv + offset, projCoords.z - bias);
        }
    }
    return shadow / 9.0; // 3x3 PCF filter
}

fn shade(in : VertOut, baseColor: vec3<f32>) -> vec3<f32> {
    // Generate crisp geometric normals using screen-space derivatives
    let dx = dpdx(in.wpos);
    let dy = dpdy(in.wpos);
    var faceNormal = normalize(cross(dx, dy));
    
    // Ensure the normal always faces the camera (fixes backface black-out issues)
    let camWorld = scene.invViewProjection * vec4<f32>(0.0, 0.0, 0.5, 1.0);
    let viewDir = normalize((camWorld.xyz / camWorld.w) - in.wpos);
    if (dot(faceNormal, viewDir) < 0.0) {
        faceNormal = -faceNormal;
    }
    
    if (scene.lightingMode == 0u) {
        // --- 0: Old Flat IFC Coloring ---
        let l1 = normalize(vec3<f32>( 0.7,  0.8,  1.0));
        let l2 = normalize(vec3<f32>(-0.5, -0.2, -1.0));
        let d  = abs(dot(faceNormal, l1)) + abs(dot(faceNormal, l2)) * 0.3 + 0.2;
        return baseColor * clamp(d, 0.0, 1.0);
    } else {
        // --- 1: New PBR + Hemisphere + Shadow Rendering ---
        let lightDir = normalize(scene.sunDirection.xyz);
        let shadowAmount = calculateShadow(in.lightSpacePos, faceNormal, lightDir);
        
        // PBR Physical Inputs
        let roughness = 0.6;
        let metallic = 0.05;
        let albedo = pow(baseColor, vec3<f32>(2.2)); // Convert sRGB to Linear space for math

        // 1. Direct Sun Light
        let NdotL = max(dot(faceNormal, lightDir), 0.0);
        let diffuse = albedo / 3.14159265;
        
        let halfDir = normalize(lightDir + viewDir);
        let specAngle = max(dot(faceNormal, halfDir), 0.0);
        let specular = pow(specAngle, mix(2.0, 128.0, 1.0 - roughness)) * mix(0.04, 1.0, metallic);
        let sunColor = vec3<f32>(1.0, 0.96, 0.9) * 2.5; // Bright, warm sun
        
        let directLight = (diffuse + vec3<f32>(specular)) * sunColor * NdotL * shadowAmount;

        // 2. Hemisphere Ambient Light (The secret to realistic unlit areas)
        // Up-facing polygons get blue sky, down-facing get dark ground bounce
        let skyColor = vec3<f32>(0.55, 0.75, 0.95) * 0.7; 
        let groundColor = vec3<f32>(0.15, 0.15, 0.16); 
        let hemiMix = faceNormal.z * 0.5 + 0.5; // Z is up in BIM/IFC
        let ambientLight = mix(groundColor, skyColor, hemiMix);
        let ambient = albedo * ambientLight;
        
        // 3. Combine Lights
        let finalRadiance = ambient + directLight;

        // 4. ACES Filmic Tone Mapping
        // Compresses massive light values down to monitor ranges beautifully
        let a = 2.51;
        let b = 0.03;
        let c = 2.43;
        let d = 0.59;
        let e = 0.14;
        let mappedColor = clamp((finalRadiance*(a*finalRadiance+b))/(finalRadiance*(c*finalRadiance+d)+e), vec3<f32>(0.0), vec3<f32>(1.0));

        // Convert back to sRGB for monitor display
        return pow(mappedColor, vec3<f32>(1.0 / 2.2));
    }
}

@fragment fn fs_opaque(in : VertOut) -> @location(0) vec4<f32> {
    if (!clip_check(in.wpos)) { discard; }
    let texColor = textureSample(baseColorTex, baseColorSamp, in.uv);
    let finalColor = in.col * texColor.rgb;
    return vec4<f32>(shade(in, finalColor), 1.0);
}

@fragment fn fs_transparent(in : VertOut) -> @location(0) vec4<f32> {
    if (!clip_check(in.wpos)) { discard; }
    let texColor = textureSample(baseColorTex, baseColorSamp, in.uv);
    let finalColor = in.col * texColor.rgb;
    return vec4<f32>(shade(in, finalColor), texColor.a * 0.35);
}
)";

// -------------------------------------------------------------------------
// Selection Solid Overlay Shader
// -------------------------------------------------------------------------
static constexpr const char* kHighlightSolidWGSL = R"(
struct VertIn  { @location(0) pos: vec3<f32>, @location(1) nor: vec3<f32>, @location(2) col: vec3<f32>, @location(3) uv: vec2<f32> };
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
struct VertIn  { @location(0) pos: vec3<f32>, @location(1) nor: vec3<f32>, @location(2) col: vec3<f32>, @location(3) uv: vec2<f32> };
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
struct VertIn  { @location(0) pos: vec3<f32>, @location(1) nor: vec3<f32>, @location(2) col: vec3<f32>, @location(3) uv: vec2<f32> };
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
struct VertIn  { @location(0) pos: vec3<f32>, @location(1) nor: vec3<f32>, @location(2) col: vec3<f32>, @location(3) uv: vec2<f32> };
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
struct VertIn  { @location(0) pos: vec3<f32>, @location(1) nor: vec3<f32>, @location(2) col: vec3<f32>, @location(3) uv: vec2<f32> };
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
struct VertIn  { @location(0) pos: vec3<f32>, @location(1) nor: vec3<f32>, @location(2) col: vec3<f32>, @location(3) uv: vec2<f32> };
struct VertOut { @builtin(position) clip : vec4<f32>, @location(0) col: vec3<f32> };

@vertex fn vs_main(v : VertIn) -> VertOut {
    var o : VertOut; 
    o.clip = scene.viewProjection * vec4<f32>(v.pos, 1.0); 
    o.col = v.col; 
    return o;
}
@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    return vec4<f32>(in.col, 1.0);
}
)";

// -------------------------------------------------------------------------
// Post-Processing SSAO Shader
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

    if (depth >= 1.0) { return baseColor; }

    let uv = pos.xy / vec2<f32>(f32(scene.screenWidth), f32(scene.screenHeight));
    let origin = getPosition(uv, depth);

    let dx = dpdx(origin);
    let dy = dpdy(origin);
    var normal = normalize(cross(dx, dy));
    if (length(normal) < 0.01) { normal = vec3<f32>(0.0, 1.0, 0.0); }

    let camPos = getPosition(vec2<f32>(0.5, 0.5), 0.0);
    let viewDir = normalize(camPos - origin);
    if (dot(normal, viewDir) < 0.0) {
        normal = -normal;
    }

    var occlusion = 0.0;
    let samples = 32;
    let radius = 0.3;
    let bias = 0.05;

    let noiseAngle = fract(sin(dot(uv, vec2<f32>(12.9898, 78.233))) * 43758.5453) * 6.2831853;

    for (var i = 0; i < samples; i = i + 1) {
        let fi = f32(i);
        let r = sqrt((fi + 0.5) / f32(samples));
        let angle = fi * 2.3999632 + noiseAngle;

        var sampleDir = normalize(vec3<f32>(cos(angle)*r, sin(angle)*r, sqrt(max(0.0, 1.0 - r*r))));
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
                let rangeCheck = 1.0 - smoothstep(radius * 0.5, radius * 1.5, depthDiff);
                occlusion += rangeCheck;
            }
        }
    }

    occlusion = 1.0 - (occlusion / f32(samples));
    occlusion = clamp(occlusion, 0.0, 1.0);
    occlusion = pow(occlusion, 1.8);

    let aoColor = mix(vec3<f32>(0.2, 0.2, 0.25), vec3<f32>(1.0), occlusion);
    return vec4<f32>(baseColor.rgb * aoColor, baseColor.a);
}
)";

// -------------------------------------------------------------------------
// Infinite Grid Shader
// -------------------------------------------------------------------------
static constexpr const char* kGridWGSL = R"(
struct VertOut {
    @builtin(position) clip : vec4<f32>,
    @location(0) wpos : vec3<f32>
};

@vertex fn vs_main(@builtin(vertex_index) vi : u32) -> VertOut {
    var pos = array<vec2<f32>, 6>(
        vec2<f32>(-1.0, -1.0), vec2<f32>( 1.0, -1.0), vec2<f32>(-1.0,  1.0),
        vec2<f32>( 1.0, -1.0), vec2<f32>( 1.0,  1.0), vec2<f32>(-1.0,  1.0)
    );
    let p = pos[vi] * 1000.0; 
    var o : VertOut;
    o.wpos = vec3<f32>(p.x, p.y, 0.0);
    o.clip = scene.viewProjection * vec4<f32>(o.wpos, 1.0);
    return o;
}

@fragment fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    let coord = in.wpos.xy;
    var derivative = abs(dpdx(coord)) + abs(dpdy(coord));
    derivative = max(derivative, vec2<f32>(0.001));

    let grid1 = abs(fract(coord - 0.5) - 0.5) / derivative;
    let line1 = min(grid1.x, grid1.y);

    let grid10 = abs(fract(coord / 10.0 - 0.5) - 0.5) / (derivative / 10.0);
    let line10 = min(grid10.x, grid10.y);

    var alpha = 0.0;
    var color = vec3<f32>(0.5, 0.5, 0.5);

    if (line1 < 1.0) { alpha = (1.0 - line1) * 0.15; }
    if (line10 < 1.0) { alpha = (1.0 - line10) * 0.4; }

    let axis = abs(coord) / derivative;
    if (axis.y < 1.5) { color = vec3<f32>(0.8, 0.2, 0.2); alpha = 1.0 - (axis.y/1.5); }
    if (axis.x < 1.5) { color = vec3<f32>(0.2, 0.8, 0.2); alpha = 1.0 - (axis.x/1.5); }

    let camWorld = scene.invViewProjection * vec4<f32>(0.0, 0.0, 0.5, 1.0);
    let camPos = camWorld.xyz / camWorld.w;
    let dist = length(coord - camPos.xy);
    let fade = 1.0 - smoothstep(100.0, 800.0, dist);

    alpha *= fade;
    if (alpha < 0.01) { discard; }

    return vec4<f32>(color, alpha);
}
)";

    } // namespace Shaders
} // namespace BimCore