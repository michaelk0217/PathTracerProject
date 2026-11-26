#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <functional>

#include <vk/Common.h>
#include <vk/Resources.h>
#include <vk/Bindless.h>
#include <vk/Pipeline.h>
#include <renderer/RenderSettings.h>

namespace pt
{
namespace vk { class Context; class Swapchain; }
class Scene;
class GpuScene;

// Owns the swapchain and per-frame resources, and runs the frame loop:
//   wait for frame slot -> acquire image -> record -> submit -> present
class Renderer : NonCopyable
{
public:
    Renderer(vk::Context& ctx, Extent2D extent, std::filesystem::path shaderDir, bool vsync = true);
    ~Renderer();

    using OverlayFn = std::function<void(VkCommandBuffer, VkImageView, Extent2D)>;

    bool renderFrame(Scene& scene, const RenderSettings& settings, const OverlayFn& overlay = {});
    VkFormat swapchainFormat() const;
    u32 swapchainImageCount() const;
    void uploadImage(u32 slot, VkImageView view, VkSampler sampler);
    void resize(Extent2D extent);

    void resetAccumulation() { m_resetAccumulation = true; }
    u32 accumulatedFrames() const { return m_accumulatedFrames; }

    u32 lightCount() const;

private:
    // one per frame in flight.
    struct FrameData
    {
        VkCommandPool     pool = VK_NULL_HANDLE;
        VkCommandBuffer   cmd = VK_NULL_HANDLE;
        VkSemaphore       imageAvailable = VK_NULL_HANDLE;
        u64               timelineValue = 0;
        vk::Buffer uniforms; // gpu::FrameUniforms, one per slot
    };

    // test fnc for recording clear screen to color
    void recordClear(VkCommandBuffer cmd, u32 imageIndex, const vec4& color);
        
    void createTargets(Extent2D extent);
    VkPipeline loadCompute(const char* spvPath, const char* entryPoint);
    void writeFrameUniforms(FrameData& f, Scene& scene, Extent2D extent, const RenderSettings& settings);
    void recordBlitToSwapchain(VkCommandBuffer cmd, u32 imageIndex);

    vk::Context& m_ctx;
    std::filesystem::path m_shaderDir;
    std::unique_ptr<vk::Swapchain> m_swapchain;
    std::unique_ptr<vk::BindlessDescriptors> m_bindless;
    std::unique_ptr<GpuScene> m_gpuScene;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE; // shared by ALL pipelines

    VkPipeline m_tonemapPipeline = VK_NULL_HANDLE;

    vk::RayTracingPipeline m_pathTracer; // pathtrace_mc.spv
    vk::RayTracingPipeline loadRayTracing(const char* spvPath);

    vk::Image2D m_accumulation; // RGBA32F linear HDR (gImages[kSlotAccumulation])
    vk::Image2D m_display; // RGBA8 sRGB-encoded (gImages[kSlotDisplay])

    std::array<FrameData, vk::kMaxFramesInFlight> m_frames;
    VkSemaphore m_timeline = VK_NULL_HANDLE;
    u64         m_timelineValue = 0;
    u64         m_frameNumber = 0;
    u32         m_accumulatedFrames = 0;
    bool        m_resetAccumulation = false;
    mat4        m_lastViewInverse{ 0.0f };
    RenderSettings m_lastSettings{};
};
}