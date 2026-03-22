// =============================================================================
// BimCore/graphics/GraphicsContext.h
// =============================================================================
#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <vector>
#include <map>
#include <cstdint>
#include <string>

struct GLFWwindow;
struct ImDrawData;

namespace BimCore {

    struct Vertex {
        float position[3];
        float normal[3];
        float color[3];
        float uv[2];
    };

    struct SceneUniforms {
        glm::mat4 viewProjection;       // offset 0   (64 bytes)
        glm::mat4 invViewProjection;    // offset 64  (64 bytes)
        glm::vec4 sunDirection;         // offset 128 (16 bytes)
        glm::vec4 highlightColor;       // offset 144 (16 bytes)
        glm::vec4 clipMin;              // offset 160 (16 bytes)
        glm::vec4 clipMax;              // offset 176 (16 bytes)
        glm::vec4 clipActiveMin;        // offset 192 (16 bytes)
        glm::vec4 clipActiveMax;        // offset 208 (16 bytes)
        uint32_t  lightingMode;         // offset 224 (4 bytes)
        uint32_t  screenWidth;          // offset 228 (4 bytes)
        uint32_t  screenHeight;         // offset 232 (4 bytes)
        uint32_t  _pad;                 // offset 236 (4 bytes)
    };

    struct HighlightRange {
        uint32_t startIndex;
        uint32_t indexCount;
    };

    struct TextureData;

    class GraphicsContext {
    public:
        GraphicsContext(GLFWwindow* window, int width, int height);
        ~GraphicsContext();

        GraphicsContext(const GraphicsContext&)            = delete;
        GraphicsContext& operator=(const GraphicsContext&) = delete;

        void UploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        void UpdateGeometry(const std::vector<Vertex>& vertices);

        void UploadTextures(const std::vector<TextureData>& textures);

        void UpdateScene(const SceneUniforms& uniforms);
        void UpdateInstanceData(const std::vector<glm::mat4>& transforms);

        void UpdateActiveBatches(const std::map<int, std::vector<uint32_t>>& solidBatches,
                                 const std::map<int, std::vector<uint32_t>>& transparentBatches);

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
        void CreateUniformBuffers();

        void CreateRenderTargets();
        void ReleaseRenderTargets();

        void CreateDefaultMaterial();

        WGPUShaderModule CreateShaderModule(const std::string& source) const;
        void CreateMainPipeline();
        void CreateHighlightPipeline();
        void CreateAABBPipeline();
        void CreateGlassPipeline();
        void CreateStencilPipelines();
        void CreateSSAOPipeline();
        void CreateGridPipeline(); // --- NEW: Setup function for the Grid ---
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

        WGPUTexture        m_offscreenTexture = nullptr;
        WGPUTextureView    m_offscreenView    = nullptr;
        WGPUTexture        m_depthTexture     = nullptr;
        WGPUTextureView    m_depthView        = nullptr;
        WGPUTextureView    m_depthReadView    = nullptr;

        WGPUBuffer         m_uniformBuffer  = nullptr;
        WGPUBuffer         m_instanceBuffer = nullptr;
        uint32_t           m_instanceCount  = 0;

        WGPUBindGroupLayout m_sceneBindGroupLayout = nullptr;
        WGPUBindGroup       m_sceneBindGroup       = nullptr;

        WGPUBindGroupLayout m_ssaoBindGroupLayout = nullptr;
        WGPUBindGroup       m_ssaoBindGroup       = nullptr;
        WGPURenderPipeline  m_ssaoPipeline        = nullptr;

        struct GPUTexture {
            WGPUTexture texture = nullptr;
            WGPUTextureView view = nullptr;
            WGPUBindGroup bindGroup = nullptr;
        };
        std::vector<GPUTexture> m_gpuTextures;

        WGPUSampler         m_defaultSampler           = nullptr;
        WGPUTexture         m_defaultTexture           = nullptr;
        WGPUTextureView     m_defaultTextureView       = nullptr;
        WGPUBindGroupLayout m_materialBindGroupLayout  = nullptr;
        WGPUBindGroup       m_defaultMaterialBindGroup = nullptr;

        WGPURenderPipeline m_pipeline            = nullptr;
        WGPURenderPipeline m_transparentPipeline = nullptr;
        WGPURenderPipeline m_highlightSolidPipeline   = nullptr;
        WGPURenderPipeline m_highlightOutlinePipeline = nullptr;
        WGPURenderPipeline m_aabbPipeline             = nullptr;
        WGPURenderPipeline m_glassPipeline            = nullptr;
        WGPURenderPipeline m_stencilMaskPipeline      = nullptr;
        WGPURenderPipeline m_capPipeline              = nullptr;
        WGPURenderPipeline m_gridPipeline             = nullptr; // --- NEW: The compiled Grid shader ---

        WGPUBuffer m_vertexBuffer = nullptr;
        WGPUBuffer m_indexBuffer  = nullptr;
        uint32_t   m_indexCount   = 0;

        struct RenderBatch {
            uint32_t startIndex;
            uint32_t indexCount;
            int textureIndex;
        };
        std::vector<RenderBatch> m_solidBatches;
        std::vector<RenderBatch> m_transparentBatches;

        WGPUBuffer m_activeIndexBuffer          = nullptr;
        uint32_t   m_activeIndexCount           = 0;
        WGPUBuffer m_activeTransparentIndexBuffer = nullptr;
        uint32_t   m_activeTransparentIndexCount  = 0;

        WGPUBuffer m_lineIndexBuffer            = nullptr;
        WGPUBuffer m_aabbVertexBuffer = nullptr;
        WGPUBuffer m_aabbIndexBuffer  = nullptr;
        bool       m_showAABB         = false;

        WGPUBuffer m_capVertexBuffer  = nullptr;
        WGPUBuffer m_capIndexBuffer   = nullptr;
        uint32_t   m_capIndexCount    = 0;

        WGPUBuffer m_glassVertexBuffer = nullptr;
        WGPUBuffer m_glassIndexBuffer  = nullptr;
        uint32_t   m_glassIndexCount   = 0;

        bool                        m_hasHighlight    = false;
        int                         m_highlightStyle  = 0;
        std::vector<HighlightRange> m_highlightRanges;
    };

} // namespace BimCore
