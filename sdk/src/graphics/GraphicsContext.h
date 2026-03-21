// =============================================================================
// BimCore/graphics/GraphicsContext.h
// =============================================================================
#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

struct GLFWwindow;
struct ImDrawData;

namespace BimCore {

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
        glm::vec4 clipMin;          // 16 bytes  offset 96
        glm::vec4 clipMax;          // 16 bytes  offset 112
        glm::vec4 clipActiveMin;    // 16 bytes  offset 128
        glm::vec4 clipActiveMax;    // 16 bytes  offset 144
        uint32_t  lightingMode;     //  4 bytes  offset 160
        uint32_t  _pad[3];          // 12 bytes  offset 164
    };

    struct HighlightRange {
        uint32_t startIndex;
        uint32_t indexCount;
    };

    class GraphicsContext {
    public:
        GraphicsContext(GLFWwindow* window, int width, int height);
        ~GraphicsContext();

        GraphicsContext(const GraphicsContext&)            = delete;
        GraphicsContext& operator=(const GraphicsContext&) = delete;

        void UploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        void UpdateGeometry(const std::vector<Vertex>& vertices);

        void UpdateScene(const SceneUniforms& uniforms);
        void UpdateInstanceData(const std::vector<glm::mat4>& transforms);
        void UpdateActiveIndices(const std::vector<uint32_t>& solidIdx, const std::vector<uint32_t>& transparentIdx);

        void SetHighlight(bool active, const std::vector<HighlightRange>& ranges, int style);
        void SetBoundingBox(bool active, const glm::vec3& minB, const glm::vec3& maxB);

        void SetClippingPlanes(
            bool actXMin, float xMin, bool actXMax, float xMax, const float* colX,
            bool actYMin, float yMin, bool actYMax, float yMax, const float* colY,
            bool actZMin, float zMin, bool actZMax, float zMax, const float* colZ,
            const glm::vec3& minB, const glm::vec3& maxB);

        void Resize(int newWidth, int newHeight);
        void RenderFrame();

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

        WGPUBuffer         m_uniformBuffer  = nullptr;
        WGPUBuffer         m_instanceBuffer = nullptr;
        uint32_t           m_instanceCount  = 0;
        WGPUBindGroup      m_sceneBindGroup = nullptr;

        WGPURenderPipeline m_pipeline            = nullptr;
        WGPURenderPipeline m_transparentPipeline = nullptr;
        WGPURenderPipeline m_highlightSolidPipeline   = nullptr;
        WGPURenderPipeline m_highlightOutlinePipeline = nullptr;
        WGPURenderPipeline m_aabbPipeline             = nullptr;
        WGPURenderPipeline m_glassPipeline            = nullptr;

        WGPUBuffer m_vertexBuffer = nullptr;
        WGPUBuffer m_indexBuffer  = nullptr;
        uint32_t   m_indexCount   = 0;

        WGPUBuffer m_activeIndexBuffer          = nullptr;
        uint32_t   m_activeIndexCount           = 0;
        WGPUBuffer m_activeTransparentIndexBuffer = nullptr;
        uint32_t   m_activeTransparentIndexCount  = 0;
        WGPUBuffer m_lineIndexBuffer            = nullptr;

        WGPUBuffer m_aabbVertexBuffer = nullptr;
        WGPUBuffer m_aabbIndexBuffer  = nullptr;
        bool       m_showAABB         = false;

        WGPUBuffer m_glassVertexBuffer = nullptr;
        WGPUBuffer m_glassIndexBuffer  = nullptr;
        uint32_t   m_glassIndexCount   = 0;

        bool                        m_hasHighlight    = false;
        int                         m_highlightStyle  = 0;
        std::vector<HighlightRange> m_highlightRanges;
    };

} // namespace BimCore
