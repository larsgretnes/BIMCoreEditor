// =============================================================================
// BimCore/graphics/GPUResourceManager.cpp
// =============================================================================
#include "GPUResourceManager.h"
#include <stdexcept>

namespace BimCore {

    GPUResourceManager::GPUResourceManager(WGPUDevice device, WGPUQueue queue, PipelineManager* pipelineMgr)
        : m_device(device), m_queue(queue), m_pipelineMgr(pipelineMgr) {
        
        WGPUBufferDescriptor ubDesc = {};
        ubDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
        ubDesc.size  = sizeof(SceneUniforms);
        m_uniformBuffer = wgpuDeviceCreateBuffer(m_device, &ubDesc);

        WGPUBufferDescriptor instDesc = {};
        instDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage;
        instDesc.size  = 1'000'000 * sizeof(glm::mat4);
        m_instanceBuffer = wgpuDeviceCreateBuffer(m_device, &instDesc);

        CreateDefaultMaterial();
        AllocateGeometryBuffers();
    }

    GPUResourceManager::~GPUResourceManager() {
        if (m_capIndexBuffer) wgpuBufferRelease(m_capIndexBuffer);
        if (m_capVertexBuffer) wgpuBufferRelease(m_capVertexBuffer);
        if (m_glassIndexBuffer) wgpuBufferRelease(m_glassIndexBuffer);
        if (m_glassVertexBuffer) wgpuBufferRelease(m_glassVertexBuffer);
        if (m_aabbIndexBuffer) wgpuBufferRelease(m_aabbIndexBuffer);
        if (m_aabbVertexBuffer) wgpuBufferRelease(m_aabbVertexBuffer);
        if (m_lineIndexBuffer) wgpuBufferRelease(m_lineIndexBuffer);
        if (m_activeTransparentIndexBuffer) wgpuBufferRelease(m_activeTransparentIndexBuffer);
        if (m_activeIndexBuffer) wgpuBufferRelease(m_activeIndexBuffer);
        if (m_indexBuffer) wgpuBufferRelease(m_indexBuffer);
        if (m_vertexBuffer) wgpuBufferRelease(m_vertexBuffer);

        for (auto& gt : m_gpuTextures) {
            if (gt.bindGroup) wgpuBindGroupRelease(gt.bindGroup);
            if (gt.view) wgpuTextureViewRelease(gt.view);
            if (gt.texture) wgpuTextureRelease(gt.texture);
        }

        if (m_defaultMaterialBindGroup) wgpuBindGroupRelease(m_defaultMaterialBindGroup);
        if (m_defaultTextureView) wgpuTextureViewRelease(m_defaultTextureView);
        if (m_defaultTexture) wgpuTextureRelease(m_defaultTexture);
        if (m_defaultSampler) wgpuSamplerRelease(m_defaultSampler);

        if (m_shadowBindGroup) wgpuBindGroupRelease(m_shadowBindGroup);
        if (m_sceneBindGroup) wgpuBindGroupRelease(m_sceneBindGroup);

        if (m_instanceBuffer) wgpuBufferRelease(m_instanceBuffer);
        if (m_uniformBuffer) wgpuBufferRelease(m_uniformBuffer);
    }

    void GPUResourceManager::CreateDefaultMaterial() {
        WGPUSamplerDescriptor sampDesc = {};
        sampDesc.addressModeU = WGPUAddressMode_Repeat;
        sampDesc.addressModeV = WGPUAddressMode_Repeat;
        sampDesc.addressModeW = WGPUAddressMode_Repeat;
        sampDesc.magFilter = WGPUFilterMode_Linear;
        sampDesc.minFilter = WGPUFilterMode_Linear;
        sampDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
        sampDesc.maxAnisotropy = 1;
        m_defaultSampler = wgpuDeviceCreateSampler(m_device, &sampDesc);

        WGPUTextureDescriptor texDesc = {};
        texDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        texDesc.dimension = WGPUTextureDimension_2D;
        texDesc.size = {1, 1, 1};
        texDesc.format = WGPUTextureFormat_RGBA8Unorm;
        texDesc.mipLevelCount = 1;
        texDesc.sampleCount = 1;
        m_defaultTexture = wgpuDeviceCreateTexture(m_device, &texDesc);

        WGPUTexelCopyTextureInfo dest = {}; dest.texture = m_defaultTexture; dest.mipLevel = 0; dest.origin = {0, 0, 0};
        WGPUTexelCopyBufferLayout dataLayout = {}; dataLayout.offset = 0; dataLayout.bytesPerRow = 4; dataLayout.rowsPerImage = 1;
        uint8_t whitePixel[4] = {255, 255, 255, 255};
        WGPUExtent3D size = {1, 1, 1};
        wgpuQueueWriteTexture(m_queue, &dest, whitePixel, 4, &dataLayout, &size);

        WGPUTextureViewDescriptor viewDesc = {};
        viewDesc.format = WGPUTextureFormat_RGBA8Unorm; viewDesc.dimension = WGPUTextureViewDimension_2D;
        viewDesc.baseMipLevel = 0; viewDesc.mipLevelCount = 1; viewDesc.baseArrayLayer = 0; viewDesc.arrayLayerCount = 1; viewDesc.aspect = WGPUTextureAspect_All;
        m_defaultTextureView = wgpuTextureCreateView(m_defaultTexture, &viewDesc);

        WGPUBindGroupEntry bgEntries[2] = {};
        bgEntries[0].binding = 0; bgEntries[0].textureView = m_defaultTextureView;
        bgEntries[1].binding = 1; bgEntries[1].sampler = m_defaultSampler;
        WGPUBindGroupDescriptor bgDesc = {}; bgDesc.layout = m_pipelineMgr->GetMaterialLayout(); bgDesc.entryCount = 2; bgDesc.entries = bgEntries;
        m_defaultMaterialBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);
    }

    void GPUResourceManager::AllocateGeometryBuffers() {
        WGPUBufferDescriptor avbd = {}; avbd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; avbd.size = 8 * sizeof(Vertex);
        m_aabbVertexBuffer = wgpuDeviceCreateBuffer(m_device, &avbd);

        constexpr uint32_t kAABBIndices[] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
        WGPUBufferDescriptor aibd = {}; aibd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index; aibd.size = sizeof(kAABBIndices);
        m_aabbIndexBuffer = wgpuDeviceCreateBuffer(m_device, &aibd);
        wgpuQueueWriteBuffer(m_queue, m_aabbIndexBuffer, 0, kAABBIndices, sizeof(kAABBIndices));

        WGPUBufferDescriptor cvbd = {}; cvbd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; cvbd.size = 24 * sizeof(Vertex);
        m_capVertexBuffer = wgpuDeviceCreateBuffer(m_device, &cvbd);
        WGPUBufferDescriptor cibd = {}; cibd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index; cibd.size = 36 * sizeof(uint32_t);
        m_capIndexBuffer = wgpuDeviceCreateBuffer(m_device, &cibd);

        WGPUBufferDescriptor gvbd = {}; gvbd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; gvbd.size = 24 * sizeof(Vertex);
        m_glassVertexBuffer = wgpuDeviceCreateBuffer(m_device, &gvbd);
        WGPUBufferDescriptor gibd = {}; gibd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index; gibd.size = 36 * sizeof(uint32_t);
        m_glassIndexBuffer = wgpuDeviceCreateBuffer(m_device, &gibd);
    }

    void GPUResourceManager::UploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
        if (vertices.empty() || indices.empty()) return;
        m_indexCount = static_cast<uint32_t>(indices.size());

        if (m_vertexBuffer) wgpuBufferRelease(m_vertexBuffer);
        if (m_indexBuffer) wgpuBufferRelease(m_indexBuffer);
        if (m_activeIndexBuffer) wgpuBufferRelease(m_activeIndexBuffer);
        if (m_activeTransparentIndexBuffer) wgpuBufferRelease(m_activeTransparentIndexBuffer);
        if (m_lineIndexBuffer) wgpuBufferRelease(m_lineIndexBuffer);

        auto makeBuffer = [&](WGPUBufferUsage usage, const void* data, size_t byteSize) {
            WGPUBufferDescriptor desc = {}; desc.usage = WGPUBufferUsage_CopyDst | usage; desc.size  = byteSize;
            WGPUBuffer buf = wgpuDeviceCreateBuffer(m_device, &desc);
            wgpuQueueWriteBuffer(m_queue, buf, 0, data, byteSize);
            return buf;
        };

        m_vertexBuffer = makeBuffer(WGPUBufferUsage_Vertex, vertices.data(), vertices.size() * sizeof(Vertex));
        m_indexBuffer  = makeBuffer(WGPUBufferUsage_Index,  indices.data(),  indices.size()  * sizeof(uint32_t));

        WGPUBufferDescriptor desc = {}; desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index; desc.size  = indices.size() * sizeof(uint32_t);
        m_activeIndexBuffer = wgpuDeviceCreateBuffer(m_device, &desc);
        m_activeTransparentIndexBuffer = wgpuDeviceCreateBuffer(m_device, &desc);
        m_activeIndexCount = 0; m_activeTransparentIndexCount = 0;

        std::vector<uint32_t> lineIndices; lineIndices.reserve(indices.size() * 2);
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            lineIndices.push_back(indices[i]);   lineIndices.push_back(indices[i+1]);
            lineIndices.push_back(indices[i+1]); lineIndices.push_back(indices[i+2]);
            lineIndices.push_back(indices[i+2]); lineIndices.push_back(indices[i]);
        }
        m_lineIndexBuffer = makeBuffer(WGPUBufferUsage_Index, lineIndices.data(), lineIndices.size() * sizeof(uint32_t));
    }

    void GPUResourceManager::UpdateGeometry(const std::vector<Vertex>& vertices) {
        if (m_vertexBuffer && !vertices.empty())
            wgpuQueueWriteBuffer(m_queue, m_vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(Vertex));
    }

    void GPUResourceManager::UpdateActiveBatches(const std::map<int, std::vector<uint32_t>>& solidMap, const std::map<int, std::vector<uint32_t>>& transMap) {
        std::vector<uint32_t> flatSolid; m_solidBatches.clear();
        for (const auto& [texIdx, inds] : solidMap) {
            if (inds.empty()) continue;
            RenderBatch b; b.startIndex = static_cast<uint32_t>(flatSolid.size()); b.indexCount = static_cast<uint32_t>(inds.size()); b.textureIndex = texIdx;
            m_solidBatches.push_back(b); flatSolid.insert(flatSolid.end(), inds.begin(), inds.end());
        }
        m_activeIndexCount = static_cast<uint32_t>(flatSolid.size());
        if (m_activeIndexBuffer && m_activeIndexCount > 0) wgpuQueueWriteBuffer(m_queue, m_activeIndexBuffer, 0, flatSolid.data(), flatSolid.size() * sizeof(uint32_t));

        std::vector<uint32_t> flatTrans; m_transparentBatches.clear();
        for (const auto& [texIdx, inds] : transMap) {
            if (inds.empty()) continue;
            RenderBatch b; b.startIndex = static_cast<uint32_t>(flatTrans.size()); b.indexCount = static_cast<uint32_t>(inds.size()); b.textureIndex = texIdx;
            m_transparentBatches.push_back(b); flatTrans.insert(flatTrans.end(), inds.begin(), inds.end());
        }
        m_activeTransparentIndexCount = static_cast<uint32_t>(flatTrans.size());
        if (m_activeTransparentIndexBuffer && m_activeTransparentIndexCount > 0) wgpuQueueWriteBuffer(m_queue, m_activeTransparentIndexBuffer, 0, flatTrans.data(), flatTrans.size() * sizeof(uint32_t));
    }

    void GPUResourceManager::UploadTextures(const std::vector<TextureData>& textures) {
        for (auto& gt : m_gpuTextures) {
            if (gt.bindGroup) wgpuBindGroupRelease(gt.bindGroup);
            if (gt.view) wgpuTextureViewRelease(gt.view);
            if (gt.texture) wgpuTextureRelease(gt.texture);
        }
        m_gpuTextures.clear();

        for (const auto& texData : textures) {
            WGPUTextureDescriptor desc = {}; desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst; desc.dimension = WGPUTextureDimension_2D; desc.size = { texData.width, texData.height, 1 }; desc.format = WGPUTextureFormat_RGBA8Unorm; desc.mipLevelCount = 1; desc.sampleCount = 1;
            GPUTexture gt; gt.texture = wgpuDeviceCreateTexture(m_device, &desc);
            WGPUExtent3D extent = { texData.width, texData.height, 1 }; WGPUTexelCopyTextureInfo dest = {}; dest.texture = gt.texture;
            WGPUTexelCopyBufferLayout layout = {}; layout.bytesPerRow = texData.width * 4; layout.rowsPerImage = texData.height;
            wgpuQueueWriteTexture(m_queue, &dest, texData.pixels.data(), texData.pixels.size(), &layout, &extent);
            WGPUTextureViewDescriptor viewDesc = {}; viewDesc.format = WGPUTextureFormat_RGBA8Unorm; viewDesc.dimension = WGPUTextureViewDimension_2D; viewDesc.baseMipLevel = 0; viewDesc.mipLevelCount = 1; viewDesc.baseArrayLayer = 0; viewDesc.arrayLayerCount = 1; viewDesc.aspect = WGPUTextureAspect_All;
            gt.view = wgpuTextureCreateView(gt.texture, &viewDesc);

            WGPUBindGroupEntry entries[2] = {}; entries[0].binding = 0; entries[0].textureView = gt.view; entries[1].binding = 1; entries[1].sampler = m_defaultSampler;
            WGPUBindGroupDescriptor bgDesc = {}; bgDesc.layout = m_pipelineMgr->GetMaterialLayout(); bgDesc.entryCount = 2; bgDesc.entries = entries;
            gt.bindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);
            m_gpuTextures.push_back(gt);
        }
    }

    WGPUBindGroup GPUResourceManager::GetTextureBindGroup(int index) const {
        if (index >= 0 && index < static_cast<int>(m_gpuTextures.size())) {
            return m_gpuTextures[index].bindGroup;
        }
        return m_defaultMaterialBindGroup;
    }

    void GPUResourceManager::UpdateScene(const SceneUniforms& uniforms, WGPUTextureView shadowView, WGPUSampler shadowSampler) {
        if (m_uniformBuffer) wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0, &uniforms, sizeof(SceneUniforms));

        if (!m_sceneBindGroup) {
            WGPUBindGroupEntry bgEntries[2] = {};
            bgEntries[0].binding = 0; bgEntries[0].buffer = m_uniformBuffer; bgEntries[0].size = sizeof(SceneUniforms);
            bgEntries[1].binding = 1; bgEntries[1].buffer = m_instanceBuffer; bgEntries[1].size = 1'000'000 * sizeof(glm::mat4);
            WGPUBindGroupDescriptor bgDesc = {}; bgDesc.layout = m_pipelineMgr->GetSceneLayout(); bgDesc.entryCount = 2; bgDesc.entries = bgEntries;
            m_sceneBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);
        }

        if (!m_shadowBindGroup && shadowView && shadowSampler) {
            WGPUBindGroupEntry bgEntries[2] = {};
            bgEntries[0].binding = 0; bgEntries[0].textureView = shadowView;
            bgEntries[1].binding = 1; bgEntries[1].sampler = shadowSampler;
            WGPUBindGroupDescriptor bgDesc = {}; bgDesc.layout = m_pipelineMgr->GetShadowLayout(); bgDesc.entryCount = 2; bgDesc.entries = bgEntries;
            m_shadowBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);
        }
    }

    void GPUResourceManager::UpdateInstanceData(const std::vector<glm::mat4>& transforms) {
        if (!m_instanceBuffer || transforms.empty()) return;
        m_instanceCount = static_cast<uint32_t>(transforms.size());
        wgpuQueueWriteBuffer(m_queue, m_instanceBuffer, 0, transforms.data(), transforms.size() * sizeof(glm::mat4));
    }

    void GPUResourceManager::SetBoundingBox(bool active, const glm::vec3& minB, const glm::vec3& maxB) {
        if (!active) return;
        Vertex v[8] = {};
        v[0].position[0]=minB.x; v[0].position[1]=minB.y; v[0].position[2]=minB.z; v[1].position[0]=maxB.x; v[1].position[1]=minB.y; v[1].position[2]=minB.z; v[2].position[0]=maxB.x; v[2].position[1]=minB.y; v[2].position[2]=maxB.z; v[3].position[0]=minB.x; v[3].position[1]=minB.y; v[3].position[2]=maxB.z; v[4].position[0]=minB.x; v[4].position[1]=maxB.y; v[4].position[2]=minB.z; v[5].position[0]=maxB.x; v[5].position[1]=maxB.y; v[5].position[2]=minB.z; v[6].position[0]=maxB.x; v[6].position[1]=maxB.y; v[6].position[2]=maxB.z; v[7].position[0]=minB.x; v[7].position[1]=maxB.y; v[7].position[2]=maxB.z;
        wgpuQueueWriteBuffer(m_queue, m_aabbVertexBuffer, 0, v, sizeof(v));
    }

    void GPUResourceManager::SetClippingPlanes(bool actXMin, float xMin, bool actXMax, float xMax, const float* colX, bool actYMin, float yMin, bool actYMax, float yMax, const float* colY, bool actZMin, float zMin, bool actZMax, float zMax, const float* colZ, const glm::vec3& minB, const glm::vec3& maxB) {
        std::vector<Vertex> glassVerts, capVerts; std::vector<uint32_t> glassInds, capInds; glassVerts.reserve(24); glassInds.reserve(36); capVerts.reserve(24); capInds.reserve(36);
        const glm::vec3 minP = minB - glm::vec3(2.0f); const glm::vec3 maxP = maxB + glm::vec3(2.0f);
        auto addPlane = [&](float px, float py, float pz, float nx, float ny, float nz, float cr, float cg, float cb, bool isActive) {
            if (!isActive) return;
            Vertex v[4] = {}; for(int i=0; i<4; ++i) { v[i].normal[0]=nx; v[i].normal[1]=ny; v[i].normal[2]=nz; v[i].color[0]=cr; v[i].color[1]=cg; v[i].color[2]=cb; }
            if (nx != 0.0f) { v[0].position[0]=px; v[0].position[1]=minP.y; v[0].position[2]=minP.z; v[1].position[0]=px; v[1].position[1]=maxP.y; v[1].position[2]=minP.z; v[2].position[0]=px; v[2].position[1]=maxP.y; v[2].position[2]=maxP.z; v[3].position[0]=px; v[3].position[1]=minP.y; v[3].position[2]=maxP.z; }
            else if (ny != 0.0f) { v[0].position[0]=minP.x; v[0].position[1]=py; v[0].position[2]=minP.z; v[1].position[0]=maxP.x; v[1].position[1]=py; v[1].position[2]=minP.z; v[2].position[0]=maxP.x; v[2].position[1]=py; v[2].position[2]=maxP.z; v[3].position[0]=minP.x; v[3].position[1]=py; v[3].position[2]=maxP.z; }
            else { v[0].position[0]=minP.x; v[0].position[1]=minP.y; v[0].position[2]=pz; v[1].position[0]=maxP.x; v[1].position[1]=minP.y; v[1].position[2]=pz; v[2].position[0]=maxP.x; v[2].position[1]=maxP.y; v[2].position[2]=pz; v[3].position[0]=minP.x; v[3].position[1]=maxP.y; v[3].position[2]=pz; }
            uint32_t ci = static_cast<uint32_t>(capVerts.size()); capVerts.insert(capVerts.end(), {v[0], v[1], v[2], v[3]}); capInds.insert(capInds.end(), {ci, ci+1, ci+2, ci, ci+2, ci+3});
            uint32_t gi = static_cast<uint32_t>(glassVerts.size()); glassVerts.insert(glassVerts.end(), {v[0], v[1], v[2], v[3]}); glassInds.insert(glassInds.end(), {gi, gi+1, gi+2, gi, gi+2, gi+3});
        };
        addPlane(xMin+0.001f, 0, 0,  1,0,0, colX[0], colX[1], colX[2], actXMin); addPlane(xMax-0.001f, 0, 0, -1,0,0, colX[0], colX[1], colX[2], actXMax); addPlane(0, yMin+0.001f, 0,  0,1,0, colY[0], colY[1], colY[2], actYMin); addPlane(0, yMax-0.001f, 0,  0,-1,0, colY[0], colY[1], colY[2], actYMax); addPlane(0, 0, zMin+0.001f,  0,0,1, colZ[0], colZ[1], colZ[2], actZMin); addPlane(0, 0, zMax-0.001f,  0,0,-1, colZ[0], colZ[1], colZ[2], actZMax);
        m_capIndexCount = static_cast<uint32_t>(capInds.size());
        if (m_capIndexCount > 0 && m_capVertexBuffer && m_capIndexBuffer) { wgpuQueueWriteBuffer(m_queue, m_capVertexBuffer, 0, capVerts.data(), capVerts.size()*sizeof(Vertex)); wgpuQueueWriteBuffer(m_queue, m_capIndexBuffer,  0, capInds.data(),  capInds.size()*sizeof(uint32_t)); }
        m_glassIndexCount = static_cast<uint32_t>(glassInds.size());
        if (m_glassIndexCount > 0 && m_glassVertexBuffer && m_glassIndexBuffer) { wgpuQueueWriteBuffer(m_queue, m_glassVertexBuffer, 0, glassVerts.data(), glassVerts.size()*sizeof(Vertex)); wgpuQueueWriteBuffer(m_queue, m_glassIndexBuffer,  0, glassInds.data(),  glassInds.size()*sizeof(uint32_t)); }
    }

} // namespace BimCore