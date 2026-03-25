// =============================================================================
// BimCore/graphics/GraphicsContext.h
// =============================================================================
#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>      // <--- THE FIX: Re-added GLM
#include <memory>
#include <vector>
#include <map>
#include <string>           // <--- THE FIX: Re-added String for TextureData

struct GLFWwindow;

namespace BimCore {

    struct Vertex { float position[3]; float normal[3]; float color[3]; float uv[2]; };
    struct TextureData { uint32_t width; uint32_t height; uint32_t channels; std::vector<uint8_t> pixels; std::string name; };
    struct HighlightRange { uint32_t startIndex; uint32_t indexCount; };
    
    struct SceneUniforms {
        glm::mat4 viewProjection;       // offset 0   (64 bytes)
        glm::mat4 invViewProjection;    // offset 64  (64 bytes)
        glm::mat4 lightSpaceMatrix;     // offset 128 (64 bytes)
        glm::vec4 sunDirection;         // offset 192 (16 bytes)
        glm::vec4 highlightColor;       // offset 208 (16 bytes)
        glm::vec4 clipMin;              // offset 224 (16 bytes)
        glm::vec4 clipMax;              // offset 240 (16 bytes)
        glm::vec4 clipActiveMin;        // offset 256 (16 bytes)
        glm::vec4 clipActiveMax;        // offset 272 (16 bytes)
        uint32_t  lightingMode;         // offset 288 (4 bytes)
        uint32_t  screenWidth;          // offset 292 (4 bytes)
        uint32_t  screenHeight;         // offset 296 (4 bytes)
        uint32_t  _pad;                 // offset 300 (4 bytes)
    };

    class PipelineManager;
    class GPUResourceManager;

    class GraphicsContext {
    public:
        GraphicsContext(GLFWwindow* window, int width, int height);
        ~GraphicsContext();

        GraphicsContext(const GraphicsContext&) = delete;
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
        void CreateRenderTargets();
        void ReleaseRenderTargets();

    private:
        WGPUInstance m_instance = nullptr;
        WGPUSurface m_surface = nullptr;
        WGPUAdapter m_adapter = nullptr;
        WGPUDevice m_device = nullptr;
        WGPUQueue m_queue = nullptr;
        WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_Undefined;

        uint32_t m_width = 0;
        uint32_t m_height = 0;

        std::unique_ptr<PipelineManager> m_pipelineMgr;
        std::unique_ptr<GPUResourceManager> m_resourceMgr;

        WGPUTexture m_offscreenTexture = nullptr;
        WGPUTextureView m_offscreenView = nullptr;
        WGPUTexture m_depthTexture = nullptr;
        WGPUTextureView m_depthView = nullptr;
        WGPUTextureView m_depthReadView = nullptr;
        WGPUTexture m_shadowTexture = nullptr;
        WGPUTextureView m_shadowView = nullptr;
        WGPUSampler m_shadowSampler = nullptr;

        WGPUBindGroup m_ssaoBindGroup = nullptr;

        bool m_hasHighlight = false;
        int m_highlightStyle = 0;
        std::vector<HighlightRange> m_highlightRanges;
        bool m_showAABB = false;
    };

} // namespace BimCore