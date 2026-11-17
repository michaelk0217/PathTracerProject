#include "Sampler.h"

namespace pt::vk
{
	void Sampler::createSampler(const pt::vk::Context& ctx, const Sampler::SamplerInfo& s) {
		VkSamplerCreateInfo samplerCInfo{};
		samplerCInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCInfo.magFilter = VK_FILTER_LINEAR;
		samplerCInfo.minFilter = VK_FILTER_LINEAR;
		samplerCInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCInfo.addressModeU = s.addressMode;
		samplerCInfo.addressModeV = s.addressMode;
		samplerCInfo.addressModeW = s.addressMode;
		samplerCInfo.mipLodBias = 0.0f;
		samplerCInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCInfo.minLod = 0.0f;
		samplerCInfo.maxLod = (float)s.mipLevels;
		samplerCInfo.anisotropyEnable = VK_TRUE;
		samplerCInfo.maxAnisotropy = s.maxAniso;

		vkCreateSampler(ctx.device, &samplerCInfo, nullptr, &m_sampler);

		m_device = ctx.device;
	}
};