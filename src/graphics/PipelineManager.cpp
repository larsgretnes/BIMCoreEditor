// =============================================================================
// BimCore/graphics/PipelineManager.cpp
// =============================================================================
#include "PipelineManager.h"
#include "ShaderLibrary.h"
#include "GraphicsContext.h" // For Vertex offsetof
#include <vector>

namespace BimCore {

    PipelineManager::PipelineManager(WGPUDevice device, WGPUTextureFormat surfaceFormat) 
        : m_device(device), m_surfaceFormat(surfaceFormat) {
        BuildLayouts();
        BuildPipelines();
    }

    PipelineManager::~PipelineManager() {
        if (m_ssaoPipeline) wgpuRenderPipelineRelease(m_ssaoPipeline);
        if (m_gridPipeline) wgpuRenderPipelineRelease(m_gridPipeline);
        if (m_capPipeline) wgpuRenderPipelineRelease(m_capPipeline);
        if (m_stencilMaskPipeline) wgpuRenderPipelineRelease(m_stencilMaskPipeline);
        if (m_glassPipeline) wgpuRenderPipelineRelease(m_glassPipeline);
        if (m_aabbPipeline) wgpuRenderPipelineRelease(m_aabbPipeline);
        if (m_highlightOutlinePipeline) wgpuRenderPipelineRelease(m_highlightOutlinePipeline);
        if (m_highlightSolidPipeline) wgpuRenderPipelineRelease(m_highlightSolidPipeline);
        if (m_transparentPipeline) wgpuRenderPipelineRelease(m_transparentPipeline);
        if (m_mainPipeline) wgpuRenderPipelineRelease(m_mainPipeline);
        if (m_shadowPipeline) wgpuRenderPipelineRelease(m_shadowPipeline);

        if (m_ssaoLayout) wgpuBindGroupLayoutRelease(m_ssaoLayout);
        if (m_materialLayout) wgpuBindGroupLayoutRelease(m_materialLayout);
        if (m_shadowLayout) wgpuBindGroupLayoutRelease(m_shadowLayout);
        if (m_sceneLayout) wgpuBindGroupLayoutRelease(m_sceneLayout);
    }

    WGPUShaderModule PipelineManager::CreateShaderModule(const std::string& source) const {
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

    void PipelineManager::BuildLayouts() {
        {
            WGPUBindGroupLayoutEntry sceneEntries[2] = {};
            sceneEntries[0].binding = 0; sceneEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
            sceneEntries[0].buffer.type = WGPUBufferBindingType_Uniform; sceneEntries[0].buffer.minBindingSize = sizeof(SceneUniforms);
            sceneEntries[1].binding = 1; sceneEntries[1].visibility = WGPUShaderStage_Vertex;
            sceneEntries[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage; sceneEntries[1].buffer.minBindingSize = sizeof(glm::mat4);
            WGPUBindGroupLayoutDescriptor sceneDesc = {}; sceneDesc.entryCount = 2; sceneDesc.entries = sceneEntries;
            m_sceneLayout = wgpuDeviceCreateBindGroupLayout(m_device, &sceneDesc);
        }
        {
            WGPUBindGroupLayoutEntry shadowEntries[2] = {};
            shadowEntries[0].binding = 0; shadowEntries[0].visibility = WGPUShaderStage_Fragment;
            shadowEntries[0].texture.sampleType = WGPUTextureSampleType_Depth; shadowEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
            shadowEntries[1].binding = 1; shadowEntries[1].visibility = WGPUShaderStage_Fragment; shadowEntries[1].sampler.type = WGPUSamplerBindingType_Comparison;
            WGPUBindGroupLayoutDescriptor shadowDesc = {}; shadowDesc.entryCount = 2; shadowDesc.entries = shadowEntries;
            m_shadowLayout = wgpuDeviceCreateBindGroupLayout(m_device, &shadowDesc);
        }
        {
            WGPUBindGroupLayoutEntry bgle[2] = {};
            bgle[0].binding = 0; bgle[0].visibility = WGPUShaderStage_Fragment;
            bgle[0].texture.sampleType = WGPUTextureSampleType_Float; bgle[0].texture.viewDimension = WGPUTextureViewDimension_2D;
            bgle[1].binding = 1; bgle[1].visibility = WGPUShaderStage_Fragment; bgle[1].sampler.type = WGPUSamplerBindingType_Filtering;
            WGPUBindGroupLayoutDescriptor bgld = {}; bgld.entryCount = 2; bgld.entries = bgle;
            m_materialLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bgld);
        }
        {
            WGPUBindGroupLayoutEntry bgle[3] = {};
            bgle[0].binding = 0; bgle[0].visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
            bgle[0].buffer.type = WGPUBufferBindingType_Uniform; bgle[0].buffer.minBindingSize = sizeof(SceneUniforms);
            bgle[1].binding = 1; bgle[1].visibility = WGPUShaderStage_Fragment;
            bgle[1].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat; bgle[1].texture.viewDimension = WGPUTextureViewDimension_2D;
            bgle[2].binding = 2; bgle[2].visibility = WGPUShaderStage_Fragment;
            bgle[2].texture.sampleType = WGPUTextureSampleType_Depth; bgle[2].texture.viewDimension = WGPUTextureViewDimension_2D;
            WGPUBindGroupLayoutDescriptor bgld = {}; bgld.entryCount = 3; bgld.entries = bgle;
            m_ssaoLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bgld);
        }
    }

    void PipelineManager::BuildPipelines() {
        {
            WGPUShaderModule shader = CreateShaderModule(Shaders::kShadowWGSL);
            WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneLayout;
            WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);
            std::vector<WGPUVertexAttribute> attrs; WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);
            WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth32Float; ds.depthWriteEnabled = WGPUOptionalBool_True; ds.depthCompare = WGPUCompareFunction_Less;
            WGPURenderPipelineDescriptor pd = {}; pd.layout = layout; pd.vertex.module = shader; pd.vertex.entryPoint = WGPUStringView{"vs_main", 7}; pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl; pd.primitive.topology = WGPUPrimitiveTopology_TriangleList; pd.primitive.cullMode = WGPUCullMode_Front; pd.primitive.frontFace = WGPUFrontFace_CCW; pd.depthStencil = &ds; pd.multisample.count = 1; pd.multisample.mask = ~0u;
            m_shadowPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);
            wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(layout);
        }
        {
            WGPUShaderModule shader = CreateShaderModule(Shaders::kMainWGSL);
            WGPUBindGroupLayout layouts[] = { m_sceneLayout, m_shadowLayout, m_materialLayout };
            WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 3; pld.bindGroupLayouts = layouts;
            WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &pld);
            std::vector<WGPUVertexAttribute> attrs; WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);
            WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth24PlusStencil8; ds.depthWriteEnabled = WGPUOptionalBool_True; ds.depthCompare = WGPUCompareFunction_Less;
            WGPUBlendState opaqueBlend = {}; opaqueBlend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_Zero }; opaqueBlend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_Zero };
            WGPUColorTargetState opaqueTarget = {}; opaqueTarget.format = m_surfaceFormat; opaqueTarget.blend = &opaqueBlend; opaqueTarget.writeMask = WGPUColorWriteMask_All;
            WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{"fs_opaque", 9}; frag.targetCount = 1; frag.targets = &opaqueTarget;
            WGPURenderPipelineDescriptor pd = {}; pd.layout = pipelineLayout; pd.vertex.module = shader; pd.vertex.entryPoint = WGPUStringView{"vs_main", 7}; pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl; pd.primitive.topology = WGPUPrimitiveTopology_TriangleList; pd.primitive.cullMode = WGPUCullMode_Back; pd.primitive.frontFace = WGPUFrontFace_CCW; pd.depthStencil = &ds; pd.fragment = &frag; pd.multisample.count = 1; pd.multisample.mask = ~0u;
            m_mainPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);
            WGPUBlendState alphaBlend = {}; alphaBlend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha }; alphaBlend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha };
            WGPUColorTargetState transTarget = opaqueTarget; transTarget.blend = &alphaBlend;
            WGPUFragmentState transFrag = frag; transFrag.entryPoint = WGPUStringView{"fs_transparent", 14}; transFrag.targets = &transTarget;
            WGPUDepthStencilState transDs = ds; transDs.depthWriteEnabled = WGPUOptionalBool_False;
            WGPURenderPipelineDescriptor tpd = pd; tpd.fragment = &transFrag; tpd.depthStencil = &transDs;
            m_transparentPipeline = wgpuDeviceCreateRenderPipeline(m_device, &tpd);
            wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(pipelineLayout);
        }
        {
            WGPUShaderModule solidShader = CreateShaderModule(Shaders::kHighlightSolidWGSL);
            WGPUShaderModule outlineShader = CreateShaderModule(Shaders::kHighlightOutlineWGSL);
            WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneLayout;
            WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);
            std::vector<WGPUVertexAttribute> attrs; WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);
            WGPUBlendState blend = {}; blend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha }; blend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha };
            WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.blend = &blend; target.writeMask = WGPUColorWriteMask_All;
            WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth24PlusStencil8; ds.depthCompare = WGPUCompareFunction_Always; ds.depthWriteEnabled = WGPUOptionalBool_False;
            WGPURenderPipelineDescriptor pd = {}; pd.layout = layout; pd.vertex.entryPoint = WGPUStringView{"vs_main", 7}; pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl; pd.primitive.frontFace = WGPUFrontFace_CCW; pd.depthStencil = &ds; pd.multisample.count = 1; pd.multisample.mask = ~0u;
            WGPUFragmentState fragSolid = {}; fragSolid.module = solidShader; fragSolid.entryPoint = WGPUStringView{"fs_main", 7}; fragSolid.targetCount = 1; fragSolid.targets = &target;
            pd.vertex.module = solidShader; pd.fragment = &fragSolid; pd.primitive.topology = WGPUPrimitiveTopology_TriangleList; pd.primitive.cullMode = WGPUCullMode_Back;
            m_highlightSolidPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);
            WGPUFragmentState fragOutline = {}; fragOutline.module = outlineShader; fragOutline.entryPoint = WGPUStringView{"fs_main", 7}; fragOutline.targetCount = 1; fragOutline.targets = &target;
            pd.vertex.module = outlineShader; pd.fragment = &fragOutline; pd.primitive.topology = WGPUPrimitiveTopology_LineList; pd.primitive.cullMode = WGPUCullMode_None;
            m_highlightOutlinePipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);
            wgpuShaderModuleRelease(solidShader); wgpuShaderModuleRelease(outlineShader); wgpuPipelineLayoutRelease(layout);
        }
        {
            WGPUShaderModule shader = CreateShaderModule(Shaders::kAABBWGSL);
            WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneLayout;
            WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);
            std::vector<WGPUVertexAttribute> attrs; WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);
            WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.writeMask = WGPUColorWriteMask_All;
            WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{"fs_main", 7}; frag.targetCount = 1; frag.targets = &target;
            WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth24PlusStencil8; ds.depthCompare = WGPUCompareFunction_LessEqual; ds.depthWriteEnabled = WGPUOptionalBool_False;
            WGPURenderPipelineDescriptor pd = {}; pd.layout = layout; pd.vertex.module = shader; pd.vertex.entryPoint = WGPUStringView{"vs_main", 7}; pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl; pd.primitive.topology = WGPUPrimitiveTopology_LineList; pd.depthStencil = &ds; pd.fragment = &frag; pd.multisample.count = 1; pd.multisample.mask = ~0u;
            m_aabbPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);
            wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(layout);
        }
        {
            WGPUShaderModule shader = CreateShaderModule(Shaders::kGlassWGSL);
            WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneLayout;
            WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);
            std::vector<WGPUVertexAttribute> attrs; WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);
            WGPUBlendState blend = {}; blend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha }; blend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha };
            WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.blend = &blend; target.writeMask = WGPUColorWriteMask_All;
            WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth24PlusStencil8; ds.depthCompare = WGPUCompareFunction_Less; ds.depthWriteEnabled = WGPUOptionalBool_False;
            WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{"fs_main", 7}; frag.targetCount = 1; frag.targets = &target;
            WGPURenderPipelineDescriptor pd = {}; pd.layout = layout; pd.vertex.module = shader; pd.vertex.entryPoint = WGPUStringView{"vs_main", 7}; pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl; pd.primitive.topology = WGPUPrimitiveTopology_TriangleList; pd.primitive.cullMode = WGPUCullMode_None; pd.depthStencil = &ds; pd.fragment = &frag; pd.multisample.count = 1; pd.multisample.mask = ~0u;
            m_glassPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);
            wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(layout);
        }
        {
            WGPUShaderModule maskShader = CreateShaderModule(Shaders::kMaskWGSL);
            WGPUShaderModule capShader  = CreateShaderModule(Shaders::kCapWGSL);
            WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneLayout;
            WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);
            std::vector<WGPUVertexAttribute> attrs; WGPUVertexBufferLayout vbl = MakeVertexBufferLayout(attrs);
            WGPUDepthStencilState maskDs = {}; maskDs.format = WGPUTextureFormat_Depth24PlusStencil8; maskDs.depthWriteEnabled = WGPUOptionalBool_False; maskDs.depthCompare = WGPUCompareFunction_Always; 
            WGPUStencilFaceState frontFace = {}; frontFace.compare = WGPUCompareFunction_Always; frontFace.failOp = WGPUStencilOperation_Keep; frontFace.depthFailOp = WGPUStencilOperation_Keep; frontFace.passOp = WGPUStencilOperation_IncrementWrap; 
            WGPUStencilFaceState backFace = {}; backFace.compare = WGPUCompareFunction_Always; backFace.failOp = WGPUStencilOperation_Keep; backFace.depthFailOp = WGPUStencilOperation_Keep; backFace.passOp = WGPUStencilOperation_DecrementWrap; 
            maskDs.stencilFront = frontFace; maskDs.stencilBack = backFace; maskDs.stencilReadMask = 0xFF; maskDs.stencilWriteMask = 0xFF;
            WGPUColorTargetState dummyTarget = {}; dummyTarget.format = m_surfaceFormat; dummyTarget.writeMask = WGPUColorWriteMask_None;
            WGPUFragmentState maskFrag = {}; maskFrag.module = maskShader; maskFrag.entryPoint = WGPUStringView{"fs_main", 7}; maskFrag.targetCount = 1; maskFrag.targets = &dummyTarget;
            WGPURenderPipelineDescriptor pdMask = {}; pdMask.layout = layout; pdMask.vertex.module = maskShader; pdMask.vertex.entryPoint = WGPUStringView{"vs_main", 7}; pdMask.vertex.bufferCount = 1; pdMask.vertex.buffers = &vbl; pdMask.primitive.topology = WGPUPrimitiveTopology_TriangleList; pdMask.primitive.cullMode = WGPUCullMode_None; pdMask.primitive.frontFace = WGPUFrontFace_CCW; pdMask.depthStencil = &maskDs; pdMask.fragment = &maskFrag; pdMask.multisample.count = 1; pdMask.multisample.mask = ~0u;
            m_stencilMaskPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pdMask);
            WGPUDepthStencilState capDs = {}; capDs.format = WGPUTextureFormat_Depth24PlusStencil8; capDs.depthWriteEnabled = WGPUOptionalBool_True; capDs.depthCompare = WGPUCompareFunction_LessEqual;
            WGPUStencilFaceState capFace = {}; capFace.compare = WGPUCompareFunction_NotEqual; capFace.failOp = WGPUStencilOperation_Keep; capFace.depthFailOp = WGPUStencilOperation_Keep; capFace.passOp = WGPUStencilOperation_Zero; 
            capDs.stencilFront = capFace; capDs.stencilBack = capFace; capDs.stencilReadMask = 0xFF; capDs.stencilWriteMask = 0xFF;
            WGPUColorTargetState capTarget = {}; capTarget.format = m_surfaceFormat; capTarget.writeMask = WGPUColorWriteMask_All;
            WGPUFragmentState capFrag = {}; capFrag.module = capShader; capFrag.entryPoint = WGPUStringView{"fs_main", 7}; capFrag.targetCount = 1; capFrag.targets = &capTarget;
            WGPURenderPipelineDescriptor pdCap = pdMask; pdCap.vertex.module = capShader; pdCap.fragment = &capFrag; pdCap.depthStencil = &capDs; pdCap.primitive.cullMode = WGPUCullMode_None;
            m_capPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pdCap);
            wgpuShaderModuleRelease(maskShader); wgpuShaderModuleRelease(capShader); wgpuPipelineLayoutRelease(layout);
        }
        {
            WGPUShaderModule shader = CreateShaderModule(Shaders::kGridWGSL);
            WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_sceneLayout;
            WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);
            WGPUBlendState blend = {}; blend.color = { WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha }; blend.alpha = { WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha };
            WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.blend = &blend; target.writeMask = WGPUColorWriteMask_All;
            WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{"fs_main", 7}; frag.targetCount = 1; frag.targets = &target;
            WGPUDepthStencilState ds = {}; ds.format = WGPUTextureFormat_Depth24PlusStencil8; ds.depthCompare = WGPUCompareFunction_Less; ds.depthWriteEnabled = WGPUOptionalBool_False;
            WGPURenderPipelineDescriptor pd = {}; pd.layout = layout; pd.vertex.module = shader; pd.vertex.entryPoint = WGPUStringView{"vs_main", 7}; pd.primitive.topology = WGPUPrimitiveTopology_TriangleList; pd.depthStencil = &ds; pd.fragment = &frag; pd.multisample.count = 1; pd.multisample.mask = ~0u;
            m_gridPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);
            wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(layout);
        }
        {
            WGPUShaderModule shader = CreateShaderModule(Shaders::kSSAOWGSL);
            WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &m_ssaoLayout;
            WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pld);
            WGPUColorTargetState target = {}; target.format = m_surfaceFormat; target.writeMask = WGPUColorWriteMask_All;
            WGPUFragmentState frag = {}; frag.module = shader; frag.entryPoint = WGPUStringView{"fs_main", 7}; frag.targetCount = 1; frag.targets = &target;
            WGPURenderPipelineDescriptor pd = {}; pd.layout = layout; pd.vertex.module = shader; pd.vertex.entryPoint = WGPUStringView{"vs_main", 7}; pd.primitive.topology = WGPUPrimitiveTopology_TriangleList; pd.fragment = &frag; pd.multisample.count = 1; pd.multisample.mask = ~0u;
            m_ssaoPipeline = wgpuDeviceCreateRenderPipeline(m_device, &pd);
            wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(layout);
        }
    }

} // namespace BimCore