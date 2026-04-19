// =============================================================================
// BimCore/graphics/GraphicsContext.cpp
// =============================================================================
#include "GraphicsContext.h"
#include "PipelineManager.h"
#include "GPUResourceManager.h"
#include "core/Core.h"
#include <stdexcept>
#include <iostream>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>

#if defined(BIM_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#elif defined(BIM_PLATFORM_MACOS)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(BIM_PLATFORM_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace BimCore {

    GraphicsContext::GraphicsContext(GLFWwindow* window, int width, int height)
        : m_width(static_cast<uint32_t>(width)), m_height(static_cast<uint32_t>(height)) {
        BIM_LOG("GPU", "Initialising WebGPU...");
        
        WGPUInstanceDescriptor instDesc = {};
        m_instance = wgpuCreateInstance(&instDesc);
        
        CreateSurface(window);
        RequestAdapterAndDevice();
        ConfigureSurface();

        m_pipelineMgr = std::make_unique<PipelineManager>(m_device, m_surfaceFormat);
        m_resourceMgr = std::make_unique<GPUResourceManager>(m_device, m_queue, m_pipelineMgr.get());

        CreateRenderTargets(); 
        BIM_LOG("GPU", "Graphics context fully initialised.");
    }

    GraphicsContext::~GraphicsContext() {
        ReleaseRenderTargets();
        m_resourceMgr.reset();
        m_pipelineMgr.reset();
        if (m_queue) wgpuQueueRelease(m_queue);
        if (m_device) wgpuDeviceRelease(m_device);
        if (m_adapter) wgpuAdapterRelease(m_adapter);
        if (m_surface) wgpuSurfaceRelease(m_surface);
        if (m_instance) wgpuInstanceRelease(m_instance);
    }

    void GraphicsContext::CreateSurface(GLFWwindow* window) {
        WGPUSurfaceDescriptor surfaceDesc = {};
        
        #if defined(BIM_PLATFORM_WINDOWS)
        WGPUSurfaceSourceWindowsHWND hwndDesc = {}; 
        hwndDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND; 
        hwndDesc.hinstance = GetModuleHandle(nullptr); 
        hwndDesc.hwnd = glfwGetWin32Window(window); 
        surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&hwndDesc);
        #elif defined(BIM_PLATFORM_LINUX)
        const int platform = glfwGetPlatform(); 
        WGPUSurfaceSourceWaylandSurface waylandDesc = {}; 
        WGPUSurfaceSourceXlibWindow x11Desc = {};
        
        if (platform == GLFW_PLATFORM_WAYLAND) { 
            waylandDesc.chain.sType = WGPUSType_SurfaceSourceWaylandSurface; 
            waylandDesc.display = glfwGetWaylandDisplay(); 
            waylandDesc.surface = glfwGetWaylandWindow(window); 
            surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&waylandDesc); 
        } else if (platform == GLFW_PLATFORM_X11) { 
            x11Desc.chain.sType = WGPUSType_SurfaceSourceXlibWindow; 
            x11Desc.display = glfwGetX11Display(); 
            x11Desc.window = glfwGetX11Window(window); 
            surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&x11Desc); 
        }
        #endif
        
        m_surface = wgpuInstanceCreateSurface(m_instance, &surfaceDesc);
    }

    void GraphicsContext::RequestAdapterAndDevice() {
        WGPURequestAdapterOptions adapterOpts = {}; 
        adapterOpts.compatibleSurface = m_surface;
        
        WGPURequestAdapterCallbackInfo adapterCb = {}; 
        adapterCb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView, void* ud, void*) { 
            if (status == WGPURequestAdapterStatus_Success) *static_cast<WGPUAdapter*>(ud) = adapter; 
        }; 
        adapterCb.userdata1 = &m_adapter;
        
        wgpuInstanceRequestAdapter(m_instance, &adapterOpts, adapterCb);
        
        WGPUDeviceDescriptor deviceDesc = {}; 
        WGPURequestDeviceCallbackInfo deviceCb = {}; 
        deviceCb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* ud, void*) { 
            if (status == WGPURequestDeviceStatus_Success) *static_cast<WGPUDevice*>(ud) = device; 
        }; 
        deviceCb.userdata1 = &m_device;
        
        wgpuAdapterRequestDevice(m_adapter, &deviceDesc, deviceCb);
        m_queue = wgpuDeviceGetQueue(m_device);
    }

    void GraphicsContext::ConfigureSurface() {
        WGPUSurfaceCapabilities caps = {}; 
        wgpuSurfaceGetCapabilities(m_surface, m_adapter, &caps);
        
        m_surfaceFormat = (caps.formatCount > 0) ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
        
        WGPUSurfaceConfiguration cfg = {}; 
        cfg.device = m_device; 
        cfg.format = m_surfaceFormat; 
        cfg.alphaMode = (caps.alphaModeCount > 0) ? caps.alphaModes[0] : WGPUCompositeAlphaMode_Opaque; 
        cfg.usage = WGPUTextureUsage_RenderAttachment; 
        cfg.width = m_width; 
        cfg.height = m_height; 
        cfg.presentMode = WGPUPresentMode_Fifo;
        
        wgpuSurfaceConfigure(m_surface, &cfg); 
        wgpuSurfaceCapabilitiesFreeMembers(caps);
    }

    void GraphicsContext::CreateRenderTargets() {
        ReleaseRenderTargets();

        WGPUTextureDescriptor cDesc = {}; 
        cDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding; 
        cDesc.dimension = WGPUTextureDimension_2D; 
        cDesc.size = { m_width, m_height, 1 }; 
        cDesc.format = m_surfaceFormat; 
        cDesc.mipLevelCount = 1; 
        cDesc.sampleCount = 1;
        
        m_offscreenTexture = wgpuDeviceCreateTexture(m_device, &cDesc); 
        m_offscreenView = wgpuTextureCreateView(m_offscreenTexture, nullptr);

        WGPUTextureDescriptor dDesc = {}; 
        dDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding; 
        dDesc.dimension = WGPUTextureDimension_2D; 
        dDesc.size = { m_width, m_height, 1 }; 
        dDesc.format = WGPUTextureFormat_Depth24PlusStencil8; 
        dDesc.mipLevelCount = 1; 
        dDesc.sampleCount = 1;
        
        m_depthTexture = wgpuDeviceCreateTexture(m_device, &dDesc); 
        m_depthView = wgpuTextureCreateView(m_depthTexture, nullptr);

        WGPUTextureViewDescriptor dReadDesc = {}; 
        dReadDesc.format = WGPUTextureFormat_Undefined; 
        dReadDesc.dimension = WGPUTextureViewDimension_2D; 
        dReadDesc.baseMipLevel = 0; 
        dReadDesc.mipLevelCount = 1; 
        dReadDesc.baseArrayLayer = 0; 
        dReadDesc.arrayLayerCount = 1; 
        dReadDesc.aspect = WGPUTextureAspect_DepthOnly;
        
        m_depthReadView = wgpuTextureCreateView(m_depthTexture, &dReadDesc);

        if (!m_shadowTexture) {
            WGPUTextureDescriptor shadowDesc = {}; 
            shadowDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding; 
            shadowDesc.dimension = WGPUTextureDimension_2D; 
            shadowDesc.size = { 4096, 4096, 1 }; 
            shadowDesc.format = WGPUTextureFormat_Depth32Float; 
            shadowDesc.mipLevelCount = 1; 
            shadowDesc.sampleCount = 1;
            
            m_shadowTexture = wgpuDeviceCreateTexture(m_device, &shadowDesc); 
            m_shadowView = wgpuTextureCreateView(m_shadowTexture, nullptr);

            WGPUSamplerDescriptor sSampDesc = {}; 
            sSampDesc.addressModeU = WGPUAddressMode_ClampToEdge; 
            sSampDesc.addressModeV = WGPUAddressMode_ClampToEdge; 
            sSampDesc.addressModeW = WGPUAddressMode_ClampToEdge; 
            sSampDesc.magFilter = WGPUFilterMode_Linear; 
            sSampDesc.minFilter = WGPUFilterMode_Linear; 
            sSampDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest; 
            sSampDesc.maxAnisotropy = 1; 
            sSampDesc.compare = WGPUCompareFunction_Less;
            
            m_shadowSampler = wgpuDeviceCreateSampler(m_device, &sSampDesc);
        }

        if (m_pipelineMgr->GetSSAOLayout() && m_resourceMgr->GetUniformBuffer()) {
            WGPUBindGroupEntry bgEntries[3] = {};
            bgEntries[0].binding = 0; 
            bgEntries[0].buffer = m_resourceMgr->GetUniformBuffer(); 
            bgEntries[0].size = sizeof(SceneUniforms);
            
            bgEntries[1].binding = 1; 
            bgEntries[1].textureView = m_offscreenView;
            
            bgEntries[2].binding = 2; 
            bgEntries[2].textureView = m_depthReadView;

            WGPUBindGroupDescriptor bgDesc = {};
            bgDesc.layout = m_pipelineMgr->GetSSAOLayout();
            bgDesc.entryCount = 3;
            bgDesc.entries = bgEntries;
            m_ssaoBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);
        }
    }

    void GraphicsContext::ReleaseRenderTargets() {
        if (m_ssaoBindGroup) { wgpuBindGroupRelease(m_ssaoBindGroup); m_ssaoBindGroup = nullptr; }
        if (m_depthReadView) { wgpuTextureViewRelease(m_depthReadView); m_depthReadView = nullptr; }
        if (m_depthView) { wgpuTextureViewRelease(m_depthView); m_depthView = nullptr; }
        if (m_depthTexture) { wgpuTextureRelease(m_depthTexture); m_depthTexture = nullptr; }
        if (m_offscreenView) { wgpuTextureViewRelease(m_offscreenView); m_offscreenView = nullptr; }
        if (m_offscreenTexture) { wgpuTextureRelease(m_offscreenTexture); m_offscreenTexture = nullptr; }
    }

    void GraphicsContext::UploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) { m_resourceMgr->UploadMesh(vertices, indices); }
    void GraphicsContext::UpdateGeometry(const std::vector<Vertex>& vertices) { m_resourceMgr->UpdateGeometry(vertices); }
    void GraphicsContext::UploadTextures(const std::vector<TextureData>& textures) { m_resourceMgr->UploadTextures(textures); }
    void GraphicsContext::UpdateInstanceData(const std::vector<glm::mat4>& transforms) { m_resourceMgr->UpdateInstanceData(transforms); }
    void GraphicsContext::UpdateActiveBatches(const std::map<int, std::vector<uint32_t>>& solidBatches, const std::map<int, std::vector<uint32_t>>& transparentBatches) { m_resourceMgr->UpdateActiveBatches(solidBatches, transparentBatches); }
    void GraphicsContext::SetBoundingBox(bool active, const glm::vec3& minB, const glm::vec3& maxB) { m_showAABB = active; m_resourceMgr->SetBoundingBox(active, minB, maxB); }
    void GraphicsContext::SetClippingPlanes(bool actXMin, float xMin, bool actXMax, float xMax, const float* colX, bool actYMin, float yMin, bool actYMax, float yMax, const float* colY, bool actZMin, float zMin, bool actZMax, float zMax, const float* colZ, const glm::vec3& minB, const glm::vec3& maxB) { m_resourceMgr->SetClippingPlanes(actXMin, xMin, actXMax, xMax, colX, actYMin, yMin, actYMax, yMax, colY, actZMin, zMin, actZMax, zMax, colZ, minB, maxB); }
    void GraphicsContext::SetHighlight(bool active, const std::vector<HighlightRange>& ranges, int style) { m_hasHighlight = active; m_highlightRanges = ranges; m_highlightStyle = style; }

    void GraphicsContext::Resize(int newWidth, int newHeight) {
        if (newWidth <= 0 || newHeight <= 0) return;
        m_width = static_cast<uint32_t>(newWidth); 
        m_height = static_cast<uint32_t>(newHeight);
        
        WGPUSurfaceConfiguration cfg = {}; 
        cfg.device = m_device; 
        cfg.format = m_surfaceFormat; 
        cfg.usage = WGPUTextureUsage_RenderAttachment; 
        cfg.alphaMode = WGPUCompositeAlphaMode_Opaque; 
        cfg.width = m_width; 
        cfg.height = m_height; 
        cfg.presentMode = WGPUPresentMode_Fifo;
        
        wgpuSurfaceConfigure(m_surface, &cfg);
        CreateRenderTargets();
    }

    void GraphicsContext::UpdateScene(const SceneUniforms& uniforms) {
        m_resourceMgr->UpdateScene(uniforms, m_shadowView, m_shadowSampler);
    }

    void GraphicsContext::RenderFrame() {
        WGPUBindGroup sceneGroup = m_resourceMgr->GetSceneBindGroup();
        if (!sceneGroup) return;

        WGPUSurfaceTexture surfTex = {};
        wgpuSurfaceGetCurrentTexture(m_surface, &surfTex);
        if (!surfTex.texture) return;

        WGPUTextureView view = wgpuTextureCreateView(surfTex.texture, nullptr);
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);

        WGPUBuffer vBuf = m_resourceMgr->GetVertexBuffer();
        WGPUBuffer aIdxBuf = m_resourceMgr->GetActiveIndexBuffer();
        uint32_t aIdxCount = m_resourceMgr->GetActiveIndexCount();

        // 1. Shadow Pass
        if (m_shadowTexture && vBuf && aIdxBuf && aIdxCount > 0) {
            WGPURenderPassDepthStencilAttachment shadowDepthAtt = {}; 
            shadowDepthAtt.view = m_shadowView; 
            shadowDepthAtt.depthClearValue = 1.0f; 
            shadowDepthAtt.depthLoadOp = WGPULoadOp_Clear; 
            shadowDepthAtt.depthStoreOp = WGPUStoreOp_Store;
            
            WGPURenderPassDescriptor shadowRpDesc = {}; 
            shadowRpDesc.depthStencilAttachment = &shadowDepthAtt;
            
            WGPURenderPassEncoder shadowRp = wgpuCommandEncoderBeginRenderPass(encoder, &shadowRpDesc);
            
            wgpuRenderPassEncoderSetPipeline(shadowRp, m_pipelineMgr->GetShadowPipeline());
            wgpuRenderPassEncoderSetBindGroup(shadowRp, 0, sceneGroup, 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(shadowRp, 0, vBuf, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(shadowRp, aIdxBuf, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            
            for (const auto& batch : m_resourceMgr->GetSolidBatches()) { 
                wgpuRenderPassEncoderDrawIndexed(shadowRp, batch.indexCount, 1, batch.startIndex, 0, 0); 
            }
            
            wgpuRenderPassEncoderEnd(shadowRp); 
            wgpuRenderPassEncoderRelease(shadowRp);
        }

        // 2. Main Geometry Pass
        WGPURenderPassColorAttachment colorAtt = {}; 
        colorAtt.view = m_offscreenView; 
        colorAtt.loadOp = WGPULoadOp_Clear; 
        colorAtt.storeOp = WGPUStoreOp_Store; 
        colorAtt.clearValue = { 0.11f, 0.11f, 0.13f, 1.0f };
        
        WGPURenderPassDepthStencilAttachment depthAtt = {}; 
        depthAtt.view = m_depthView; 
        depthAtt.depthClearValue = 1.0f; 
        depthAtt.depthLoadOp = WGPULoadOp_Clear; 
        depthAtt.depthStoreOp = WGPUStoreOp_Store; 
        depthAtt.stencilClearValue = 0; 
        depthAtt.stencilLoadOp = WGPULoadOp_Clear; 
        depthAtt.stencilStoreOp = WGPUStoreOp_Store;
        
        WGPURenderPassDescriptor rpDesc = {}; 
        rpDesc.colorAttachmentCount = 1; 
        rpDesc.colorAttachments = &colorAtt; 
        rpDesc.depthStencilAttachment = &depthAtt;

        WGPURenderPassEncoder rp = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
        
        wgpuRenderPassEncoderSetBindGroup(rp, 0, sceneGroup, 0, nullptr);
        if (m_resourceMgr->GetShadowBindGroup()) {
            wgpuRenderPassEncoderSetBindGroup(rp, 1, m_resourceMgr->GetShadowBindGroup(), 0, nullptr);
        }

        // Main Opaque
        if (vBuf && aIdxBuf && aIdxCount > 0) {
            wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetMainPipeline());
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, vBuf, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, aIdxBuf, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            
            for (const auto& batch : m_resourceMgr->GetSolidBatches()) {
                wgpuRenderPassEncoderSetBindGroup(rp, 2, m_resourceMgr->GetTextureBindGroup(batch.textureIndex), 0, nullptr);
                wgpuRenderPassEncoderDrawIndexed(rp, batch.indexCount, 1, batch.startIndex, 0, 0);
            }

            // --- TEMPORARILY DISABLED: Stencil Masking ---
            // This causes aggressive visual bleeding on non-manifold IFC slabs.
            // wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetStencilMaskPipeline());
            // wgpuRenderPassEncoderDrawIndexed(rp, aIdxCount, 1, 0, 0, 0);
        }

        // --- TEMPORARILY DISABLED: Section Capping ---
        // Disabling this removes the giant red blobs and lets you see perfectly into the model.
        // if (m_resourceMgr->GetCapIndexCount() > 0 && m_resourceMgr->GetCapVertexBuffer() && m_resourceMgr->GetCapIndexBuffer()) {
        //     wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetCapPipeline());
        //     wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_resourceMgr->GetCapVertexBuffer(), 0, WGPU_WHOLE_SIZE);
        //     wgpuRenderPassEncoderSetIndexBuffer(rp, m_resourceMgr->GetCapIndexBuffer(), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
        //     wgpuRenderPassEncoderSetStencilReference(rp, 0); 
        //     wgpuRenderPassEncoderDrawIndexed(rp, m_resourceMgr->GetCapIndexCount(), 1, 0, 0, 0);
        // }

        // Grid
        if (m_pipelineMgr->GetGridPipeline()) {
            wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetGridPipeline());
            wgpuRenderPassEncoderDraw(rp, 6, 1, 0, 0);
        }

        // Transparent Materials
        WGPUBuffer tIdxBuf = m_resourceMgr->GetActiveTransparentIndexBuffer();
        if (vBuf && tIdxBuf && m_resourceMgr->GetActiveTransparentIndexCount() > 0) {
            wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetTransparentPipeline());
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, vBuf, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, tIdxBuf, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            
            for (const auto& batch : m_resourceMgr->GetTransparentBatches()) {
                wgpuRenderPassEncoderSetBindGroup(rp, 2, m_resourceMgr->GetTextureBindGroup(batch.textureIndex), 0, nullptr);
                wgpuRenderPassEncoderDrawIndexed(rp, batch.indexCount, 1, batch.startIndex, 0, 0);
            }
        }

        // Glass / Clipping Planes
        if (m_resourceMgr->GetGlassIndexCount() > 0 && m_resourceMgr->GetGlassVertexBuffer() && m_resourceMgr->GetGlassIndexBuffer()) {
            wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetGlassPipeline());
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_resourceMgr->GetGlassVertexBuffer(), 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_resourceMgr->GetGlassIndexBuffer(), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, m_resourceMgr->GetGlassIndexCount(), 1, 0, 0, 0);
        }

        // Highlights
        if (m_hasHighlight && !m_highlightRanges.empty() && vBuf) {
            if (m_highlightStyle == 1 && m_resourceMgr->GetLineIndexBuffer()) {
                wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetHighlightOutlinePipeline());
                wgpuRenderPassEncoderSetVertexBuffer(rp, 0, vBuf, 0, WGPU_WHOLE_SIZE);
                wgpuRenderPassEncoderSetIndexBuffer(rp, m_resourceMgr->GetLineIndexBuffer(), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
                for (const auto& range : m_highlightRanges) {
                    wgpuRenderPassEncoderDrawIndexed(rp, range.indexCount * 2, 1, range.startIndex * 2, 0, 0);
                }
            } else if (m_highlightStyle == 0 && m_resourceMgr->GetIndexBuffer()) {
                wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetHighlightSolidPipeline());
                wgpuRenderPassEncoderSetVertexBuffer(rp, 0, vBuf, 0, WGPU_WHOLE_SIZE);
                wgpuRenderPassEncoderSetIndexBuffer(rp, m_resourceMgr->GetIndexBuffer(), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
                for (const auto& range : m_highlightRanges) {
                    wgpuRenderPassEncoderDrawIndexed(rp, range.indexCount, 1, range.startIndex, 0, 0);
                }
            }
        }

        // Bounding Boxes
        if (m_showAABB && m_resourceMgr->GetAABBVertexBuffer() && m_resourceMgr->GetAABBIndexBuffer()) {
            wgpuRenderPassEncoderSetPipeline(rp, m_pipelineMgr->GetAABBPipeline());
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_resourceMgr->GetAABBVertexBuffer(), 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_resourceMgr->GetAABBIndexBuffer(), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, 24, 1, 0, 0, 0);
        }
        
        wgpuRenderPassEncoderEnd(rp);

        // 3. SSAO / Post-Process Pass
        WGPURenderPassColorAttachment ssaoAtt = {};
        ssaoAtt.view = view;
        ssaoAtt.loadOp = WGPULoadOp_Clear;
        ssaoAtt.storeOp = WGPUStoreOp_Store;
        ssaoAtt.clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };

        WGPURenderPassDescriptor ssaoDesc = {};
        ssaoDesc.colorAttachmentCount = 1;
        ssaoDesc.colorAttachments = &ssaoAtt;

        WGPURenderPassEncoder ssaoRp = wgpuCommandEncoderBeginRenderPass(encoder, &ssaoDesc);

        if (m_ssaoBindGroup && m_pipelineMgr->GetSSAOPipeline()) {
            wgpuRenderPassEncoderSetPipeline(ssaoRp, m_pipelineMgr->GetSSAOPipeline());
            wgpuRenderPassEncoderSetBindGroup(ssaoRp, 0, m_ssaoBindGroup, 0, nullptr);
            wgpuRenderPassEncoderDraw(ssaoRp, 3, 1, 0, 0);
        }

        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), ssaoRp);
        wgpuRenderPassEncoderEnd(ssaoRp);

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(m_queue, 1, &cmd);
        wgpuSurfacePresent(m_surface);

        wgpuCommandBufferRelease(cmd);
        wgpuRenderPassEncoderRelease(rp);
        wgpuRenderPassEncoderRelease(ssaoRp);
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(view);
    }

    void GraphicsContext::InitImGui(GLFWwindow* window) {
        IMGUI_CHECKVERSION(); 
        ImGui::CreateContext(); 
        ImGuiIO& io = ImGui::GetIO();
        
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        
        ImFontConfig defaultCfg;
        defaultCfg.SizePixels = 13.0f;
        io.Fonts->AddFontDefault(&defaultCfg);

        ImFontConfig fontCfg;
        fontCfg.MergeMode = true;
        fontCfg.PixelSnapH = true;
        fontCfg.GlyphMinAdvanceX = 14.0f;

        static const ImWchar iconRanges[] = { 0xe000, 0xf8ff, 0 };
        io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 14.0f, &fontCfg, iconRanges);
        
        ImGui_ImplGlfw_InitForOther(window, true);
        
        ImGui_ImplWGPU_InitInfo wgpuInfo = {}; 
        wgpuInfo.Device = m_device; 
        wgpuInfo.NumFramesInFlight = 3; 
        wgpuInfo.RenderTargetFormat = m_surfaceFormat; 
        wgpuInfo.DepthStencilFormat = WGPUTextureFormat_Undefined;
        
        ImGui_ImplWGPU_Init(&wgpuInfo);
    }

    void GraphicsContext::ShutdownImGui() {
        ImGui_ImplWGPU_Shutdown(); 
        ImGui_ImplGlfw_Shutdown(); 
        ImGui::DestroyContext();
    }

} // namespace BimCore