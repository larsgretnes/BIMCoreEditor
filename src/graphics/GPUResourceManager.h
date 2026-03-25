// =============================================================================
// BimCore/graphics/GPUResourceManager.h
// =============================================================================
#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <vector>
#include <map>
#include <string>

#include "GraphicsContext.h"
#include "PipelineManager.h"

namespace BimCore {

    class GPUResourceManager {
    public:
        GPUResourceManager(WGPUDevice device, WGPUQueue queue, PipelineManager* pipelineMgr);
        ~GPUResourceManager();

        void UploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        void UpdateGeometry(const std::vector<Vertex>& vertices);
        void UploadTextures(const std::vector<TextureData>& textures);
        void UpdateScene(const SceneUniforms& uniforms, WGPUTextureView shadowView, WGPUSampler shadowSampler);
        void UpdateInstanceData(const std::vector<glm::mat4>& transforms);

        WGPUBindGroup GetSceneBindGroup() const { return m_sceneBindGroup; }
        WGPUBindGroup GetShadowBindGroup() const { return m_shadowBindGroup; }
        WGPUBindGroup GetDefaultMaterialBindGroup() const { return m_defaultMaterialBindGroup; }
        
        WGPUBuffer GetUniformBuffer() const { return m_uniformBuffer; } // <--- THE FIX

        WGPUBuffer GetVertexBuffer() const { return m_vertexBuffer; }
        WGPUBuffer GetIndexBuffer() const { return m_indexBuffer; }
        WGPUBuffer GetActiveIndexBuffer() const { return m_activeIndexBuffer; }
        WGPUBuffer GetActiveTransparentIndexBuffer() const { return m_activeTransparentIndexBuffer; }
        WGPUBuffer GetLineIndexBuffer() const { return m_lineIndexBuffer; }
        WGPUBuffer GetAABBVertexBuffer() const { return m_aabbVertexBuffer; }
        WGPUBuffer GetAABBIndexBuffer() const { return m_aabbIndexBuffer; }
        WGPUBuffer GetCapVertexBuffer() const { return m_capVertexBuffer; }
        WGPUBuffer GetCapIndexBuffer() const { return m_capIndexBuffer; }
        WGPUBuffer GetGlassVertexBuffer() const { return m_glassVertexBuffer; }
        WGPUBuffer GetGlassIndexBuffer() const { return m_glassIndexBuffer; }

        uint32_t GetIndexCount() const { return m_indexCount; }
        uint32_t GetActiveIndexCount() const { return m_activeIndexCount; }
        uint32_t GetActiveTransparentIndexCount() const { return m_activeTransparentIndexCount; }
        uint32_t GetCapIndexCount() const { return m_capIndexCount; }
        uint32_t GetGlassIndexCount() const { return m_glassIndexCount; }
        
        WGPUBindGroup GetTextureBindGroup(int index) const;

        void UpdateActiveBatches(const std::map<int, std::vector<uint32_t>>& solidBatches,
                                 const std::map<int, std::vector<uint32_t>>& transparentBatches);

        void SetBoundingBox(bool active, const glm::vec3& minB, const glm::vec3& maxB);
        void SetClippingPlanes(
            bool actXMin, float xMin, bool actXMax, float xMax, const float* colX,
            bool actYMin, float yMin, bool actYMax, float yMax, const float* colY,
            bool actZMin, float zMin, bool actZMax, float zMax, const float* colZ,
            const glm::vec3& minB, const glm::vec3& maxB);

        struct RenderBatch {
            uint32_t startIndex;
            uint32_t indexCount;
            int textureIndex;
        };
        const std::vector<RenderBatch>& GetSolidBatches() const { return m_solidBatches; }
        const std::vector<RenderBatch>& GetTransparentBatches() const { return m_transparentBatches; }

    private:
        void CreateDefaultMaterial();
        void AllocateGeometryBuffers();

    private:
        WGPUDevice m_device;
        WGPUQueue m_queue;
        PipelineManager* m_pipelineMgr;

        WGPUBuffer m_uniformBuffer = nullptr;
        WGPUBuffer m_instanceBuffer = nullptr;
        uint32_t m_instanceCount = 0;

        WGPUBindGroup m_sceneBindGroup = nullptr;
        WGPUBindGroup m_shadowBindGroup = nullptr;

        WGPUSampler m_defaultSampler = nullptr;
        WGPUTexture m_defaultTexture = nullptr;
        WGPUTextureView m_defaultTextureView = nullptr;
        WGPUBindGroup m_defaultMaterialBindGroup = nullptr;

        struct GPUTexture {
            WGPUTexture texture = nullptr;
            WGPUTextureView view = nullptr;
            WGPUBindGroup bindGroup = nullptr;
        };
        std::vector<GPUTexture> m_gpuTextures;

        WGPUBuffer m_vertexBuffer = nullptr;
        WGPUBuffer m_indexBuffer = nullptr;
        uint32_t m_indexCount = 0;

        WGPUBuffer m_activeIndexBuffer = nullptr;
        uint32_t m_activeIndexCount = 0;
        WGPUBuffer m_activeTransparentIndexBuffer = nullptr;
        uint32_t m_activeTransparentIndexCount = 0;

        std::vector<RenderBatch> m_solidBatches;
        std::vector<RenderBatch> m_transparentBatches;

        WGPUBuffer m_lineIndexBuffer = nullptr;
        WGPUBuffer m_aabbVertexBuffer = nullptr;
        WGPUBuffer m_aabbIndexBuffer = nullptr;
        WGPUBuffer m_capVertexBuffer = nullptr;
        WGPUBuffer m_capIndexBuffer = nullptr;
        uint32_t m_capIndexCount = 0;
        WGPUBuffer m_glassVertexBuffer = nullptr;
        WGPUBuffer m_glassIndexBuffer = nullptr;
        uint32_t m_glassIndexCount = 0;
    };

} // namespace BimCore