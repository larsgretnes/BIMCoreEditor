// =============================================================================
// BimCore/graphics/PipelineManager.h
// =============================================================================
#pragma once

#include <webgpu/webgpu.h>
#include <string>
#include <vector>

namespace BimCore {

    class PipelineManager {
    public:
        PipelineManager(WGPUDevice device, WGPUTextureFormat surfaceFormat);
        ~PipelineManager();

        WGPUBindGroupLayout GetSceneLayout() const { return m_sceneLayout; }
        WGPUBindGroupLayout GetShadowLayout() const { return m_shadowLayout; }
        WGPUBindGroupLayout GetMaterialLayout() const { return m_materialLayout; }
        WGPUBindGroupLayout GetSSAOLayout() const { return m_ssaoLayout; }

        WGPURenderPipeline GetShadowPipeline() const { return m_shadowPipeline; }
        WGPURenderPipeline GetMainPipeline() const { return m_mainPipeline; }
        WGPURenderPipeline GetTransparentPipeline() const { return m_transparentPipeline; }
        WGPURenderPipeline GetHighlightSolidPipeline() const { return m_highlightSolidPipeline; }
        WGPURenderPipeline GetHighlightOutlinePipeline() const { return m_highlightOutlinePipeline; }
        WGPURenderPipeline GetAABBPipeline() const { return m_aabbPipeline; }
        WGPURenderPipeline GetGlassPipeline() const { return m_glassPipeline; }
        WGPURenderPipeline GetStencilMaskPipeline() const { return m_stencilMaskPipeline; }
        WGPURenderPipeline GetCapPipeline() const { return m_capPipeline; }
        WGPURenderPipeline GetGridPipeline() const { return m_gridPipeline; }
        WGPURenderPipeline GetSSAOPipeline() const { return m_ssaoPipeline; }

    private:
        WGPUShaderModule CreateShaderModule(const std::string& source) const;
        void BuildLayouts();
        void BuildPipelines();

    private:
        WGPUDevice m_device;
        WGPUTextureFormat m_surfaceFormat;

        WGPUBindGroupLayout m_sceneLayout = nullptr;
        WGPUBindGroupLayout m_shadowLayout = nullptr;
        WGPUBindGroupLayout m_materialLayout = nullptr;
        WGPUBindGroupLayout m_ssaoLayout = nullptr;

        WGPURenderPipeline m_shadowPipeline = nullptr;
        WGPURenderPipeline m_mainPipeline = nullptr;
        WGPURenderPipeline m_transparentPipeline = nullptr;
        WGPURenderPipeline m_highlightSolidPipeline = nullptr;
        WGPURenderPipeline m_highlightOutlinePipeline = nullptr;
        WGPURenderPipeline m_aabbPipeline = nullptr;
        WGPURenderPipeline m_glassPipeline = nullptr;
        WGPURenderPipeline m_stencilMaskPipeline = nullptr;
        WGPURenderPipeline m_capPipeline = nullptr;
        WGPURenderPipeline m_gridPipeline = nullptr;
        WGPURenderPipeline m_ssaoPipeline = nullptr;
    };

} // namespace BimCore