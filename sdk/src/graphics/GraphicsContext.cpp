// =============================================================================
// BimCore/graphics/GraphicsContext.cpp
// =============================================================================
#include "GraphicsContext.h"
#include "ShaderLibrary.h" // <-- NEW: Include our extracted shaders
#include "Core.h"
#include <stdexcept>
#include <cstring>
#include <vector>
#include <iostream>

// ImGui backend headers
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>

// Platform-specific GLFW native handle accessors
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

    // =============================================================================
    // Construction / Destruction
    // =============================================================================

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
        CreateDepthTexture();
        CreateUniformBuffers();
        CreateMainPipeline();
        CreateHighlightPipeline();
        CreateAABBPipeline();
        CreateGlassPipeline();
        AllocateGeometryBuffers();
        BIM_LOG("GPU", "Graphics context fully initialised.");
    }

    GraphicsContext::~GraphicsContext() {
        // Release in reverse-init order
        if (m_glassIndexBuffer)              wgpuBufferRelease(m_glassIndexBuffer);
        if (m_glassVertexBuffer)             wgpuBufferRelease(m_glassVertexBuffer);
        if (m_aabbIndexBuffer)               wgpuBufferRelease(m_aabbIndexBuffer);
        if (m_aabbVertexBuffer)              wgpuBufferRelease(m_aabbVertexBuffer);
        if (m_lineIndexBuffer)               wgpuBufferRelease(m_lineIndexBuffer);
        if (m_activeTransparentIndexBuffer)  wgpuBufferRelease(m_activeTransparentIndexBuffer);
        if (m_activeIndexBuffer)             wgpuBufferRelease(m_activeIndexBuffer);
        if (m_indexBuffer)                   wgpuBufferRelease(m_indexBuffer);
        if (m_vertexBuffer)                  wgpuBufferRelease(m_vertexBuffer);

        if (m_glassPipeline)                 wgpuRenderPipelineRelease(m_glassPipeline);
        if (m_aabbPipeline)                  wgpuRenderPipelineRelease(m_aabbPipeline);
        if (m_highlightOutlinePipeline)      wgpuRenderPipelineRelease(m_highlightOutlinePipeline);
        if (m_highlightSolidPipeline)        wgpuRenderPipelineRelease(m_highlightSolidPipeline);
        if (m_transparentPipeline)           wgpuRenderPipelineRelease(m_transparentPipeline);
        if (m_pipeline)                      wgpuRenderPipelineRelease(m_pipeline);

        if (m_sceneBindGroup)                wgpuBindGroupRelease(m_sceneBindGroup);
        if (m_instanceBuffer)                wgpuBufferRelease(m_instanceBuffer);
        if (m_uniformBuffer)                 wgpuBufferRelease(m_uniformBuffer);

        ReleaseDepthTexture();

        if (m_queue)    wgpuQueueRelease(m_queue);
        if (m_device)   wgpuDeviceRelease(m_device);
        if (m_adapter)  wgpuAdapterRelease(m_adapter);
        if (m_surface)  wgpuSurfaceRelease(m_surface);
        if (m_instance) wgpuInstanceRelease(m_instance);
    }

    // =============================================================================
    // Initialisation helpers
    // =============================================================================

    void GraphicsContext::CreateSurface(GLFWwindow* window) {
        WGPUSurfaceDescriptor surfaceDesc = {};

        #if defined(BIM_PLATFORM_WINDOWS)
        BIM_LOG("GPU", "Creating Win32 HWND surface...");
        WGPUSurfaceSourceWindowsHWND hwndDesc = {};
        hwndDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        hwndDesc.hinstance   = GetModuleHandle(nullptr);
        hwndDesc.hwnd        = glfwGetWin32Window(window);
        surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&hwndDesc);

        #elif defined(BIM_PLATFORM_MACOS)
        BIM_LOG("GPU", "Creating Cocoa surface...");
        WGPUSurfaceSourceCocoaWindow cocoaDesc = {};
        cocoaDesc.chain.sType = WGPUSType_SurfaceSourceCocoaWindow;
        cocoaDesc.window      = glfwGetCocoaWindow(window);
        surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&cocoaDesc);

        #elif defined(BIM_PLATFORM_LINUX)
        const int platform = glfwGetPlatform();
        WGPUSurfaceSourceWaylandSurface waylandDesc = {};
        WGPUSurfaceSourceXlibWindow     x11Desc     = {};

        if (platform == GLFW_PLATFORM_WAYLAND) {
            BIM_LOG("GPU", "Creating Wayland surface...");
            waylandDesc.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
            waylandDesc.display     = glfwGetWaylandDisplay();
            waylandDesc.surface     = glfwGetWaylandWindow(window);
            surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&waylandDesc);
        } else if (platform == GLFW_PLATFORM_X11) {
            BIM_LOG("GPU", "Creating X11 surface...");
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

        m_surfaceFormat = (caps.formatCount > 0)
        ? caps.formats[0]
        : WGPUTextureFormat_BGRA8Unorm;

        WGPUSurfaceConfiguration cfg = {};
        cfg.device      = m_device;
        cfg.format      = m_surfaceFormat;
        cfg.alphaMode   = (caps.alphaModeCount > 0)
        ? caps.alphaModes[0]
        : WGPUCompositeAlphaMode_Opaque;
        cfg.usage       = WGPUTextureUsage_RenderAttachment;
        cfg.width       = m_width;
        cfg.height      = m_height;
        cfg.presentMode = WGPUPresentMode_Fifo;

        wgpuSurfaceConfigure(m_surface, &cfg);
        wgpuSurfaceCapabilitiesFreeMembers(caps);
    }

    void GraphicsContext::CreateDepthTexture() {
        WGPUTextureDescriptor desc = {};
        desc.usage         = WGPUTextureUsage_RenderAttachment;
        desc.dimension     = WGPUTextureDimension_2D;
        desc.size          = { m_width, m_height, 1 };
        desc.format        = WGPUTextureFormat_Depth32Float;
        desc.mipLevelCount = 1;
        desc.sampleCount   = 1;
        m_depthTexture = wgpuDeviceCreateTexture(m_device, &desc);
        m_depthView    = wgpuTextureCreateView(m_depthTexture, nullptr);
    }

    void GraphicsContext::ReleaseDepthTexture() {
        if (m_depthView)    { wgpuTextureViewRelease(m_depthView);  m_depthView    = nullptr; }
        if (m_depthTexture) { wgpuTextureRelease(m_depthTexture);   m_depthTexture = nullptr; }
    }

    void GraphicsContext::CreateUniformBuffers() {
        // Uniform buffer (scene data — camera, lighting, clipping)
        WGPUBufferDescriptor ubDesc = {};
        ubDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
        ubDesc.size  = sizeof(SceneUniforms);
        m_uniformBuffer = wgpuDeviceCreateBuffer(m_device, &ubDesc);

        // Instance storage buffer (up to 1 million transforms)
        WGPUBufferDescriptor instDesc = {};
        instDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage;
        instDesc.size  = 1'000'000 * sizeof(glm::mat4);
        m_instanceBuffer = wgpuDeviceCreateBuffer(m_device, &instDesc);

        // Bind group layout
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
        WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(m_device, &bglDesc);

        // Bind group
        WGPUBindGroupEntry bgEntries[2] = {};
        bgEntries[0].binding = 0;
        bgEntries[0].buffer  = m_uniformBuffer;
        bgEntries[0].size    = sizeof(SceneUniforms);

        bgEntries[1].binding = 1;
        bgEntries[1].buffer  = m_instanceBuffer;
        bgEntries[1].size    = 1'000'000 * sizeof(glm::mat4);

        WGPUBindGroupDescriptor bgDesc = {};
        bgDesc.layout     = bgl;
        bgDesc.entryCount = 2;
        bgDesc.entries    = bgEntries;
        m_sceneBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);

        wgpuBindGroupLayoutRelease(bgl);
    }

    // =============================================================================
    // Pipeline creation
    // =============================================================================

    WGPUShaderModule GraphicsContext::CreateShaderModule(const char* wgsl) const {
        WGPUShaderSourceWGSL src = {};
        src.chain.sType = WGPUSType_ShaderSourceWGSL;
        src.code        = WGPUStringView{ wgsl, strlen(wgsl) };
        WGPUShaderModuleDescriptor desc = {};
        desc.nextInChain = &src.chain;
        return wgpuDeviceCreateShaderModule(m_device, &desc);
    }

    static WGPUVertexBufferLayout MakeVertexBufferLayout(std::vector<WGPUVertexAttribute>& attrs) {
        attrs.resize(3);
        attrs[0] = { WGPUVertexFormat_Float32x3, offsetof(BimCore::Vertex, position), 0 };
        attrs[1] = { WGPUVertexFormat_Float32x3, offsetof(BimCore::Vertex, normal),   1 };
        attrs[2] = { WGPUVertexFormat_Float32x3, offsetof(BimCore::Vertex, color),    2 };
        WGPUVertexBufferLayout layout = {};
        layout.arrayStride    = sizeof(BimCore::Vertex);
        layout.stepMode       = WGPUVertexStepMode_Vertex;
        layout.attributeCount = 3;
        layout.attributes     = attrs.data();
        return layout;
    }

    void GraphicsContext::CreateMainPipeline() {
        BIM_LOG("GPU", "Compiling main + transparent pipelines...");

        // Uses the centralized shader from ShaderLibrary.h
        WGPUShaderModule shader = CreateShaderModule(Shaders::kMainWGSL);

        // Pipeline layout
        WGPUBindGroupLayoutEntry bgle[2] = {};
        bgle[0].binding               = 0;
        bgle[0].visibility            = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        bgle[0].buffer.type           = WGPUBufferBindingType_Uniform;
        bgle[0].buffer.minBindingSize = sizeof(SceneUniforms);
        bgle[1].binding               = 1;
        bgle[1].visibility            = WGPUShaderStage_Vertex;
        bgle[1].buffer.type           = WGPUBufferBindingType_ReadOnlyStorage;
        bgle[1].buffer.minBindingSize = sizeof(glm::mat4);

        WGPUBindGroupLayoutDescriptor bgld = {};
        bgld.entryCount = 2; bgld.entries = bgle;
        WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(m_device, &bgld);

        WGPUPipelineLayoutDescriptor pld = {};
        pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
        WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        WGPUDepthStencilState ds = {};
        ds.format             = WGPUTextureFormat_Depth32Float;
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

        // Transparent variant — alpha blend, depth read-only, fs_transparent entry
        WGPUBlendState alphaBlend = {};
        alphaBlend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha,           WGPUBlendFactor_OneMinusSrcAlpha };
        alphaBlend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One,                WGPUBlendFactor_OneMinusSrcAlpha };

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
        wgpuBindGroupLayoutRelease(bgl);
        wgpuPipelineLayoutRelease(pipelineLayout);
    }

    void GraphicsContext::CreateHighlightPipeline() {
        BIM_LOG("GPU", "Compiling highlight pipeline...");
        WGPUShaderModule shader = CreateShaderModule(Shaders::kHighlightWGSL);

        WGPUBindGroupLayoutEntry bgle[2] = {};
        bgle[0].binding = 0; bgle[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        bgle[0].buffer.type = WGPUBufferBindingType_Uniform; bgle[0].buffer.minBindingSize = sizeof(SceneUniforms);
        bgle[1].binding = 1; bgle[1].visibility = WGPUShaderStage_Vertex;
        bgle[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage; bgle[1].buffer.minBindingSize = sizeof(glm::mat4);

        WGPUBindGroupLayoutDescriptor bgld = {}; bgld.entryCount = 2; bgld.entries = bgle;
        WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(m_device, &bgld);
        WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        WGPUBlendState blend = {};
        blend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha };
        blend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One,      WGPUBlendFactor_OneMinusSrcAlpha };
        WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.blend = &blend; target.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{ "fs_main", 7 }; frag.targetCount = 1; frag.targets = &target;

        WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth32Float;
        ds.depthCompare = WGPUCompareFunction_LessEqual; ds.depthWriteEnabled = WGPUOptionalBool_False;

        WGPURenderPipelineDescriptor pd = {};
        pd.layout = layout; pd.vertex.module = shader;
        pd.vertex.entryPoint = WGPUStringView{ "vs_main", 7 };
        pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl;
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pd.primitive.cullMode = WGPUCullMode_Back; pd.primitive.frontFace = WGPUFrontFace_CCW;
        pd.depthStencil = &ds; pd.fragment = &frag;
        pd.multisample.count = 1; pd.multisample.mask = ~0u;
        m_highlightSolidPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        pd.primitive.topology = WGPUPrimitiveTopology_LineList;
        m_highlightOutlinePipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        wgpuShaderModuleRelease(shader);
        wgpuBindGroupLayoutRelease(bgl);
        wgpuPipelineLayoutRelease(layout);
    }

    void GraphicsContext::CreateAABBPipeline() {
        BIM_LOG("GPU", "Compiling AABB pipeline...");
        WGPUShaderModule shader = CreateShaderModule(Shaders::kAABBWGSL);

        WGPUBindGroupLayoutEntry bgle[2] = {};
        bgle[0].binding = 0; bgle[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        bgle[0].buffer.type = WGPUBufferBindingType_Uniform; bgle[0].buffer.minBindingSize = sizeof(SceneUniforms);
        bgle[1].binding = 1; bgle[1].visibility = WGPUShaderStage_Vertex;
        bgle[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage; bgle[1].buffer.minBindingSize = sizeof(glm::mat4);

        WGPUBindGroupLayoutDescriptor bgld = {}; bgld.entryCount = 2; bgld.entries = bgle;
        WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(m_device, &bgld);
        WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{ "fs_main", 7 }; frag.targetCount = 1; frag.targets = &target;
        WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth32Float;
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
        wgpuBindGroupLayoutRelease(bgl);
        wgpuPipelineLayoutRelease(layout);
    }

    void GraphicsContext::CreateGlassPipeline() {
        BIM_LOG("GPU", "Compiling glass clipping-plane pipeline...");
        WGPUShaderModule shader = CreateShaderModule(Shaders::kGlassWGSL);

        WGPUBindGroupLayoutEntry bgle[2] = {};
        bgle[0].binding = 0; bgle[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        bgle[0].buffer.type = WGPUBufferBindingType_Uniform; bgle[0].buffer.minBindingSize = sizeof(SceneUniforms);
        bgle[1].binding = 1; bgle[1].visibility = WGPUShaderStage_Vertex;
        bgle[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage; bgle[1].buffer.minBindingSize = sizeof(glm::mat4);

        WGPUBindGroupLayoutDescriptor bgld = {}; bgld.entryCount = 2; bgld.entries = bgle;
        WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(m_device, &bgld);
        WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);

        std::vector<WGPUVertexAttribute> attrs;
        WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);

        WGPUBlendState blend = {};
        blend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha };
        blend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One,      WGPUBlendFactor_OneMinusSrcAlpha };
        WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.blend = &blend; target.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{ "fs_main", 7 }; frag.targetCount = 1; frag.targets = &target;
        WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth32Float;
        ds.depthCompare = WGPUCompareFunction_LessEqual; ds.depthWriteEnabled = WGPUOptionalBool_False;

        WGPURenderPipelineDescriptor pd = {};
        pd.layout = layout; pd.vertex.module = shader;
        pd.vertex.entryPoint = WGPUStringView{ "vs_main", 7 };
        pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl;
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pd.primitive.cullMode = WGPUCullMode_None; // Glass is double-sided
        pd.depthStencil = &ds; pd.fragment = &frag;
        pd.multisample.count = 1; pd.multisample.mask = ~0u;
        m_glassPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);

        wgpuShaderModuleRelease(shader);
        wgpuBindGroupLayoutRelease(bgl);
        wgpuPipelineLayoutRelease(layout);
    }

    void GraphicsContext::AllocateGeometryBuffers() {
        // AABB vertex buffer (8 corners, fixed size)
        WGPUBufferDescriptor avbd = {}; avbd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; avbd.size = 8 * sizeof(Vertex);
        m_aabbVertexBuffer = wgpuDeviceCreateBuffer(m_device, &avbd);

        constexpr uint32_t kAABBIndices[] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
        WGPUBufferDescriptor aibd = {}; aibd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index; aibd.size = sizeof(kAABBIndices);
        m_aabbIndexBuffer = wgpuDeviceCreateBuffer(m_device, &aibd);
        wgpuQueueWriteBuffer(m_queue, m_aabbIndexBuffer, 0, kAABBIndices, sizeof(kAABBIndices));

        // Glass clipping plane buffers (max 3 planes × 4 verts / 6 indices each)
        WGPUBufferDescriptor gvbd = {}; gvbd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; gvbd.size = 12 * sizeof(Vertex);
        m_glassVertexBuffer = wgpuDeviceCreateBuffer(m_device, &gvbd);
        WGPUBufferDescriptor gibd = {}; gibd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index; gibd.size = 18 * sizeof(uint32_t);
        m_glassIndexBuffer = wgpuDeviceCreateBuffer(m_device, &gibd);
    }

    // =============================================================================
    // Per-frame public interface
    // =============================================================================

    void GraphicsContext::UpdateScene(const SceneUniforms& uniforms) {
        if (m_uniformBuffer)
            wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0, &uniforms, sizeof(SceneUniforms));
    }

    void GraphicsContext::UpdateInstanceData(const std::vector<glm::mat4>& transforms) {
        if (!m_instanceBuffer || transforms.empty()) return;
        m_instanceCount = static_cast<uint32_t>(transforms.size());
        wgpuQueueWriteBuffer(m_queue, m_instanceBuffer, 0,
                             transforms.data(), transforms.size() * sizeof(glm::mat4));
    }

    void GraphicsContext::UploadMesh(const std::vector<Vertex>& vertices,
                                     const std::vector<uint32_t>& indices)
    {
        if (vertices.empty() || indices.empty()) return;
        m_indexCount = static_cast<uint32_t>(indices.size());

        // Release old mesh buffers
        if (m_vertexBuffer) { wgpuBufferRelease(m_vertexBuffer); m_vertexBuffer = nullptr; }
        if (m_indexBuffer)  { wgpuBufferRelease(m_indexBuffer);  m_indexBuffer  = nullptr; }
        if (m_activeIndexBuffer) { wgpuBufferRelease(m_activeIndexBuffer); m_activeIndexBuffer = nullptr; }
        if (m_activeTransparentIndexBuffer) { wgpuBufferRelease(m_activeTransparentIndexBuffer); m_activeTransparentIndexBuffer = nullptr; }
        if (m_lineIndexBuffer)  { wgpuBufferRelease(m_lineIndexBuffer);  m_lineIndexBuffer  = nullptr; }

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

        // Dynamic visibility buffers — same size as the full index buffer
        {
            WGPUBufferDescriptor desc = {};
            desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
            desc.size  = indices.size() * sizeof(uint32_t);
            m_activeIndexBuffer            = wgpuDeviceCreateBuffer(m_device, &desc);
            m_activeTransparentIndexBuffer = wgpuDeviceCreateBuffer(m_device, &desc);
            m_activeIndexCount            = 0;
            m_activeTransparentIndexCount = 0;
        }

        // Wireframe line index buffer (each triangle edge = 2 indices)
        std::vector<uint32_t> lineIndices;
        lineIndices.reserve(indices.size() * 2);
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            lineIndices.push_back(indices[i]);   lineIndices.push_back(indices[i+1]);
            lineIndices.push_back(indices[i+1]); lineIndices.push_back(indices[i+2]);
            lineIndices.push_back(indices[i+2]); lineIndices.push_back(indices[i]);
        }
        m_lineIndexBuffer = makeBuffer(WGPUBufferUsage_Index,
                                       lineIndices.data(), lineIndices.size() * sizeof(uint32_t));

        BIM_LOG("GPU", "Mesh uploaded — " << vertices.size() << " verts, "
        << indices.size() << " indices.");
    }

    void GraphicsContext::UpdateGeometry(const std::vector<Vertex>& vertices) {
        if (m_vertexBuffer && !vertices.empty())
            wgpuQueueWriteBuffer(m_queue, m_vertexBuffer, 0,
                                 vertices.data(), vertices.size() * sizeof(Vertex));
    }

    void GraphicsContext::UpdateActiveIndices(const std::vector<uint32_t>& solidIdx,
                                              const std::vector<uint32_t>& transparentIdx)
    {
        m_activeIndexCount = static_cast<uint32_t>(solidIdx.size());
        if (m_activeIndexBuffer && m_activeIndexCount > 0)
            wgpuQueueWriteBuffer(m_queue, m_activeIndexBuffer, 0,
                                 solidIdx.data(), solidIdx.size() * sizeof(uint32_t));

            m_activeTransparentIndexCount = static_cast<uint32_t>(transparentIdx.size());
        if (m_activeTransparentIndexBuffer && m_activeTransparentIndexCount > 0)
            wgpuQueueWriteBuffer(m_queue, m_activeTransparentIndexBuffer, 0,
                                 transparentIdx.data(), transparentIdx.size() * sizeof(uint32_t));
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

        ReleaseDepthTexture();
        CreateDepthTexture();
    }

    // =============================================================================
    // Scene-state setters
    // =============================================================================

    void GraphicsContext::SetHighlight(bool active,
                                       const std::vector<HighlightRange>& ranges,
                                       int style)
    {
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

    void GraphicsContext::SetClippingPlanes(bool activeX, float x, const float* colX,
                                            bool activeY, float y, const float* colY,
                                            bool activeZ, float z, const float* colZ,
                                            const glm::vec3& minB,
                                            const glm::vec3& maxB)
    {
        std::vector<Vertex>   verts;
        std::vector<uint32_t> inds;
        verts.reserve(12);
        inds.reserve(18);

        const glm::vec3 minP = minB - glm::vec3(2.0f);
        const glm::vec3 maxP = maxB + glm::vec3(2.0f);

        auto addPlane = [&](float px, float py, float pz,
                            float nx, float ny, float nz,
                            float cr, float cg, float cb)
        {
            uint32_t i = static_cast<uint32_t>(verts.size());
            Vertex v = {}; v.normal[0]=nx; v.normal[1]=ny; v.normal[2]=nz;
            v.color[0]=cr; v.color[1]=cg; v.color[2]=cb;
            if (nx != 0.0f) {
                v.position[0]=px; v.position[1]=minP.y; v.position[2]=minP.z; verts.push_back(v);
                v.position[0]=px; v.position[1]=maxP.y; v.position[2]=minP.z; verts.push_back(v);
                v.position[0]=px; v.position[1]=maxP.y; v.position[2]=maxP.z; verts.push_back(v);
                v.position[0]=px; v.position[1]=minP.y; v.position[2]=maxP.z; verts.push_back(v);
            } else if (ny != 0.0f) {
                v.position[0]=minP.x; v.position[1]=py; v.position[2]=minP.z; verts.push_back(v);
                v.position[0]=maxP.x; v.position[1]=py; v.position[2]=minP.z; verts.push_back(v);
                v.position[0]=maxP.x; v.position[1]=py; v.position[2]=maxP.z; verts.push_back(v);
                v.position[0]=minP.x; v.position[1]=py; v.position[2]=maxP.z; verts.push_back(v);
            } else {
                v.position[0]=minP.x; v.position[1]=minP.y; v.position[2]=pz; verts.push_back(v);
                v.position[0]=maxP.x; v.position[1]=minP.y; v.position[2]=pz; verts.push_back(v);
                v.position[0]=maxP.x; v.position[1]=maxP.y; v.position[2]=pz; verts.push_back(v);
                v.position[0]=minP.x; v.position[1]=maxP.y; v.position[2]=pz; verts.push_back(v);
            }
            inds.insert(inds.end(), {i, i+1, i+2, i, i+2, i+3});
        };

        if (activeX) addPlane(x+0.001f, 0,       0,       1,0,0, colX[0], colX[1], colX[2]);
        if (activeY) addPlane(0,       y+0.001f, 0,       0,1,0, colY[0], colY[1], colY[2]);
        if (activeZ) addPlane(0,       0,       z+0.001f, 0,0,1, colZ[0], colZ[1], colZ[2]);

        m_glassIndexCount = static_cast<uint32_t>(inds.size());
        if (m_glassIndexCount > 0) {
            wgpuQueueWriteBuffer(m_queue, m_glassVertexBuffer, 0, verts.data(), verts.size()*sizeof(Vertex));
            wgpuQueueWriteBuffer(m_queue, m_glassIndexBuffer,  0, inds.data(),  inds.size()*sizeof(uint32_t));
        }
    }

    // =============================================================================
    // Render
    // =============================================================================

    void GraphicsContext::RenderFrame() {
        WGPUSurfaceTexture surfTex = {};
        wgpuSurfaceGetCurrentTexture(m_surface, &surfTex);
        if (!surfTex.texture) return;

        WGPUTextureView view = wgpuTextureCreateView(surfTex.texture, nullptr);
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);

        WGPURenderPassColorAttachment colorAtt = {};
        colorAtt.view       = view;
        colorAtt.loadOp     = WGPULoadOp_Clear;
        colorAtt.storeOp    = WGPUStoreOp_Store;
        colorAtt.clearValue = { 0.12, 0.14, 0.18, 1.0 };

        WGPURenderPassDepthStencilAttachment depthAtt = {};
        depthAtt.view              = m_depthView;
        depthAtt.depthClearValue   = 1.0f;
        depthAtt.depthLoadOp       = WGPULoadOp_Clear;
        depthAtt.depthStoreOp      = WGPUStoreOp_Store;

        WGPURenderPassDescriptor rpDesc = {};
        rpDesc.colorAttachmentCount      = 1;
        rpDesc.colorAttachments          = &colorAtt;
        rpDesc.depthStencilAttachment    = &depthAtt;

        WGPURenderPassEncoder rp = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
        wgpuRenderPassEncoderSetBindGroup(rp, 0, m_sceneBindGroup, 0, nullptr);

        // 1. Opaque geometry
        if (m_vertexBuffer && m_activeIndexBuffer && m_activeIndexCount > 0) {
            wgpuRenderPassEncoderSetPipeline(rp, m_pipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_activeIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, m_activeIndexCount, 1, 0, 0, 0);
        }

        // 2. Transparent (IFC glass)
        if (m_vertexBuffer && m_activeTransparentIndexBuffer && m_activeTransparentIndexCount > 0) {
            wgpuRenderPassEncoderSetPipeline(rp, m_transparentPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_activeTransparentIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, m_activeTransparentIndexCount, 1, 0, 0, 0);
        }

        // 3. Highlight overlay
        if (m_hasHighlight && !m_highlightRanges.empty() && m_vertexBuffer && m_indexBuffer) {
            WGPURenderPipeline hlPipeline = (m_highlightStyle == 1)
            ? m_highlightOutlinePipeline
            : m_highlightSolidPipeline;
            wgpuRenderPassEncoderSetPipeline(rp, hlPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            for (const auto& range : m_highlightRanges)
                wgpuRenderPassEncoderDrawIndexed(rp, range.indexCount, 1, range.startIndex, 0, 0);
        }

        // 4. AABB bounding box
        if (m_showAABB && m_aabbVertexBuffer && m_aabbIndexBuffer) {
            wgpuRenderPassEncoderSetPipeline(rp, m_aabbPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_aabbVertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_aabbIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, 24, 1, 0, 0, 0);
        }

        // 5. Glass clipping planes
        if (m_glassIndexCount > 0 && m_glassVertexBuffer && m_glassIndexBuffer) {
            wgpuRenderPassEncoderSetPipeline(rp, m_glassPipeline);
            wgpuRenderPassEncoderSetVertexBuffer(rp, 0, m_glassVertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(rp, m_glassIndexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(rp, m_glassIndexCount, 1, 0, 0, 0);
        }

        // 6. ImGui on top
        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), rp);

        wgpuRenderPassEncoderEnd(rp);

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(m_queue, 1, &cmd);
        wgpuSurfacePresent(m_surface);

        wgpuCommandBufferRelease(cmd);
        wgpuRenderPassEncoderRelease(rp);
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(view);
    }

    // =============================================================================
    // ImGui integration
    // =============================================================================

    void GraphicsContext::InitImGui(GLFWwindow* window) {
        BIM_LOG("UI", "Initialising Dear ImGui...");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        io.Fonts->AddFontDefault();

        ImFontConfig fontCfg;
        fontCfg.MergeMode       = true;
        fontCfg.PixelSnapH      = true;
        fontCfg.GlyphMinAdvanceX = 14.0f;
        static const ImWchar iconRanges[] = { 0xe000, 0xf8ff, 0 };
        io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 14.0f, &fontCfg, iconRanges);

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOther(window, true);

        ImGui_ImplWGPU_InitInfo wgpuInfo = {};
        wgpuInfo.Device              = m_device;
        wgpuInfo.NumFramesInFlight   = 3;
        wgpuInfo.RenderTargetFormat  = m_surfaceFormat;
        wgpuInfo.DepthStencilFormat  = WGPUTextureFormat_Depth32Float;
        ImGui_ImplWGPU_Init(&wgpuInfo);
    }

    void GraphicsContext::ShutdownImGui() {
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

} // namespace BimCore
