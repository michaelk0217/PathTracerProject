#pragma once

#include <functional>
#include <vector>

#include "Common.h"

namespace pt::vk
{

    struct DeviceCapabilities
    {
        bool rayTracingMaintenance1 = false; // VK_KHR_ray_tracing_maintenance1
        bool rayTracingPositionFetch = false; // VK_KHR_ray_tracing_position_fetch
        bool shaderExecutionReorder = false; // VK_EXT_/NV_ray_tracing_invocation_reorder (extension present)
        bool memoryBudget = false; // VK_EXT_memory_budget
        bool debugUtils = false; // VK_EXT_debug_utils
        float maxAnisotropy = 1.f;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipeline{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
        VkPhysicalDeviceAccelerationStructurePropertiesKHR accelStruct{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
        VkPhysicalDeviceProperties properties{};
    };

    struct ContextDesc
    {
        const char* appName = "PathTracer";
        bool enableValidation = true;
        std::vector<const char*> instanceExtensions;
        std::function<VkSurfaceKHR(VkInstance)> createSurface;// keeps vk/ independent of SDL
    };

    // Owns vulkan instance, device, queue, surface, and vma allocator
    // base req: vulkan 1.3 + KHR ray tracing extension (pipeline + ray query)
    class Context : NonCopyable
    {
    public:
        explicit Context(const ContextDesc& desc);
        ~Context();

        void waitIdle() const { vkDeviceWaitIdle(device); }

        // Names show up in validation messages, RenderDoc and Nsight.
        void setName(VkObjectType type, u64 handle, const char* name) const;
        template <class T>
        void setName(T handle, VkObjectType type, const char* name) const {
            setName(type, reinterpret_cast<u64>(handle), name);
        }

        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE; // graphics + compute + present. Maybe I'll add separate queues later.
        u32 queueFamily = ~0u;
        VmaAllocator allocator = VK_NULL_HANDLE;
        DeviceCapabilities caps{};

    private:
        void createInstance(const ContextDesc& desc);
        void pickPhysicalDevice();
        void createDevice();
        void createAllocator();

        VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
        std::vector<const char*> m_deviceExtensions;
    };
}