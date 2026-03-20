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
        uint32_t  _pad[3];          // 12 bytes  offset 132
    };

    struct HighlightRange {
        uint32_t startIndex;
        uint32_t indexCount;
    };

    // -----------------------------------------------------------------------------

    class GraphicsContext {
    public:
        GraphicsContext(GLFWwindow* window, int width, int height);
        ~GraphicsContext();

        GraphicsContext(const GraphicsContext&)            = delete;
        GraphicsContext& operator=(const GraphicsContext&) = delete;

        // --- Mesh setup ---
        void UploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        void UpdateGeometry(const std::vector<Vertex>& vertices);

        // --- Dynamic frame updates ---
        void UpdateScene(const SceneUniforms& uniforms);
        void UpdateInstanceData(const std::vector<glm::mat4>& transforms);
        void UpdateActiveIndices(const std::vector<uint32_t>& solidIdx, const std::vector<uint32_t>& transparentIdx);

        // --- Feature toggles ---
        void SetHighlight(bool active, const std::vector<HighlightRange>& ranges, int style);
        void SetBoundingBox(bool active, const glm::vec3& minB, const glm::vec3& maxB);

        // --- NEW: Clipping planes with customizable colors ---
        void SetClippingPlanes(bool activeX, float x, const float* colX,
                               bool activeY, float y, const float* colY,
                               bool activeZ, float z, const float* colZ,
                               const glm::vec3& minB,
                               const glm::vec3& maxB);

        // --- Core events ---
        void Resize(int newWidth, int newHeight);
        void RenderFrame();

        // --- ImGui ---
        void InitImGui(GLFWwindow* window);
        void ShutdownImGui();

    private:
        void CreateSurface(GLFWwindow* window);
        void RequestAdapterAndDevice();
        void ConfigureSurface();
        void CreateDepthTexture();
        void ReleaseDepthTexture();
        void CreateUniformBuffers();

        WGPUShaderModule CreateShaderModule(const char* wgsl) const;
        void CreateMainPipeline();
        void CreateHighlightPipeline();
        void CreateAABBPipeline();
        void CreateGlassPipeline();
        void AllocateGeometryBuffers();

    private:
        // --- WebGPU core ---
        WGPUInstance       m_instance = nullptr;
        WGPUSurface        m_surface  = nullptr;
        WGPUAdapter        m_adapter  = nullptr;
        WGPUDevice         m_device   = nullptr;
        WGPUQueue          m_queue    = nullptr;
        WGPUTextureFormat  m_surfaceFormat = WGPUTextureFormat_Undefined;

        uint32_t           m_width    = 0;
        uint32_t           m_height   = 0;

        WGPUTexture        m_depthTexture = nullptr;
        WGPUTextureView    m_depthView    = nullptr;

        // --- Bind Groups ---
        WGPUBuffer         m_uniformBuffer  = nullptr;
        WGPUBuffer         m_instanceBuffer = nullptr;
        uint32_t           m_instanceCount  = 0;
        WGPUBindGroup      m_sceneBindGroup = nullptr;

        // --- Pipelines ---
        WGPURenderPipeline m_pipeline            = nullptr;
        WGPURenderPipeline m_transparentPipeline = nullptr;
        WGPURenderPipeline m_highlightSolidPipeline   = nullptr;
        WGPURenderPipeline m_highlightOutlinePipeline = nullptr;
        WGPURenderPipeline m_aabbPipeline             = nullptr;
        WGPURenderPipeline m_glassPipeline            = nullptr;

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
        int                       m_highlightStyle  = 0;
        std::vector<HighlightRange> m_highlightRanges;
    };

} // namespace BimCore
