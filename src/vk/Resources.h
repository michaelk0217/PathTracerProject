#pragma once

#include "Common.h"

namespace pt::vk
{

class Context;

// ---------------------------------------------------------------------------
// Buffer: VMA-backed, always created with SHADER_DEVICE_ADDRESS so shaders can
// reach it through a pointer (push constants / GpuScene tables).
// ---------------------------------------------------------------------------
enum class MemoryUsage
{
	GpuOnly,	// device local; fill via staging or GPU writes
	Upload,		// host-visible, persistently mapped, sequential write (per-frame constants)
	Readback,	// host-visible, cached (GPU -> CPU)
};

class Buffer : NonCopyable
{
public:
	Buffer() = default;
	Buffer(const Context& ctx, VkDeviceSize size, VkBufferUsageFlags usage, MemoryUsage memory, const char* debugName = nullptr);
	~Buffer() { destroy(); }
	Buffer(Buffer&& o) noexcept { *this = std::move(o); }
	Buffer& operator=(Buffer&& o) noexcept;

	void destroy();
	void upload(const void* data, VkDeviceSize size, VkDeviceSize offset = 0); // upload memory only

	VkBuffer handle() const { return m_buffer; }
	VkDeviceAddress address() const { return m_address; }
	VkDeviceSize size() const { return m_size; }
	void* mapped() const { return m_mapped; }
	explicit operator bool() const { return m_buffer != VK_NULL_HANDLE; }

private:
	VmaAllocator m_allocator = VK_NULL_HANDLE;
	VkBuffer m_buffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;
	VkDeviceAddress m_address = 0;
	VkDeviceSize m_size = 0;
	void* m_mapped = nullptr;
};


// ---------------------------------------------------------------------------
// Image2D: image + default view + *tracked* layout/stage/access, so transitions
// only need the destination state. PassList builds on this.
// ---------------------------------------------------------------------------
struct ImageState {
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
	VkAccessFlags2 access = VK_ACCESS_2_NONE;
};

class Image2D : NonCopyable
{
public:
	Image2D() = default;
	Image2D(const Context& ctx, Extent2D extent, VkFormat format, VkImageUsageFlags usage, const char* debugName = nullptr);
	~Image2D() { destroy(); }
	Image2D(Image2D&& o) noexcept { *this = std::move(o); }
	Image2D& operator=(Image2D&& o) noexcept;

	void destroy();

	// Records sync2 barrier from traced state to "next"
	void transition(VkCommandBuffer cmd, const ImageState& next);

	void generateMipmaps(VkCommandBuffer& commandBuffer);
	void uploadFromPixelData(const Context& ctx, void* data);

	VkImage handle() const { return m_image; }
	VkImageView view() const { return m_view; }
	VkFormat format() const { return m_format; }
	Extent2D extent() const { return m_extent; }
	const ImageState& state() const { return m_state; }

private:
	VkDevice m_device = VK_NULL_HANDLE;
	VmaAllocator m_allocator = VK_NULL_HANDLE;
	VkImage m_image = VK_NULL_HANDLE;
	VkImageView m_view = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;
	VkFormat m_format = VK_FORMAT_UNDEFINED;
	Extent2D m_extent{};
	ImageState m_state{};
	u32 m_mipLevels = 1;
};

// Common destination states:
namespace states
{
inline constexpr ImageState ComputeWrite{	
	VK_IMAGE_LAYOUT_GENERAL,
	VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
	VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
inline constexpr ImageState TransferDst{
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
	VK_ACCESS_2_TRANSFER_WRITE_BIT};
inline constexpr ImageState TransferSrc{
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
	VK_ACCESS_2_TRANSFER_READ_BIT};
inline constexpr ImageState ShaderRead{
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
	VK_ACCESS_2_SHADER_SAMPLED_READ_BIT };
inline constexpr ImageState ColorAttachment{
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
inline constexpr ImageState Present{
	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	VK_PIPELINE_STAGE_2_NONE,
	VK_ACCESS_2_NONE};
} // namespace states

// Raw barrier for images not owned (going to be used for swapchain images)
void imageBarrier(VkCommandBuffer cmd, VkImage image, const ImageState& from, const ImageState& to, 
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

// Global memory barrier (for any buffers)
void memoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
	VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess);

} //namespace pt::vk