#include "Resources.h"

#include <cstring>
#include <utility>

#include "Context.h"
#include "Immediate.h"

namespace pt::vk
{
// --------------- BUFFER --------------
Buffer::Buffer(const Context& ctx, VkDeviceSize size, VkBufferUsageFlags usage, MemoryUsage memory, const char* debugName)
	: m_allocator(ctx.allocator), m_size(size)
{
	VkBufferCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	info.size = size;
	info.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc{};
	alloc.usage = VMA_MEMORY_USAGE_AUTO;
	switch (memory)
	{
	case MemoryUsage::GpuOnly:
		alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case MemoryUsage::Upload:
		alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		break;
	case MemoryUsage::Readback:
		alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		break;
	}

	VmaAllocationInfo allocInfo{};
	VK_CHECK(vmaCreateBuffer(m_allocator, &info, &alloc, &m_buffer, &m_allocation, &allocInfo));
	m_mapped = allocInfo.pMappedData;

	VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	addrInfo.buffer = m_buffer;
	m_address = vkGetBufferDeviceAddress(ctx.device, &addrInfo);

	if (debugName) ctx.setName(m_buffer, VK_OBJECT_TYPE_BUFFER, debugName);
}

Buffer& Buffer::operator=(Buffer&& o) noexcept
{
	if (this != &o)
	{
		destroy();
		m_allocator = std::exchange(o.m_allocator, VK_NULL_HANDLE);
		m_buffer = std::exchange(o.m_buffer, VK_NULL_HANDLE);
		m_allocation = std::exchange(o.m_allocation, VK_NULL_HANDLE);
		m_address = std::exchange(o.m_address, 0);
		m_size = std::exchange(o.m_size, 0);
		m_mapped = std::exchange(o.m_mapped, nullptr);
	}
	return *this;
}

void Buffer::destroy()
{
	if (m_buffer) vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
	m_buffer = VK_NULL_HANDLE;
	m_allocation = VK_NULL_HANDLE;
	m_mapped = nullptr;
}

void Buffer::upload(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
	PT_ASSERT(m_mapped != nullptr, "Buffer::upload needs MemoryUsage::Upload (use a staging copy for GPUOnly)");
	PT_ASSERT(offset + size <= m_size, "Buffer::upload out of range");
	std::memcpy(static_cast<u8*>(m_mapped) + offset, data, size);
	vmaFlushAllocation(m_allocator, m_allocation, offset, size);
}


// -------------IMAGE2D------------
Image2D::Image2D(const Context& ctx, Extent2D extent, VkFormat format, VkImageUsageFlags usage, const char* debugName)
	: m_device(ctx.device), m_allocator(ctx.allocator), m_format(format), m_extent(extent)
{
	VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = { extent.width, extent.height, 1 };
	info.mipLevels = m_mipLevels;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usage;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo alloc{};
	alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	VK_CHECK(vmaCreateImage(m_allocator, &info, &alloc, &m_image, &m_allocation, nullptr));

	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = m_image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_view));

	if (debugName)
	{
		ctx.setName(m_image, VK_OBJECT_TYPE_IMAGE, debugName);
		ctx.setName(m_view, VK_OBJECT_TYPE_IMAGE_VIEW, debugName);
	}
}

Image2D& Image2D::operator=(Image2D&& o) noexcept
{
	if (this != &o)
	{
		destroy();
		m_device = std::exchange(o.m_device, VK_NULL_HANDLE);
		m_allocator = std::exchange(o.m_allocator, VK_NULL_HANDLE);
		m_image = std::exchange(o.m_image, VK_NULL_HANDLE);
		m_view = std::exchange(o.m_view, VK_NULL_HANDLE);
		m_allocation = std::exchange(o.m_allocation, VK_NULL_HANDLE);
		m_format = o.m_format;
		m_extent = o.m_extent;
		m_state = o.m_state;
	}
	return *this;
}

void Image2D::destroy()
{
	if (m_view) vkDestroyImageView(m_device, m_view, nullptr);
	if (m_image) vmaDestroyImage(m_allocator, m_image, m_allocation);
	m_view = VK_NULL_HANDLE;
	m_image = VK_NULL_HANDLE;
	m_allocation = VK_NULL_HANDLE;
	m_state = {};
}

void Image2D::transition(VkCommandBuffer cmd, const ImageState& next)
{
	imageBarrier(cmd, m_image, m_state, next);
	m_state = next;
}

void Image2D::generateMipmaps(VkCommandBuffer& commandBuffer) {
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = handle();
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;
	
	int32_t mipWidth = m_extent.width;
	int32_t mipHeight = m_extent.height;
	
	for (uint32_t i = 1; i < m_mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	
		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
	
		vkCmdBlitImage(commandBuffer,
			handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);
	
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}
	
	barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
}

void Image2D::uploadFromPixelData(const Context& ctx, void* data) {
	size_t dataSize = m_extent.width * m_extent.height * 4;
	pt::vk::Buffer uploadBuffer = pt::vk::Buffer(ctx, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, pt::vk::MemoryUsage::Upload);
	memcpy(uploadBuffer.mapped(), data, dataSize);
	
	constexpr pt::vk::ImageState kDstOptimal{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT };
	
	auto uploadFunction = [&](VkCommandBuffer cmd) {
		transition(cmd, kDstOptimal);
	
		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
	
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;

		VkExtent3D ext{
			.width = m_extent.width,
			.height = m_extent.height,
			.depth = 1
		};
		copyRegion.imageExtent = ext;
	
		vkCmdCopyBufferToImage(cmd, uploadBuffer.handle(), handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		generateMipmaps(cmd);
	};
	
	submitImmediate(ctx, uploadFunction);
	
	uploadBuffer.destroy();
}

//-------------BARRIERS------------
void imageBarrier(VkCommandBuffer cmd, VkImage image, const ImageState& from, const ImageState& to, VkImageAspectFlags aspect)
{
	VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	b.srcStageMask = from.stage == VK_PIPELINE_STAGE_2_NONE ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : from.stage;
	b.srcAccessMask = from.access;
	b.dstStageMask = to.stage;
	b.dstAccessMask = to.access;
	b.oldLayout = from.layout;
	b.newLayout = to.layout;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = image;
	b.subresourceRange = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dep.imageMemoryBarrierCount = 1;
	dep.pImageMemoryBarriers = &b;
	vkCmdPipelineBarrier2(cmd, &dep);
}

void memoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
{
	VkMemoryBarrier2 b{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	b.srcStageMask = srcStage;
	b.srcAccessMask = srcAccess;
	b.dstStageMask = dstStage;
	b.dstAccessMask = dstAccess;
	VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dep.memoryBarrierCount = 1;
	dep.pMemoryBarriers = &b;
	vkCmdPipelineBarrier2(cmd, &dep);
}
}