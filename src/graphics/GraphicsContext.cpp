// =============================================================================
// BimCore/graphics/GraphicsContext.cpp
// =============================================================================
#include "GraphicsContext.h"
#include "ShaderLibrary.h"
#include "core/Core.h"
#include "scene/SceneModel.h"
#include <stdexcept>
#include <cstring>
#include <vector>
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
    : m_width(static_cast<uint32_t>(width))
    , m_height(static_cast<uint32_t>(height))
    {
        BIM_LOG("GPU", "Initialising WebGPU...");
        WGPUInstanceDescriptor instDesc = {};
        m_instance = wgpuCreateInstance(&instDesc);
        if (!m_instance) throw std::runtime_error("[GPU] wgpuCreateInstance failed");

        CreateSurface(window);
        RequestAdapterAndDevice();
        ConfigureSurface();

        CreateUniformBuffers();
        CreateDefaultMaterial();

        CreateSSAOPipeline();
        CreateRenderTargets();

        CreateMainPipeline();
        CreateHighlightPipeline();
        CreateAABBPipeline();
        CreateGlassPipeline();
        CreateStencilPipelines();
        CreateGridPipeline(); // --- NEW: Compile the Grid ---

        AllocateGeometryBuffers();
        BIM_LOG("GPU", "Graphics context fully initialised.");
    }

    GraphicsContext::~GraphicsContext() {
        if (m_capIndexBuffer)                wgpuBufferRelease(m_capIndexBuffer);
        if (m_capVertexBuffer)               wgpuBufferRelease(m_capVertexBuffer);
        if (m_glassIndexBuffer)              wgpuBufferRelease(m_glassIndexBuffer);
        if (m_glassVertexBuffer)             wgpuBufferRelease(m_glassVertexBuffer);
        if (m_aabbIndexBuffer)               wgpuBufferRelease(m_aabbIndexBuffer);
        if (m_aabbVertexBuffer)              wgpuBufferRelease(m_aabbVertexBuffer);
        if (m_lineIndexBuffer)               wgpuBufferRelease(m_lineIndexBuffer);
        if (m_activeTransparentIndexBuffer)  wgpuBufferRelease(m_activeTransparentIndexBuffer);
        if (m_activeIndexBuffer)             wgpuBufferRelease(m_activeIndexBuffer);
        if (m_indexBuffer)                   wgpuBufferRelease(m_indexBuffer);
        if (m_vertexBuffer)                  wgpuBufferRelease(m_vertexBuffer);

        if (m_gridPipeline)                  wgpuRenderPipelineRelease(m_gridPipeline); // --- NEW ---
        if (m_capPipeline)                   wgpuRenderPipelineRelease(m_capPipeline);
        if (m_stencilMaskPipeline)           wgpuRenderPipelineRelease(m_stencilMaskPipeline);
        if (m_glassPipeline)                 wgpuRenderPipelineRelease(m_glassPipeline);
        if (m_aabbPipeline)                  wgpuRenderPipelineRelease(m_aabbPipeline);
        if (m_highlightOutlinePipeline)      wgpuRenderPipelineRelease(m_highlightOutlinePipeline);
        if (m_highlightSolidPipeline)        wgpuRenderPipelineRelease(m_highlightSolidPipeline);
        if (m_transparentPipeline)           wgpuRenderPipelineRelease(m_transparentPipeline);
        if (m_pipeline)                      wgpuRenderPipelineRelease(m_pipeline);
        if (m_ssaoPipeline)                  wgpuRenderPipelineRelease(m_ssaoPipeline);

        for (auto& gt : m_gpuTextures) {
            if (gt.bindGroup) wgpuBindGroupRelease(gt.bindGroup);
            if (gt.view) wgpuTextureViewRelease(gt.view);
            if (gt.texture) wgpuTextureRelease(gt.texture);
        }

        if (m_defaultMaterialBindGroup)      wgpuBindGroupRelease(m_defaultMaterialBindGroup);
        if (m_materialBindGroupLayout)       wgpuBindGroupLayoutRelease(m_materialBindGroupLayout);
        if (m_defaultTextureView)            wgpuTextureViewRelease(m_defaultTextureView);
        if (m_defaultTexture)                wgpuTextureRelease(m_defaultTexture);
        if (m_defaultSampler)                wgpuSamplerRelease(m_defaultSampler);

        ReleaseRenderTargets();

        if (m_sceneBindGroupLayout)          wgpuBindGroupLayoutRelease(m_sceneBindGroupLayout);
        if (m_ssaoBindGroupLayout)           wgpuBindGroupLayoutRelease(m_ssaoBindGroupLayout);
        if (m_instanceBuffer)                wgpuBufferRelease(m_instanceBuffer);
        if (m_uniformBuffer)                 wgpuBufferRelease(m_uniformBuffer);

        if (m_queue)    wgpuQueueRelease(m_queue);
        if (m_device)   wgpuDeviceRelease(m_device);
        if (m_adapter)  wgpuAdapterRelease(m_adapter);
        if (m_surface)  wgpuSurfaceRelease(m_surface);
        if (m_instance) wgpuInstanceRelease(m_instance);
    }

    void GraphicsContext::CreateSurface(GLFWwindow* window) {
        WGPUSurfaceDescriptor surfaceDesc = {};

        #if defined(BIM_PLATFORM_WINDOWS)
        WGPUSurfaceSourceWindowsHWND hwndDesc = {};
        hwndDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        hwndDesc.hinstance   = GetModuleHandle(nullptr);
        hwndDesc.hwnd        = glfwGetWin32Window(window);
        surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&hwndDesc);
        #elif defined(BIM_PLATFORM_MACOS)
        WGPUSurfaceSourceCocoaWindow cocoaDesc = {};
        cocoaDesc.chain.sType = WGPUSType_SurfaceSourceCocoaWindow;
        cocoaDesc.window      = glfwGetCocoaWindow(window);
        surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&cocoaDesc);
        #elif defined(BIM_PLATFORM_LINUX)
        const int platform = glfwGetPlatform();
        WGPUSurfaceSourceWaylandSurface waylandDesc = {};
        WGPUSurfaceSourceXlibWindow     x11Desc     = {};

        if (platform == GLFW_PLATFORM_WAYLAND) {
            waylandDesc.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
            waylandDesc.display     = glfwGetWaylandDisplay();
            waylandDesc.surface     = glfwGetWaylandWindow(window);
            surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&waylandDesc);
        } else if (platform == GLFW_PLATFORM_X11) {
            x11Desc.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
            x11Desc.display     = glfwGetX11Display();
            x11Desc.window      = glfwGetX11Window(window);
            surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&x11Desc);
        } else {
            throw std::runtime_error("[GPU] Unsupported Linux windowing system");
        }
        #else
        throw std::runtime_error("[GPU] Unsupported platform");
        #endif

        m_surface = wgpuInstanceCreateSurface(m_instance, &surfaceDesc);
        if (!m_surface) throw std::runtime_error("[GPU] wgpuInstanceCreateSurface failed");
    }

    void GraphicsContext::RequestAdapterAndDevice() {
        WGPURequestAdapterOptions adapterOpts = {};
        adapterOpts.compatibleSurface = m_surface;

        WGPURequestAdapterCallbackInfo adapterCb = {};
        adapterCb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView, void* ud, void*) {
            if (status == WGPURequestAdapterStatus_Success)
                *static_cast<WGPUAdapter*>(ud) = adapter;
        };
        adapterCb.userdata1 = &m_adapter;

        wgpuInstanceRequestAdapter(m_instance, &adapterOpts, adapterCb);
        if (!m_adapter) throw std::runtime_error("[GPU] Failed to acquire WebGPU adapter");

        WGPUDeviceDescriptor deviceDesc = {};
        WGPURequestDeviceCallbackInfo deviceCb = {};
        deviceCb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* ud, void*) {
            if (status == WGPURequestDeviceStatus_Success)
                *static_cast<WGPUDevice*>(ud) = device;
        };
        deviceCb.userdata1 = &m_device;

        wgpuAdapterRequestDevice(m_adapter, &deviceDesc, deviceCb);
        if (!m_device) throw std::runtime_error("[GPU] Failed to acquire WebGPU device");

        m_queue = wgpuDeviceGetQueue(m_device);
    }

    void GraphicsContext::ConfigureSurface() {
        WGPUSurfaceCapabilities caps = {};
        wgpuSurfaceGetCapabilities(m_surface, m_adapter, &caps);

        m_surfaceFormat = (caps.formatCount > 0) ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;

        WGPUSurfaceConfiguration cfg = {};
        cfg.device      = m_device;
        cfg.format      = m_surfaceFormat;
        cfg.alphaMode   = (caps.alphaModeCount > 0) ? caps.alphaModes[0] : WGPUCompositeAlphaMode_Opaque;
        cfg.usage       = WGPUTextureUsage_RenderAttachment;
        cfg.width       = m_width;
        cfg.height      = m_height;
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

        if (m_ssaoBindGroupLayout) {
            WGPUBindGroupEntry bgEntries[3] = {};
            bgEntries[0].binding = 0; bgEntries[0].buffer = m_uniformBuffer; bgEntries[0].size = sizeof(SceneUniforms);
            bgEntries[1].binding = 1; bgEntries[1].textureView = m_offscreenView;
            bgEntries[2].binding = 2; bgEntries[2].textureView = m_depthReadView;

            WGPUBindGroupDescriptor bgDesc = {};
            bgDesc.layout = m_ssaoBindGroupLayout;
            bgDesc.entryCount = 3;
            bgDesc.entries = bgEntries;
            m_ssaoBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);
        }
    }

    void GraphicsContext::ReleaseRenderTargets() {
        if (m_sceneBindGroup) { wgpuBindGroupRelease(m_sceneBindGroup); m_sceneBindGroup = nullptr; }
        if (m_ssaoBindGroup) { wgpuBindGroupRelease(m_ssaoBindGroup); m_ssaoBindGroup = nullptr; }
        if (m_depthReadView) { wgpuTextureViewRelease(m_depthReadView); m_depthReadView = nullptr; }
        if (m_depthView) { wgpuTextureViewRelease(m_depthView); m_depthView = nullptr; }
        if (m_depthTexture) { wgpuTextureRelease(m_depthTexture); m_depthTexture = nullptr; }
        if (m_offscreenView) { wgpuTextureViewRelease(m_offscreenView); m_offscreenView = nullptr; }
        if (m_offscreenTexture) { wgpuTextureRelease(m_offscreenTexture); m_offscreenTexture = nullptr; }
    }

    void GraphicsContext::CreateUniformBuffers() {
        WGPUBufferDescriptor ubDesc = {};
        ubDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
        ubDesc.size  = sizeof(SceneUniforms);
        m_uniformBuffer = wgpuDeviceCreateBuffer(m_device, &ubDesc);

        WGPUBufferDescriptor instDesc = {};
        instDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage;
        instDesc.size  = 1'000'000 * sizeof(glm::mat4);
        m_instanceBuffer = wgpuDeviceCreateBuffer(m_device, &instDesc);

        WGPUBindGroupLayoutEntry bglEntries[2] = {};
        bglEntries[0].binding               = 0;
        bglEntries[0].visibility            = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        bglEntries[0].buffer.type           = WGPUBufferBindingType_Uniform;
        bglEntries[0].buffer.minBindingSize = sizeof(SceneUniforms);

        bglEntries[1].binding               = 1;
        bglEntries[1].visibility            = WGPUShaderStage_Vertex;
        bglEntries[1].buffer.type           = WGPUBufferBindingType_ReadOnlyStorage;
        bglEntries[1].buffer.minBindingSize = sizeof(glm::mat4);

        WGPUBindGroupLayoutDescriptor bglDesc = {};
        bglDesc.entryCount = 2;
        bglDesc.entries    = bglEntries;
        m_sceneBindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bglDesc);
    }

    void GraphicsContext::CreateDefaultMaterial() {
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

        WGPUTexelCopyTextureInfo dest = {};
        dest.texture = m_defaultTexture;
        dest.mipLevel = 0;
        dest.origin = {0, 0, 0};

        WGPUTexelCopyBufferLayout dataLayout = {};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = 4;
        dataLayout.rowsPerImage = 1;

        uint8_t whitePixel[4] = {255, 255, 255, 255};
        WGPUExtent3D size = {1, 1, 1};
        wgpuQueueWriteTexture(m_queue, &dest, whitePixel, 4, &dataLayout, &size);

        WGPUTextureViewDescriptor viewDesc = {};
        viewDesc.format = WGPUTextureFormat_RGBA8Unorm; viewDesc.dimension = WGPUTextureViewDimension_2D;
        viewDesc.baseMipLevel = 0; viewDesc.mipLevelCount = 1; viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1; viewDesc.aspect = WGPUTextureAspect_All;
        m_defaultTextureView = wgpuTextureCreateView(m_defaultTexture, &viewDesc);

        WGPUBindGroupLayoutEntry bgle[2] = {};
        bgle[0].binding = 0; bgle[0].visibility = WGPUShaderStage_Fragment;
        bgle[0].texture.sampleType = WGPUTextureSampleType_Float; bgle[0].texture.viewDimension = WGPUTextureViewDimension_2D;
        bgle[1].binding = 1; bgle[1].visibility = WGPUShaderStage_Fragment;
        bgle[1].sampler.type = WGPUSamplerBindingType_Filtering;

        WGPUBindGroupLayoutDescriptor bgld = {}; bgld.entryCount = 2; bgld.entries = bgle;
        m_materialBindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bgld);

        WGPUBindGroupEntry bgEntries[2] = {};
        bgEntries[0].binding = 0; bgEntries[0].textureView = m_defaultTextureView;
        bgEntries[1].binding = 1; bgEntries[1].sampler = m_defaultSampler;

        WGPUBindGroupDescriptor bgDesc = {}; bgDesc.layout = m_materialBindGroupLayout;
        bgDesc.entryCount = 2; bgDesc.entries = bgEntries;
        m_defaultMaterialBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);
    }

    WGPUShaderModule GraphicsContext::CreateShaderModule(const std::string& source) const {
        std::string fullCode = Shaders::kUniformsWGSL + source;
        WGPUShaderSourceWGSL src = {};
        src.chain.sType = WGPUSType_ShaderSourceWGSL;
        src.code        = WGPUStringView{ fullCode.c_str(), fullCode.length() };
        WGPUShaderModuleDescriptor desc = {};
        desc.nextInChain = &src.chain;
        return wgpuDeviceCreateShaderModule(m_device, &desc);
    }

    static WGPUVertexBufferLayout MakeVertexBufferLayout(std::vector<WGPUVertexAttribute>& attrs) {
        attrs.resize(4);
        attrs[0] = { WGPUVertexFormat_Float32x3, offsetof(BimCore::Vertex, position), 0 };
        attrs[1] = { WGPUVertexFormat_Float32x3, offsetof(BimCore::Vertex, normal),   1 };
        attrs[2] = { WGPUVertexFormat_Float32x3, offsetof(BimCore::Vertex, color),    2 };
        attrs[3] = { WGPUVertexFormat_Float32x2, offsetof(BimCore::Vertex, uv),       3 };
        WGPUVertexBufferLayout layout = {};
        layout.arrayStride    = sizeof(BimCore::Vertex);
        layout.stepMode       = WGPUVertexStepMode_Vertex;
        layout.attributeCount = 4;
        layout.attributes     = attrs.data();
        return layout;
    }

    // --- NEW: Dynamic Grid setup (uses a full-screen procedural quad) ---
    void GraphicsContext::CreateGridPipeline() {
        WGPUShaderModule shader = CreateShaderModule(Shaders::kGridWGSL);

        WGPUPipelineLayoutDescriptor pld = {};
        pld.bindGroupLayoutCount = 1;
        pld.bindGroupLayouts = &m_sceneBindGroupLayout;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        WGPUBlendState blend = {};
        blend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha };
        blend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One,      WGPUBlendFactor_OneMinusSrcAlpha };

        WGPUColorTargetState target = {};
        target.format = m_surfaceFormat;
        target.blend = &blend;
        target.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState frag = {};
        frag.module = shader;
        frag.entryPoint = WGPUStringView{"fs_main", 7};
        frag.targetCount = 1;
        frag.targets = &target;

        WGPUDepthStencilState ds = {};
        ds.format = WGPUTextureFormat_Depth24PlusStencil8;
        ds.depthCompare = WGPUCompareFunction_Less;
        ds.depthWriteEnabled = WGPUOptionalBool_False;

        WGPURenderPipelineDescriptor pd = {};
        pd.layout = layout;
        pd.vertex.module = shader;
        pd.vertex.entryPoint = WGPUStringView{"vs_main", 7};
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pd.depthStencil = &ds;
        pd.fragment = &frag;
        pd.multisample.count = 1;
        pd.multisample.mask = ~0u;

        m_gridPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        wgpuShaderModuleRelease(shader);
        wgpuPipelineLayoutRelease(layout);
    }

    void GraphicsContext::CreateSSAOPipeline() {
        WGPUBindGroupLayoutEntry bgle[3] = {};
        bgle[0].binding = 0; bgle[0].visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
        bgle[0].buffer.type = WGPUBufferBindingType_Uniform; bgle[0].buffer.minBindingSize = sizeof(SceneUniforms);

        bgle[1].binding = 1; bgle[1].visibility = WGPUShaderStage_Fragment;
        bgle[1].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        bgle[1].texture.viewDimension = WGPUTextureViewDimension_2D;

        bgle[2].binding = 2; bgle[2].visibility = WGPUShaderStage_Fragment;
        bgle[2].texture.sampleType = WGPUTextureSampleType_Depth;
        bgle[2].texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutDescriptor bgld = {}; bgld.entryCount = 3; bgld.entries = bgle;
        m_ssaoBindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bgld);

        WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_ssaoBindGroupLayout;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        WGPUShaderModule shader = CreateShaderModule(Shaders::kSSAOWGSL);

        WGPUColorTargetState target = {};
        target.format = m_surfaceFormat;
        target.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState frag = {};
        frag.module = shader;
        frag.entryPoint = WGPUStringView{"fs_main", 7};
        frag.targetCount = 1;
        frag.targets = &target;

        WGPURenderPipelineDescriptor pd = {};
        pd.layout = layout;
        pd.vertex.module = shader;
        pd.vertex.entryPoint = WGPUStringView{"vs_main", 7};
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pd.fragment = &frag;
        pd.multisample.count = 1;
        pd.multisample.mask = ~0u;

        m_ssaoPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        wgpuShaderModuleRelease(shader);
        wgpuPipelineLayoutRelease(layout);
    }

    void GraphicsContext::CreateMainPipeline() {
        WGPUShaderModule shader = CreateShaderModule(Shaders::kMainWGSL);

        WGPUBindGroupLayout layouts[] = { m_sceneBindGroupLayout, m_materialBindGroupLayout };
        WGPUPipelineLayoutDescriptor pld = {};
        pld.bindGroupLayoutCount = 2;
        pld.bindGroupLayouts = layouts;
        WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        WGPUDepthStencilState ds = {};
        ds.format             = WGPUTextureFormat_Depth24PlusStencil8;
        ds.depthWriteEnabled  = WGPUOptionalBool_True;
        ds.depthCompare       = WGPUCompareFunction_Less;

        WGPUBlendState opaqueBlend = {};
        opaqueBlend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_Zero };
        opaqueBlend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_Zero };

        WGPUColorTargetState opaqueTarget = {};
        opaqueTarget.format    = m_surfaceFormat;
        opaqueTarget.blend     = &opaqueBlend;
        opaqueTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState frag = {};
        frag.module      = shader;
        frag.entryPoint  = WGPUStringView{ "fs_opaque", strlen("fs_opaque") };
        frag.targetCount = 1;
        frag.targets     = &opaqueTarget;

        WGPURenderPipelineDescriptor pd = {};
        pd.layout                    = pipelineLayout;
        pd.vertex.module             = shader;
        pd.vertex.entryPoint         = WGPUStringView{ "vs_main", strlen("vs_main") };
        pd.vertex.bufferCount        = 1;
        pd.vertex.buffers            = &vbl;
        pd.primitive.topology        = WGPUPrimitiveTopology_TriangleList;
        pd.primitive.cullMode        = WGPUCullMode_Back;
        pd.primitive.frontFace       = WGPUFrontFace_CCW;
        pd.depthStencil              = &ds;
        pd.fragment                  = &frag;
        pd.multisample.count         = 1;
        pd.multisample.mask          = ~0u;
        m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        WGPUBlendState alphaBlend = {};
        alphaBlend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha };
        alphaBlend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One,      WGPUBlendFactor_OneMinusSrcAlpha };

        WGPUColorTargetState transTarget = opaqueTarget;
        transTarget.blend = &alphaBlend;

        WGPUFragmentState transFrag = frag;
        transFrag.entryPoint = WGPUStringView{ "fs_transparent", strlen("fs_transparent") };
        transFrag.targets    = &transTarget;

        WGPUDepthStencilState transDs = ds;
        transDs.depthWriteEnabled = WGPUOptionalBool_False;

        WGPURenderPipelineDescriptor tpd = pd;
        tpd.fragment    = &transFrag;
        tpd.depthStencil = &transDs;
        m_transparentPipeline = wgpuDeviceCreateRenderPipeline(m_device, &tpd);

        wgpuShaderModuleRelease(shader);
        wgpuPipelineLayoutRelease(pipelineLayout);
    }

    void GraphicsContext::CreateHighlightPipeline() {
        WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneBindGroupLayout;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        WGPUBlendState blend = {};
        blend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha };
        blend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One,      WGPUBlendFactor_OneMinusSrcAlpha };

        WGPUColorTargetState target = {};
        target.format = m_surfaceFormat;
        target.blend = &blend;
        target.writeMask = WGPUColorWriteMask_All;

        WGPUDepthStencilState ds = {};
        ds.format = WGPUTextureFormat_Depth24PlusStencil8;
        ds.depthCompare = WGPUCompareFunction_Always;
        ds.depthWriteEnabled = WGPUOptionalBool_False;

        WGPURenderPipelineDescriptor pd = {};
        pd.layout = layout;
        pd.vertex.entryPoint = WGPUStringView{ "vs_main", 7 };
        pd.vertex.bufferCount = 1;
        pd.vertex.buffers = &vbl;
        pd.primitive.frontFace = WGPUFrontFace_CCW;
        pd.depthStencil = &ds;
        pd.multisample.count = 1;
        pd.multisample.mask = ~0u;

        WGPUShaderModule solidShader = CreateShaderModule(Shaders::kHighlightSolidWGSL);
        WGPUFragmentState fragSolid = {};
        fragSolid.module = solidShader;
        fragSolid.entryPoint = WGPUStringView{ "fs_main", 7 };
        fragSolid.targetCount = 1;
        fragSolid.targets = &target;

        pd.vertex.module = solidShader;
        pd.fragment = &fragSolid;
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pd.primitive.cullMode = WGPUCullMode_Back;
        m_highlightSolidPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        WGPUShaderModule outlineShader = CreateShaderModule(Shaders::kHighlightOutlineWGSL);
        WGPUFragmentState fragOutline = {};
        fragOutline.module = outlineShader;
        fragOutline.entryPoint = WGPUStringView{ "fs_main", 7 };
        fragOutline.targetCount = 1;
        fragOutline.targets = &target;

        pd.vertex.module = outlineShader;
        pd.fragment = &fragOutline;
        pd.primitive.topology = WGPUPrimitiveTopology_LineList;
        pd.primitive.cullMode = WGPUCullMode_None;
        m_highlightOutlinePipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        wgpuShaderModuleRelease(solidShader);
        wgpuShaderModuleRelease(outlineShader);
        wgpuPipelineLayoutRelease(layout);
    }

    void GraphicsContext::CreateAABBPipeline() {
        WGPUShaderModule shader = CreateShaderModule(Shaders::kAABBWGSL);

        WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneBindGroupLayout;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{ "fs_main", 7 }; frag.targetCount = 1; frag.targets = &target;
        WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth24PlusStencil8;
        ds.depthCompare = WGPUCompareFunction_LessEqual; ds.depthWriteEnabled = WGPUOptionalBool_False;

        WGPURenderPipelineDescriptor pd = {};
        pd.layout = layout; pd.vertex.module = shader;
        pd.vertex.entryPoint = WGPUStringView{ "vs_main", 7 };
        pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl;
        pd.primitive.topology = WGPUPrimitiveTopology_LineList;
        pd.depthStencil = &ds; pd.fragment = &frag;
        pd.multisample.count = 1; pd.multisample.mask = ~0u;
        m_aabbPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        wgpuShaderModuleRelease(shader);
        wgpuPipelineLayoutRelease(layout);
    }

    void GraphicsContext::CreateGlassPipeline() {
        WGPUShaderModule shader = CreateShaderModule(Shaders::kGlassWGSL);

        WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneBindGroupLayout;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        WGPUBlendState blend = {};
        blend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha };
        blend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One,      WGPUBlendFactor_OneMinusSrcAlpha };
        WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.blend = &blend; target.writeMask = WGPUColorWriteMask_All;

        WGPUDepthStencilState ds = {};
        ds.format = WGPUTextureFormat_Depth24PlusStencil8;
        ds.depthCompare = WGPUCompareFunction_Less;
        ds.depthWriteEnabled = WGPUOptionalBool_False;

        WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{ "fs_main", 7 }; frag.targetCount = 1; frag.targets = &target;

        WGPURenderPipelineDescriptor pd = {};
        pd.layout = layout; pd.vertex.module = shader;
        pd.vertex.entryPoint = WGPUStringView{ "vs_main", 7 };
        pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl;
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pd.primitive.cullMode = WGPUCullMode_None;
        pd.depthStencil = &ds; pd.fragment = &frag;
        pd.multisample.count = 1; pd.multisample.mask = ~0u;
        m_glassPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        wgpuShaderModuleRelease(shader);
        wgpuPipelineLayoutRelease(layout);
    }

void GraphicsContext::CreateStencilPipelines() {
        WGPUShaderModule maskShader = CreateShaderModule(Shaders::kMaskWGSL);
        WGPUShaderModule capShader  = CreateShaderModule(Shaders::kCapWGSL);

        WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneBindGroupLayout;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        // --- FIXED: The Mathematical Stencil Counting Algorithm ---
        WGPUDepthStencilState maskDs = {};
        maskDs.format = WGPUTextureFormat_Depth24PlusStencil8;
        maskDs.depthWriteEnabled = WGPUOptionalBool_False;
        maskDs.depthCompare = WGPUCompareFunction_Always; // Ignore depth, count everything!

        WGPUStencilFaceState frontFace = {};
        frontFace.compare = WGPUCompareFunction_Always;
        frontFace.failOp = WGPUStencilOperation_Keep;
        frontFace.depthFailOp = WGPUStencilOperation_Keep;
        frontFace.passOp = WGPUStencilOperation_IncrementWrap; // Front faces +1

        WGPUStencilFaceState backFace = {};
        backFace.compare = WGPUCompareFunction_Always;
        backFace.failOp = WGPUStencilOperation_Keep;
        backFace.depthFailOp = WGPUStencilOperation_Keep;
        backFace.passOp = WGPUStencilOperation_DecrementWrap; // Back faces -1

        maskDs.stencilFront = frontFace;
        maskDs.stencilBack = backFace;
        maskDs.stencilReadMask = 0xFF;
        maskDs.stencilWriteMask = 0xFF;

        WGPUColorTargetState dummyTarget = {};
        dummyTarget.format = m_surfaceFormat;
        dummyTarget.writeMask = WGPUColorWriteMask_None;

        WGPUFragmentState maskFrag = {};
        maskFrag.module = maskShader;
        maskFrag.entryPoint = WGPUStringView{"fs_main", 7};
        maskFrag.targetCount = 1;
        maskFrag.targets = &dummyTarget;

        WGPURenderPipelineDescriptor pdMask = {};
        pdMask.layout = layout;
        pdMask.vertex.module = maskShader;
        pdMask.vertex.entryPoint = WGPUStringView{"vs_main", 7};
        pdMask.vertex.bufferCount = 1;
        pdMask.vertex.buffers = &vbl;
        pdMask.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pdMask.primitive.cullMode = WGPUCullMode_None; // CRITICAL: Render BOTH faces to count them
        pdMask.primitive.frontFace = WGPUFrontFace_CCW;
        pdMask.depthStencil = &maskDs;
        pdMask.fragment = &maskFrag;
        pdMask.multisample.count = 1;
        pdMask.multisample.mask = ~0u;

        m_stencilMaskPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pdMask);

        WGPUDepthStencilState capDs = {};
        capDs.format = WGPUTextureFormat_Depth24PlusStencil8;
        capDs.depthWriteEnabled = WGPUOptionalBool_True; // Write depth so overlapping caps occlude correctly
        capDs.depthCompare = WGPUCompareFunction_LessEqual;

        WGPUStencilFaceState capFace = {};
        capFace.compare = WGPUCompareFunction_NotEqual; // Only draw where Stencil != 0
        capFace.failOp = WGPUStencilOperation_Keep;
        capFace.depthFailOp = WGPUStencilOperation_Keep;
        capFace.passOp = WGPUStencilOperation_Zero; // Clear the stencil as we draw

        capDs.stencilFront = capFace;
        capDs.stencilBack = capFace;
        capDs.stencilReadMask = 0xFF;
        capDs.stencilWriteMask = 0xFF;

        WGPUColorTargetState capTarget = {};
        capTarget.format = m_surfaceFormat;
        capTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState capFrag = {};
        capFrag.module = capShader;
        capFrag.entryPoint = WGPUStringView{"fs_main", 7};
        capFrag.targetCount = 1;
        capFrag.targets = &capTarget;

        WGPURenderPipelineDescriptor pdCap = pdMask;
        pdCap.vertex.module = capShader;
        pdCap.fragment = &capFrag;
        pdCap.depthStencil = &capDs;
        pdCap.primitive.cullMode = WGPUCullMode_None;

        m_capPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pdCap);

        wgpuShaderModuleRelease(maskShader);
        wgpuShaderModuleRelease(capShader);
        wgpuPipelineLayoutRelease(layout);
    }

    void GraphicsContext::AllocateGeometryBuffers() {
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

    void GraphicsContext::UploadTextures(const std::vector<TextureData>& textures) {
        for (auto& gt : m_gpuTextures) {
            if (gt.bindGroup) wgpuBindGroupRelease(gt.bindGroup);
            if (gt.view) wgpuTextureViewRelease(gt.view);
            if (gt.texture) wgpuTextureRelease(gt.texture);
        }
        m_gpuTextures.clear();

        for (const auto& texData : textures) {
            WGPUTextureDescriptor desc = {};
            desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
            desc.dimension = WGPUTextureDimension_2D;
            desc.size = { texData.width, texData.height, 1 };
            desc.format = WGPUTextureFormat_RGBA8Unorm;
            desc.mipLevelCount = 1;
            desc.sampleCount = 1;

            GPUTexture gt;
            gt.texture = wgpuDeviceCreateTexture(m_device, &desc);

            WGPUExtent3D extent = { texData.width, texData.height, 1 };
            WGPUTexelCopyTextureInfo dest = {};
            dest.texture = gt.texture;

            WGPUTexelCopyBufferLayout layout = {};
            layout.bytesPerRow = texData.width * 4;
            layout.rowsPerImage = texData.height;

            wgpuQueueWriteTexture(m_queue, &dest, texData.pixels.data(), texData.pixels.size(), &layout, &extent);

            WGPUTextureViewDescriptor viewDesc = {};
            viewDesc.format = WGPUTextureFormat_RGBA8Unorm;
            viewDesc.dimension = WGPUTextureViewDimension_2D;
            viewDesc.baseMipLevel = 0; viewDesc.mipLevelCount = 1;
            viewDesc.baseArrayLayer = 0; viewDesc.arrayLayerCount = 1;
            viewDesc.aspect = WGPUTextureAspect_All;
            gt.view = wgpuTextureCreateView(gt.texture, &viewDesc);

            WGPUBindGroupEntry entries[2] = {};
            entries[0].binding = 0; entries[0].textureView = gt.view;
            entries[1].binding = 1; entries[1].sampler = m_defaultSampler;

            WGPUBindGroupDescriptor bgDesc = {};
            bgDesc.layout = m_materialBindGroupLayout;
            bgDesc.entryCount = 2;
            bgDesc.entries = entries;
            gt.bindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);

            m_gpuTextures.push_back(gt);
        }
    }

    void GraphicsContext::UpdateScene(const SceneUniforms& uniforms) {
        if (m_uniformBuffer) {
            wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0, &uniforms, sizeof(SceneUniforms));
        }

        if (!m_sceneBindGroup && m_sceneBindGroupLayout) {
            WGPUBindGroupEntry bgEntries[2] = {};
            bgEntries[0].binding = 0; bgEntries[0].buffer = m_uniformBuffer; bgEntries[0].size = sizeof(SceneUniforms);
            bgEntries[1].binding = 1; bgEntries[1].buffer = m_instanceBuffer; bgEntries[1].size = 1'000'000 * sizeof(glm::mat4);

            WGPUBindGroupDescriptor bgDesc = {};
            bgDesc.layout = m_sceneBindGroupLayout;
            bgDesc.entryCount = 2;
            bgDesc.entries = bgEntries;
            m_sceneBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);
        }
    }

    void GraphicsContext::UpdateInstanceData(const std::vector<glm::mat4>& transforms) {
        if (!m_instanceBuffer || transforms.empty()) return;
        m_instanceCount = static_cast<uint32_t>(transforms.size());
        wgpuQueueWriteBuffer(m_queue, m_instanceBuffer, 0,
                             transforms.data(), transforms.size() * sizeof(glm::mat4));
    }

    void GraphicsContext::UploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
        if (vertices.empty() || indices.empty()) return;
        m_indexCount = static_cast<uint32_t>(indices.size());

        if (m_vertexBuffer) { wgpuBufferRelease(m_vertexBuffer); m_vertexBuffer = nullptr; }
        if (m_indexBuffer)  { wgpuBufferRelease(m_indexBuffer);  m_indexBuffer  = nullptr; }
        if (m_activeIndexBuffer) { wgpuBufferRelease(m_activeIndexBuffer); m_activeIndexBuffer = nullptr; }
        if (m_activeTransparentIndexBuffer) { wgpuBufferRelease(m_activeTransparentIndexBuffer); m_activeTransparentIndexBuffer = nullptr; }
        if (m_lineIndexBuffer) { wgpuBufferRelease(m_lineIndexBuffer); m_lineIndexBuffer = nullptr; }

        auto makeBuffer = [&](WGPUBufferUsage usage, const void* data, size_t byteSize) {
            WGPUBufferDescriptor desc = {};
            desc.usage = WGPUBufferUsage_CopyDst | usage;
            desc.size  = byteSize;
            WGPUBuffer buf = wgpuDeviceCreateBuffer(m_device, &desc);
            wgpuQueueWriteBuffer(m_queue, buf, 0, data, byteSize);
            return buf;
        };

        m_vertexBuffer = makeBuffer(WGPUBufferUsage_Vertex, vertices.data(), vertices.size() * sizeof(Vertex));
        m_indexBuffer  = makeBuffer(WGPUBufferUsage_Index,  indices.data(),  indices.size()  * sizeof(uint32_t));

        {
            WGPUBufferDescriptor desc = {};
            desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
            desc.size  = indices.size() * sizeof(uint32_t);
            m_activeIndexBuffer            = wgpuDeviceCreateBuffer(m_device, &desc);
            m_activeTransparentIndexBuffer = wgpuDeviceCreateBuffer(m_device, &desc);
            m_activeIndexCount            = 0;
            m_activeTransparentIndexCount = 0;
        }

        std::vector<uint32_t> lineIndices;
        lineIndices.reserve(indices.size() * 2);
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            lineIndices.push_back(indices[i]);   lineIndices.push_back(indices[i+1]);
            lineIndices.push_back(indices[i+1]); lineIndices.push_back(indices[i+2]);
            lineIndices.push_back(indices[i+2]); lineIndices.push_back(indices[i]);
        }
        m_lineIndexBuffer = makeBuffer(WGPUBufferUsage_Index, lineIndices.data(), lineIndices.size() * sizeof(uint32_t));
    }

    void GraphicsContext::UpdateGeometry(const std::vector<Vertex>& vertices) {
        if (m_vertexBuffer && !vertices.empty())
            wgpuQueueWriteBuffer(m_queue, m_vertexBuffer, 0,
                                 vertices.data(), vertices.size() * sizeof(Vertex));
    }

    void GraphicsContext::UpdateActiveBatches(const std::map<int, std::vector<uint32_t>>& solidMap, const std::map<int, std::vector<uint32_t>>& transMap) {
        std::vector<uint32_t> flatSolid;
        m_solidBatches.clear();
        for (const auto& [texIdx, inds] : solidMap) {
            if (inds.empty()) continue;
            RenderBatch b;
            b.startIndex = static_cast<uint32_t>(flatSolid.size());
            b.indexCount = static_cast<uint32_t>(inds.size());
            b.textureIndex = texIdx;
            m_solidBatches.push_back(b);
            flatSolid.insert(flatSolid.end(), inds.begin(), inds.end());
        }

        m_activeIndexCount = static_cast<uint32_t>(flatSolid.size());
        if (m_activeIndexBuffer && m_activeIndexCount > 0)
            wgpuQueueWriteBuffer(m_queue, m_activeIndexBuffer, 0, flatSolid.data(), flatSolid.size() * sizeof(uint32_t));

        std::vector<uint32_t> flatTrans;
        m_transparentBatches.clear();
        for (const auto& [texIdx, inds] : transMap) {
            if (inds.empty()) continue;
            RenderBatch b;
            b.startIndex = static_cast<uint32_t>(flatTrans.size());
            b.indexCount = static_cast<uint32_t>(inds.size());
            b.textureIndex = texIdx;
            m_transparentBatches.push_back(b);
            flatTrans.insert(flatTrans.end(), inds.begin(), inds.end());
        }

        m_activeTransparentIndexCount = static_cast<uint32_t>(flatTrans.size());
        if (m_activeTransparentIndexBuffer && m_activeTransparentIndexCount > 0)
            wgpuQueueWriteBuffer(m_queue, m_activeTransparentIndexBuffer, 0, flatTrans.data(), flatTrans.size() * sizeof(uint32_t));
    }

    void GraphicsContext::Resize(int newWidth, int newHeight) {
        if (newWidth <= 0 || newHeight <= 0) return;
        m_width  = static_cast<uint32_t>(newWidth);
        m_height = static_cast<uint32_t>(newHeight);

        WGPUSurfaceConfiguration cfg = {};
        cfg.device      = m_device;
        cfg.format      = m_surfaceFormat;
        cfg.usage       = WGPUTextureUsage_RenderAttachment;
        cfg.alphaMode   = WGPUCompositeAlphaMode_Opaque;
        cfg.width       = m_width;
        cfg.height      = m_height;
        cfg.presentMode = WGPUPresentMode_Fifo;
        wgpuSurfaceConfigure(m_surface, &cfg);

        CreateRenderTargets();
    }

    void GraphicsContext::SetHighlight(bool active, const std::vector<HighlightRange>& ranges, int style) {
        m_hasHighlight    = active;
        m_highlightRanges = ranges;
        m_highlightStyle  = style;
    }

    void GraphicsContext::SetBoundingBox(bool active, const glm::vec3& minB, const glm::vec3& maxB) {
        m_showAABB = active;
        if (!active) return;

        Vertex v[8] = {};
        v[0].position[0]=minB.x; v[0].position[1]=minB.y; v[0].position[2]=minB.z;
        v[1].position[0]=maxB.x; v[1].position[1]=minB.y; v[1].position[2]=minB.z;
        v[2].position[0]=maxB.x; v[2].position[1]=minB.y; v[2].position[2]=maxB.z;
        v[3].position[0]=minB.x; v[3].position[1]=minB.y; v[3].position[2]=maxB.z;
        v[4].position[0]=minB.x; v[4].position[1]=maxB.y; v[4].position[2]=minB.z;
        v[5].position[0]=maxB.x; v[5].position[1]=maxB.y; v[5].position[2]=minB.z;
        v[6].position[0]=maxB.x; v[6].position[1]=maxB.y; v[6].position[2]=maxB.z;
        v[7].position[0]=minB.x; v[7].position[1]=maxB.y; v[7].position[2]=maxB.z;
        wgpuQueueWriteBuffer(m_queue, m_aabbVertexBuffer, 0, v, sizeof(v));
    }

void GraphicsContext::SetClippingPlanes(
        bool actXMin, float xMin, bool actXMax, float xMax, const float* colX,
        bool actYMin, float yMin, bool actYMax, float yMax, const float* colY,
        bool actZMin, float zMin, bool actZMax, float zMax, const float* colZ,
        const glm::vec3& minB, const glm::vec3& maxB)
    {
        std::vector<Vertex>   glassVerts, capVerts;
        std::vector<uint32_t> glassInds, capInds;
        glassVerts.reserve(24); glassInds.reserve(36);
        capVerts.reserve(24); capInds.reserve(36);

        const glm::vec3 minP = minB - glm::vec3(2.0f);
        const glm::vec3 maxP = maxB + glm::vec3(2.0f);

        auto addPlane = [&](float px, float py, float pz, float nx, float ny, float nz, float cr, float cg, float cb, bool isActive) {
            // --- FIXED: If the plane is turned off, do not render its capping quad! ---
            if (!isActive) return;

            Vertex v[4] = {};
            for(int i=0; i<4; ++i) {
                v[i].normal[0]=nx; v[i].normal[1]=ny; v[i].normal[2]=nz;
                v[i].color[0]=cr; v[i].color[1]=cg; v[i].color[2]=cb;
            }
            if (nx != 0.0f) {
                v[0].position[0]=px; v[0].position[1]=minP.y; v[0].position[2]=minP.z;
                v[1].position[0]=px; v[1].position[1]=maxP.y; v[1].position[2]=minP.z;
                v[2].position[0]=px; v[2].position[1]=maxP.y; v[2].position[2]=maxP.z;
                v[3].position[0]=px; v[3].position[1]=minP.y; v[3].position[2]=maxP.z;
            } else if (ny != 0.0f) {
                v[0].position[0]=minP.x; v[0].position[1]=py; v[0].position[2]=minP.z;
                v[1].position[0]=maxP.x; v[1].position[1]=py; v[1].position[2]=minP.z;
                v[2].position[0]=maxP.x; v[2].position[1]=py; v[2].position[2]=maxP.z;
                v[3].position[0]=minP.x; v[3].position[1]=py; v[3].position[2]=maxP.z;
            } else {
                v[0].position[0]=minP.x; v[0].position[1]=minP.y; v[0].position[2]=pz;
                v[1].position[0]=maxP.x; v[1].position[1]=minP.y; v[1].position[2]=pz;
                v[2].position[0]=maxP.x; v[2].position[1]=maxP.y; v[2].position[2]=pz;
                v[3].position[0]=minP.x; v[3].position[1]=maxP.y; v[3].position[2]=pz;
            }

            uint32_t ci = static_cast<uint32_t>(capVerts.size());
            capVerts.insert(capVerts.end(), {v[0], v[1], v[2], v[3]});
            capInds.insert(capInds.end(), {ci, ci+1, ci+2, ci, ci+2, ci+3});

            uint32_t gi = static_cast<uint32_t>(glassVerts.size());
            glassVerts.insert(glassVerts.end(), {v[0], v[1], v[2], v[3]});
            glassInds.insert(glassInds.end(), {gi, gi+1, gi+2, gi, gi+2, gi+3});
        };

        addPlane(xMin+0.001f, 0, 0,  1,0,0, colX[0], colX[1], colX[2], actXMin);
        addPlane(xMax-0.001f, 0, 0, -1,0,0, colX[0], colX[1], colX[2], actXMax);
        addPlane(0, yMin+0.001f, 0,  0,1,0, colY[0], colY[1], colY[2], actYMin);
        addPlane(0, yMax-0.001f, 0,  0,-1,0, colY[0], colY[1], colY[2], actYMax);
        addPlane(0, 0, zMin+0.001f,  0,0,1, colZ[0], colZ[1], colZ[2], actZMin);
        addPlane(0, 0, zMax-0.001f,  0,0,-1, colZ[0], colZ[1], colZ[2], actZMax);

        m_capIndexCount = static_cast<uint32_t>(capInds.size());
        if (m_capIndexCount > 0 && m_capVertexBuffer && m_capIndexBuffer) {
            wgpuQueueWriteBuffer(m_queue, m_capVertexBuffer, 0, capVerts.data(), capVerts.size()*sizeof(Vertex));
            wgpuQueueWriteBuffer(m_queue, m_capIndexBuffer,  0, capInds.data(),  capInds.size()*sizeof(uint32_t));
        }

        m_glassIndexCount = static_cast<uint32_t>(glassInds.size());
        if (m_glassIndexCount > 0 && m_glassVertexBuffer && m_glassIndexBuffer) {
            wgpuQueueWriteBuffer(m_queue, m_glassVertexBuffer, 0, glassVerts.data(), glassVerts.size()*sizeof(Vertex));
            wgpuQueueWriteBuffer(m_queue, m_glassIndexBuffer,  0, glassInds.data(),  glassInds.size()*sizeof(uint32_t));
        }
    }
    
    void GraphicsContext::RenderFrame() {
        if (!m_sceneBindGroup) return;

        WGPUSurfaceTexture surfTex = {};
        wgpuSurfaceGetCurrentTexture(m_surface, &surfTex);
        if (!surfTex.texture) return;

        WGPUTextureView view = wgpuTextureCreateView(surfTex.texture, nullptr);
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);

        WGPURenderPassColorAttachment colorAtt = {};
        colorAtt.view       = m_offscreenView;
        colorAtt.loadOp     = WGPULoadOp_Clear;
        colorAtt.storeOp    = WGPUStoreOp_Store;
        colorAtt.clearValue = { 0.11f, 0.11f, 0.13f, 1.0f };

        WGPURenderPassDepthStencilAttachment depthAtt = {};
        depthAtt.view              = m_depthView;
        depthAtt.depthClearValue   = 1.0f;
        depthAtt.depthLoadOp       = WGPULoadOp_Clear;
        depthAtt.depthStoreOp      = WGPUStoreOp_Store;
        depthAtt.stencilClearValue = 0;
        depthAtt.stencilLoadOp     = WGPULoadOp_Clear;
        depthAtt.stencilStoreOp    = WGPUStoreOp_Store;

        WGPURenderPassDescriptor rpDesc = {};
        rpDesc.colorAttachmentCount      = 1;
        rpDesc.colorAttachments          = &colorAtt;
        rpDesc.depthStencilAttachment    = &depthAtt;

        WGPURenderPassEncoder rp = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
        wgpuRenderPassEncoderSetBindGroup(rp, 0, m_sceneBindGroup, 0, nullptr);

        if (m_vertexBuffer && m_activeIndexBuffer && m_activeIndexCount > 0) {
            wgpuRenderPassEncoderSetPipeline(rp, m_pipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_activeIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);

            for (const auto& batch : m_solidBatches) {
                WGPUBindGroup targetBG = m_defaultMaterialBindGroup;
                if (batch.textureIndex >= 0 && batch.textureIndex < static_cast<int>(m_gpuTextures.size())) {
                    targetBG = m_gpuTextures[batch.textureIndex].bindGroup;
                }
                wgpuRenderPassEncoderSetBindGroup(rp, 1, targetBG, 0, nullptr);
                wgpuRenderPassEncoderDrawIndexed(rp, batch.indexCount, 1, batch.startIndex, 0, 0);
            }
        }

        if (m_vertexBuffer && m_activeIndexBuffer && m_activeIndexCount > 0) {
            wgpuRenderPassEncoderSetPipeline(rp, m_stencilMaskPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_activeIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, m_activeIndexCount, 1, 0, 0, 0);
        }

        if (m_capIndexCount > 0 && m_capVertexBuffer && m_capIndexBuffer) {
            wgpuRenderPassEncoderSetPipeline(rp, m_capPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_capVertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_capIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetStencilReference(rp, 0); // Check for NotEqual 0
            wgpuRenderPassEncoderDrawIndexed(rp, m_capIndexCount, 1, 0, 0, 0);
        }

        // --- NEW: Draw the Infinite Grid BEFORE transparent models ---
        if (m_gridPipeline) {
            wgpuRenderPassEncoderSetPipeline(rp, m_gridPipeline);
            wgpuRenderPassEncoderDraw(rp, 6, 1, 0, 0);
        }

        if (m_vertexBuffer && m_activeTransparentIndexBuffer && m_activeTransparentIndexCount > 0) {
            wgpuRenderPassEncoderSetPipeline(rp, m_transparentPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_activeTransparentIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);

            for (const auto& batch : m_transparentBatches) {
                WGPUBindGroup targetBG = m_defaultMaterialBindGroup;
                if (batch.textureIndex >= 0 && batch.textureIndex < static_cast<int>(m_gpuTextures.size())) {
                    targetBG = m_gpuTextures[batch.textureIndex].bindGroup;
                }
                wgpuRenderPassEncoderSetBindGroup(rp, 1, targetBG, 0, nullptr);
                wgpuRenderPassEncoderDrawIndexed(rp, batch.indexCount, 1, batch.startIndex, 0, 0);
            }
        }

        if (m_glassIndexCount > 0 && m_glassVertexBuffer && m_glassIndexBuffer) {
            wgpuRenderPassEncoderSetPipeline(rp, m_glassPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_glassVertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_glassIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, m_glassIndexCount, 1, 0, 0, 0);
        }

        if (m_hasHighlight && !m_highlightRanges.empty() && m_vertexBuffer) {
            if (m_highlightStyle == 1 && m_lineIndexBuffer) {
                wgpuRenderPassEncoderSetPipeline(rp, m_highlightOutlinePipeline);
                wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
                wgpuRenderPassEncoderSetIndexBuffer(rp, m_lineIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
                for (const auto& range : m_highlightRanges) {
                    wgpuRenderPassEncoderDrawIndexed(rp, range.indexCount * 2, 1, range.startIndex * 2, 0, 0);
                }
            } else if (m_highlightStyle == 0 && m_indexBuffer) {
                wgpuRenderPassEncoderSetPipeline(rp, m_highlightSolidPipeline);
                wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
                wgpuRenderPassEncoderSetIndexBuffer(rp, m_indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
                for (const auto& range : m_highlightRanges) {
                    wgpuRenderPassEncoderDrawIndexed(rp, range.indexCount, 1, range.startIndex, 0, 0);
                }
            }
        }

        if (m_showAABB && m_aabbVertexBuffer && m_aabbIndexBuffer) {
            wgpuRenderPassEncoderSetPipeline(rp, m_aabbPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_aabbVertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_aabbIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, 24, 1, 0, 0, 0);
        }

        wgpuRenderPassEncoderEnd(rp);

        if (m_ssaoBindGroup) {
            WGPURenderPassColorAttachment ssaoAtt = {};
            ssaoAtt.view = view;
            ssaoAtt.loadOp = WGPULoadOp_Clear;
            ssaoAtt.storeOp = WGPUStoreOp_Store;
            ssaoAtt.clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };

            WGPURenderPassDescriptor ssaoDesc = {};
            ssaoDesc.colorAttachmentCount = 1;
            ssaoDesc.colorAttachments = &ssaoAtt;

            WGPURenderPassEncoder ssaoRp = wgpuCommandEncoderBeginRenderPass(encoder, &ssaoDesc);

            wgpuRenderPassEncoderSetPipeline(ssaoRp, m_ssaoPipeline);
            wgpuRenderPassEncoderSetBindGroup(ssaoRp, 0, m_ssaoBindGroup, 0, nullptr);
            wgpuRenderPassEncoderDraw(ssaoRp, 3, 1, 0, 0);

            ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), ssaoRp);
            wgpuRenderPassEncoderEnd(ssaoRp);
        }

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(m_queue, 1, &cmd);
        wgpuSurfacePresent(m_surface);

        wgpuCommandBufferRelease(cmd);
        wgpuRenderPassEncoderRelease(rp);
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(view);
    }

    void GraphicsContext::InitImGui(GLFWwindow* window) {
        BIM_LOG("UI", "Initialising Dear ImGui...");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

        io.Fonts->AddFontDefault();

        ImFontConfig fontCfg;
        fontCfg.MergeMode       = true;
        fontCfg.PixelSnapH      = true;
        fontCfg.GlyphMinAdvanceX = 14.0f;
        static const ImWchar iconRanges[] = { 0xe000, 0xf8ff, 0 };
        io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 14.0f, &fontCfg, iconRanges);

        ImGui_ImplGlfw_InitForOther(window, true);

        ImGui_ImplWGPU_InitInfo wgpuInfo = {};
        wgpuInfo.Device              = m_device;
        wgpuInfo.NumFramesInFlight   = 3;
        wgpuInfo.RenderTargetFormat  = m_surfaceFormat;
        wgpuInfo.DepthStencilFormat  = WGPUTextureFormat_Undefined;
        ImGui_ImplWGPU_Init(&wgpuInfo);
    }

    void GraphicsContext::ShutdownImGui() {
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

} // namespace BimCore
