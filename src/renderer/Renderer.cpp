#include "Renderer.h"

#include <cmath>

#include <shared/GpuStructs.h>
#include <vk/Context.h>
#include <vk/Resources.h>
#include <vk/Swapchain.h>
#include <vk/Pipeline.h>
#include <scene/Scene.h>
#include <renderer/GpuScene.h>

namespace pt
{
namespace
{
constexpr vk::ImageState kComputeRead{ VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT };
}
namespace
{
bool integratorSettingsChanged(const RenderSettings& a, const RenderSettings& b)
{
	return a.maxBounces != b.maxBounces || a.lightSampleCount != b.lightSampleCount;
}
}
Renderer::Renderer(vk::Context& ctx, Extent2D extent, std::filesystem::path shaderDir, bool vsync)
	: m_ctx(ctx), m_shaderDir(shaderDir)
{
	m_swapchain = std::make_unique<vk::Swapchain>(ctx, extent, vsync);

	// global bindless descriptor set + one pipeline layout every pipeline uses (set 0 + pushConstants)
	m_bindless = std::make_unique<vk::BindlessDescriptors>(ctx);
	m_pipelineLayout = vk::createPipelineLayout(ctx, m_bindless->layout(), sizeof(gpu::PushConstants));
	m_gpuScene = std::make_unique<GpuScene>(ctx, *m_bindless.get());

	// timeline semaphore (64bit counter that gpu increments when work finishes)
	VkSemaphoreTypeCreateInfo timelineType{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	timelineType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timelineType.initialValue = 0;
	VkSemaphoreCreateInfo timelineInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	timelineInfo.pNext = &timelineType;
	VK_CHECK(vkCreateSemaphore(ctx.device, &timelineInfo, nullptr, &m_timeline));
	ctx.setName(m_timeline, VK_OBJECT_TYPE_SEMAPHORE, "FrameTimeline");


	// PER FRAME RESOURCES
	for (FrameData& f : m_frames)
	{
		VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // reset pool every frame
		poolInfo.queueFamilyIndex = ctx.queueFamily;
		VK_CHECK(vkCreateCommandPool(ctx.device, &poolInfo, nullptr, &f.pool));

		VkCommandBufferAllocateInfo alloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc.commandPool = f.pool;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc.commandBufferCount = 1;
		VK_CHECK(vkAllocateCommandBuffers(ctx.device, &alloc, &f.cmd));

		VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO }; // binary semaphore
		VK_CHECK(vkCreateSemaphore(ctx.device, &semInfo, nullptr, &f.imageAvailable));

		f.uniforms = vk::Buffer(ctx, sizeof(gpu::FrameUniforms), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vk::MemoryUsage::Upload, "FrameUniforms");
	}
	log::info("Renderer: swapchain {}x{}, {} images", 
		m_swapchain->extent().width, m_swapchain->extent().height, m_swapchain->imageCount());

	createTargets(m_swapchain->extent());
	
	m_tonemapPipeline = loadCompute("passes/tonemap.spv", "tonemap");
	m_pathTracer = loadRayTracing("integrators/pathtrace_mc.spv");

	PT_ASSERT(m_tonemapPipeline, "tonemap shaders missing: check if shader build succeeded");
	PT_ASSERT(m_pathTracer.pipeline, "pathtrace_mc.spv missing: check if shader was built");
}

Renderer::~Renderer()
{
	m_ctx.waitIdle();
	
	vkDestroyPipeline(m_ctx.device, m_tonemapPipeline, nullptr);
	vkDestroyPipeline(m_ctx.device, m_pathTracer.pipeline, nullptr);
	m_pathTracer.sbt.destroy();
	vkDestroyPipelineLayout(m_ctx.device, m_pipelineLayout, nullptr);

	for (FrameData& f : m_frames)
	{
		f.uniforms.destroy();
		vkDestroySemaphore(m_ctx.device, f.imageAvailable, nullptr);
		vkDestroyCommandPool(m_ctx.device, f.pool, nullptr);
	}

	vkDestroySemaphore(m_ctx.device, m_timeline, nullptr);
	m_accumulation.destroy();
	m_display.destroy();
	m_gpuScene.reset();
	m_bindless.reset();
	m_swapchain.reset();
}

void Renderer::createTargets(Extent2D extent)
{
	m_accumulation = vk::Image2D(m_ctx, extent, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "Accumulation");
	m_display = vk::Image2D(m_ctx, extent, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "Display");

	m_bindless->setStorageImage(gpu::kSlotAccumulation, m_accumulation.view());
	m_bindless->setStorageImage(gpu::kSlotDisplay, m_display.view());

	m_resetAccumulation = true;
}

void Renderer::resize(Extent2D extent)
{
	if (extent.isZero()) return;
	m_ctx.waitIdle();
	m_swapchain->recreate(extent);
	createTargets(m_swapchain->extent());
	log::info("Renderer: resized to {}x{}", m_swapchain->extent().width, m_swapchain->extent().height);
}

VkPipeline Renderer::loadCompute(const char* spvPath, const char* entryPoint)
{
	const auto spirv = vk::loadSpirv(m_shaderDir / spvPath);
	if (spirv.empty()) return VK_NULL_HANDLE;

	VkShaderModule module = vk::createShaderModule(m_ctx, spirv);
	VkPipeline pipeline = vk::createComputePipeline(m_ctx, m_pipelineLayout, module, entryPoint, entryPoint);
	vkDestroyShaderModule(m_ctx.device, module, nullptr);
	return pipeline;
}

vk::RayTracingPipeline Renderer::loadRayTracing(const char* spvPath)
{
	const auto spirv = vk::loadSpirv(m_shaderDir / spvPath);
	if (spirv.empty()) return {};

	vk::RayTracingPipelineDesc desc;
	desc.module = vk::createShaderModule(m_ctx, spirv);
	vk::RayTracingPipeline p = vk::createRayTracingPipeline(m_ctx, m_pipelineLayout, desc);
	vkDestroyShaderModule(m_ctx.device, desc.module, nullptr);

	return p;
}

void Renderer::writeFrameUniforms(FrameData& f, Scene& scene, Extent2D extent, const RenderSettings& settings)
{
	const float aspect = float(extent.width) / float(extent.height);
	const mat4 viewInverse = glm::inverse(scene.camera.view());

	if (viewInverse != m_lastViewInverse || integratorSettingsChanged(settings, m_lastSettings))
	{
		m_resetAccumulation = true;
	}
	m_lastViewInverse = viewInverse;
	m_lastSettings = settings;

	if (m_resetAccumulation)
	{
		m_accumulatedFrames = 0;
		m_resetAccumulation = false;
	}

	gpu::FrameUniforms u{};
	u.viewInverse = viewInverse;
	u.projInverse = glm::inverse(scene.camera.projection(aspect));
	u.cameraPosition = scene.camera.position;
	u.maxBounces = settings.maxBounces;
	u.exposure = settings.exposure;
	u.lightMaterial = scene.lightMaterial;
	u.lightPosition = scene.lightPosition;
	u.lightWidth = scene.lightWidth;
	u.directionToSun = scene.directionToSun;
	u.sunIrradiance = scene.sunIrradiance;
	u.lightSampleCount = settings.lightSampleCount;
	u.frameIndex = static_cast<u32>(m_frameNumber);
	u.accumulatedFrames = m_accumulatedFrames;
	u.skyRadiance = scene.skyRadiance;
	f.uniforms.upload(&u, sizeof(u));

	++m_accumulatedFrames;
}

void Renderer::uploadImage(u32 slot, VkImageView view, VkSampler sampler) {
	m_bindless.get()->setTexture(slot, view, sampler);
};

bool Renderer::renderFrame(Scene& scene, const RenderSettings& settings, const OverlayFn& overlay)
{
	if (m_gpuScene->sync(scene)) m_resetAccumulation = true;

	if (m_gpuScene->consumeTlasChanged()) m_bindless->setTlas(m_gpuScene->tlas());

	const u32 slot = static_cast<u32>(m_frameNumber % vk::kMaxFramesInFlight);
	FrameData& f = m_frames[slot];

	// wait until gpu finishes frame that last used this frame slot
	if (f.timelineValue > 0)
	{
		VkSemaphoreWaitInfo wait{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
		wait.semaphoreCount = 1;
		wait.pSemaphores = &m_timeline;
		wait.pValues = &f.timelineValue;
		VK_CHECK(vkWaitSemaphores(m_ctx.device, &wait, UINT64_MAX));
	}


	// acquire net swapchain image
	u32 imageIndex = 0;
	if (!m_swapchain->acquire(f.imageAvailable, imageIndex))
	{
		resize(m_swapchain->extent()); // window resized
		return false;
	}

	// begin command recording
	VK_CHECK(vkResetCommandPool(m_ctx.device, f.pool, 0));
	VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(f.cmd, &begin));
	//--------------------------------------------------------------------

	// // clear screen test
	//const float t = static_cast<float>(m_frameNumber) * 0.02f;
	//const vec4 color{ 0.5f + 0.5f * std::sin(t), 0.3, 0.5 + 0.5f * std::cos(t), 1.0f };
	//recordClear(f.cmd, imageIndex, color);

	

	const Extent2D e = m_swapchain->extent();

	writeFrameUniforms(f, scene, e, settings);

	gpu::PushConstants pc{};
	pc.frame = f.uniforms.address();
	pc.meshes = m_gpuScene->meshInfoAddress();
	pc.materials = m_gpuScene->materialAddress();
	pc.extent = { e.width, e.height };

	VkDescriptorSet set = m_bindless->set();
	vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &set, 0, nullptr);
	vkCmdPushConstants(f.cmd, m_pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(pc), &pc);
	// compute threads (8, 8, 1)
	const u32 gx = (e.width + 7) / 8;
	const u32 gy = (e.height + 7) / 8;

	// // test shader
	//m_accumulation.transition(f.cmd, vk::states::ComputeWrite);
	//vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_testPatternPipeline);
	//vkCmdDispatch(f.cmd, gx, gy, 1);

	m_accumulation.transition(f.cmd, vk::states::ComputeWrite);
	vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pathTracer.pipeline);
	vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineLayout, 0, 1, &set, 0, nullptr);
	vkCmdTraceRaysKHR(f.cmd, &m_pathTracer.rayGenRegion, &m_pathTracer.missRegion, &m_pathTracer.hitRegion,
		&m_pathTracer.callableRegion, e.width, e.height, 1);

	m_accumulation.transition(f.cmd, kComputeRead);
	m_display.transition(f.cmd, vk::states::ComputeWrite);
	vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_tonemapPipeline);
	vkCmdDispatch(f.cmd, gx, gy, 1);

	recordBlitToSwapchain(f.cmd, imageIndex); // display -> swapchain

	const VkImage swapImage = m_swapchain->image(imageIndex);
	if (overlay)
	{
		vk::imageBarrier(f.cmd, swapImage, vk::states::TransferDst, vk::states::ColorAttachment);
		overlay(f.cmd, m_swapchain->view(imageIndex), e);
		vk::imageBarrier(f.cmd, swapImage, vk::states::ColorAttachment, vk::states::Present);
	}
	else
	{
		vk::imageBarrier(f.cmd, swapImage, vk::states::TransferDst, vk::states::Present);
	}


	//--------------------------------------------------------------------
	VK_CHECK(vkEndCommandBuffer(f.cmd));

	// submit to queue
	f.timelineValue = ++m_timelineValue;
	
	VkSemaphoreSubmitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	waitInfo.semaphore = f.imageAvailable;
	waitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;

	VkSemaphoreSubmitInfo signalInfos[2]{};
	signalInfos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalInfos[0].semaphore = m_swapchain->renderFinished(imageIndex);
	signalInfos[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	signalInfos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalInfos[1].semaphore = m_timeline;
	signalInfos[1].value = f.timelineValue;
	signalInfos[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	cmdInfo.commandBuffer = f.cmd;
	
	VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submit.waitSemaphoreInfoCount = 1;
	submit.pWaitSemaphoreInfos = &waitInfo;
	submit.commandBufferInfoCount = 1;
	submit.pCommandBufferInfos = &cmdInfo;
	submit.signalSemaphoreInfoCount = 2;
	submit.pSignalSemaphoreInfos = signalInfos;
	VK_CHECK(vkQueueSubmit2(m_ctx.queue, 1, &submit, VK_NULL_HANDLE));

	// present (waits on renderFinished[imageIndex] inside swapchain::present
	const bool presented = m_swapchain->present(imageIndex);

	++m_frameNumber;
	if (!presented) resize(m_swapchain->extent());
	return presented;
}

VkFormat Renderer::swapchainFormat() const { return m_swapchain->format(); }

u32 Renderer::swapchainImageCount() const { return m_swapchain->imageCount(); }

void Renderer::recordBlitToSwapchain(VkCommandBuffer cmd, u32 imageIndex)
{
	m_display.transition(cmd, vk::states::TransferSrc);

	const VkImage dst = m_swapchain->image(imageIndex);
	constexpr vk::ImageState kAcquired{ VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_NONE };
	//constexpr vk::ImageState kPresent{ VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE };
	
	vk::imageBarrier(cmd, dst, kAcquired, vk::states::TransferDst);

	// blit: display is RGBA, swapchain is BGRA. convrting formats
	const Extent2D e = m_swapchain->extent();
	VkImageBlit2 region{ VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
	region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.srcOffsets[1] = { static_cast<i32>(e.width), static_cast<i32>(e.height), 1 };
	region.dstOffsets[1] = { static_cast<i32>(e.width), static_cast<i32>(e.height), 1 };

	VkBlitImageInfo2 blit{ VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
	blit.srcImage = m_display.handle();
	blit.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blit.dstImage = dst;
	blit.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blit.regionCount = 1;
	blit.pRegions = &region;
	blit.filter = VK_FILTER_NEAREST;
	vkCmdBlitImage2(cmd, &blit);

	//vk::imageBarrier(cmd, dst, vk::states::TransferDst, kPresent);

}

void Renderer::recordClear(VkCommandBuffer cmd, u32 imageIndex, const vec4& color)
{
	const VkImage image = m_swapchain->image(imageIndex);

	constexpr vk::ImageState kAcquired{ VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_NONE };
	vk::imageBarrier(cmd, image, kAcquired, vk::states::TransferDst);

	VkClearColorValue clear{};
	clear.float32[0] = color.r;
	clear.float32[1] = color.g;
	clear.float32[2] = color.b;
	clear.float32[3] = color.a;
	const VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);

	constexpr vk::ImageState kPresent{ VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE };
	vk::imageBarrier(cmd, image, vk::states::TransferDst, kPresent);
}

}