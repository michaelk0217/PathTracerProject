#pragma once

#include <functional>
#include <vector>

#include "Common.h"
#include "Context.h"

namespace pt::vk
{
class Sampler : NonCopyable {
public:
    struct SamplerInfo {
        VkSamplerAddressMode addressMode;
        u32 mipLevels;
        float maxAniso;
    };
    
    void createSampler(const Context& ctx, const SamplerInfo& s);

    void destroy() {
        vkDestroySampler(m_device, m_sampler, nullptr);
    }

    Sampler() = default;

    // move constructor for moving in GpuScene
    Sampler(Sampler&& other) noexcept
        : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE)) {
    }

    // move assignment operator (rule of 5)
    Sampler& operator=(Sampler&& other) noexcept {
        if (this != &other) {
            destroy();
            m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
            m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~Sampler() {
        destroy();
        m_sampler = VK_NULL_HANDLE;
    };

    VkSampler getSampler() {
        return m_sampler;
    }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};
};