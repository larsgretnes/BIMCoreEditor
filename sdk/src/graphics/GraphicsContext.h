#pragma once
// =============================================================================
// BimCore/graphics/GraphicsContext.h
// Owns the WebGPU device, surface, pipelines, and all GPU buffers.
// Nothing GPU-specific leaks past this header.
// =============================================================================
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

struct GLFWwindow;
struct ImDrawData;

namespace BimCore {

// ---- GPU-side data types (shared with scene layer) -------------------------

struct Vertex {
    float position[3];
    float normal[3];
    float color[3];
};

// Matches the WGSL struct layout exactly — keep fields 16-byte aligned
struct SceneUniforms {
    glm::mat4 viewProjection;   // 64 bytes  offset 0
    glm::vec4 sunDirection;     // 16 bytes  offset 64
    glm::vec4 highlightColor;   // 16 bytes  offset 80
    glm::vec4 clipDistances;    // 16 bytes  offset 96   (x, y, z, unused)
    glm::vec4 clipActive;       // 16 bytes  offset 112  (x, y, z, unused) — 1.0 = active
    uint32_t  lightingMode;     //  4 bytes  offset 128
    uint32_t  _pad[3];          // 12 bytes  padding     total = 144 bytes
};

struct HighlightRange {
    uint32_t startIndex;
    uint32_t indexCount;
};

// ---- Main class -------------------------------------------------------------

class GraphicsContext {
public:
    GraphicsContext(GLFWwindow* window, int width, int height);
    ~GraphicsContext();

    GraphicsContext(const GraphicsContext&)            = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;

    // --- Per-frame interface ---
    void UpdateScene(const SceneUniforms& uniforms);
    void UpdateInstanceData(const std::vector<glm::mat4>& transforms);
    void RenderFrame();

    // --- Geometry ---
    void UploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void UpdateGeometry(const std::vector<Vertex>& vertices);    // CPU→GPU vertex upload (colour edits, explode)
    void UpdateActiveIndices(const std::vector<uint32_t>& solidIdx,
                             const std::vector<uint32_t>& transparentIdx);

    // --- Resize ---
    void Resize(int newWidth, int newHeight);

    // --- Scene state ---
    void SetHighlight(bool active,
                      const std::vector<HighlightRange>& ranges,
                      int style = 0);
    void SetBoundingBox(bool active,
                        const glm::vec3& minB = glm::vec3(0.0f),
                        const glm::vec3& maxB = glm::vec3(0.0f));
    void SetClippingPlanes(bool activeX, float x,
                           bool activeY, float y,
                           bool activeZ, float z,
                           const glm::vec3& minB,
                           const glm::vec3& maxB);

    // --- ImGui integration ---
    void InitImGui(GLFWwindow* window);
    void ShutdownImGui();

private:
    // Initialisation helpers
    void CreateSurface(GLFWwindow* window);
    void RequestAdapterAndDevice();
    void ConfigureSurface();
    void CreateDepthTexture();
    void CreateUniformBuffers();

    // Pipeline creation split into logical units
    void CreateMainPipeline();
    void CreateHighlightPipeline();
    void CreateAABBPipeline();
    void CreateGlassPipeline();
    void AllocateGeometryBuffers();

    // Internal helpers
    WGPUShaderModule CreateShaderModule(const char* wgsl) const;
    void             ReleaseDepthTexture();

    // --- WebGPU core objects ---
    WGPUInstance m_instance = nullptr;
    WGPUSurface  m_surface  = nullptr;
    WGPUAdapter  m_adapter  = nullptr;
    WGPUDevice   m_device   = nullptr;
    WGPUQueue    m_queue    = nullptr;

    WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_Undefined;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;

    // --- Depth buffer ---
    WGPUTexture     m_depthTexture = nullptr;
    WGPUTextureView m_depthView    = nullptr;

    // --- Uniform / bind-group ---
    WGPUBuffer     m_uniformBuffer    = nullptr;
    WGPUBuffer     m_instanceBuffer   = nullptr;
    WGPUBindGroup  m_sceneBindGroup   = nullptr;

    // --- Pipelines ---
    WGPURenderPipeline m_pipeline            = nullptr; // opaque solid
    WGPURenderPipeline m_transparentPipeline = nullptr; // IFC glass
    WGPURenderPipeline m_highlightSolidPipeline   = nullptr;
    WGPURenderPipeline m_highlightOutlinePipeline = nullptr;
    WGPURenderPipeline m_aabbPipeline             = nullptr;
    WGPURenderPipeline m_glassPipeline            = nullptr; // clipping-plane quads

    // --- Mesh buffers ---
    WGPUBuffer m_vertexBuffer = nullptr;
    WGPUBuffer m_indexBuffer  = nullptr;
    uint32_t   m_indexCount   = 0;

    WGPUBuffer m_activeIndexBuffer          = nullptr;
    uint32_t   m_activeIndexCount           = 0;
    WGPUBuffer m_activeTransparentIndexBuffer = nullptr;
    uint32_t   m_activeTransparentIndexCount  = 0;
    WGPUBuffer m_lineIndexBuffer            = nullptr;

    // --- AABB / bounding-box buffers ---
    WGPUBuffer m_aabbVertexBuffer = nullptr;
    WGPUBuffer m_aabbIndexBuffer  = nullptr;
    bool       m_showAABB         = false;

    // --- Glass clipping plane buffers ---
    WGPUBuffer m_glassVertexBuffer = nullptr;
    WGPUBuffer m_glassIndexBuffer  = nullptr;
    uint32_t   m_glassIndexCount   = 0;

    // --- Highlight state ---
    bool                      m_hasHighlight    = false;
    std::vector<HighlightRange> m_highlightRanges;
    int                       m_highlightStyle  = 0;

    // --- Instance count ---
    uint32_t m_instanceCount = 0;
};

} // namespace BimCore
