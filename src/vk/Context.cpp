#include "Context.h"

#include <algorithm>
#include <cstring>

namespace pt::vk
{
namespace
{
const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

const char* kRequiredDeviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	VK_KHR_RAY_QUERY_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
};

// Callback for Vulkan Validation Layer
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        log::error("[vk] {}", data->pMessage);
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        log::warn("[vk] {}", data->pMessage);
    }
    else {
        log::info("[vk] {}", data->pMessage);
    }
    return VK_FALSE;
}

bool hasExtension(const std::vector<VkExtensionProperties>& exts, const char* name)
{
    return std::any_of(exts.begin(), exts.end(), [&](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, name) == 0; });
}

std::vector<VkExtensionProperties> deviceExtensions(VkPhysicalDevice pd)
{
    u32 count = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, exts.data());
    return exts;
}
} // namespace

Context::Context(const ContextDesc& desc)
{
    VK_CHECK(volkInitialize());
    createInstance(desc);
    surface = desc.createSurface ? desc.createSurface(instance) : VK_NULL_HANDLE;
    pickPhysicalDevice();
    createDevice();
    createAllocator();
}

Context::~Context()
{
    if (device) waitIdle();
    if (allocator) vmaDestroyAllocator(allocator);
    if (device) vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (m_messenger) vkDestroyDebugUtilsMessengerEXT(instance, m_messenger, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
    volkFinalize();
}

void Context::createInstance(const ContextDesc& desc)
{
    PT_ASSERT(volkGetInstanceVersion() >= VK_API_VERSION_1_3, "Vulkan 1.3 loader required- update GPU driver");

    std::vector<const char*> extensions = desc.instanceExtensions;
    std::vector<const char*> layers;

    u32 extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, available.data());

    caps.debugUtils = hasExtension(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (caps.debugUtils) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (desc.enableValidation)
    {
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> layerProps(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());
        const bool found = std::any_of(layerProps.begin(), layerProps.end(), [](const VkLayerProperties& l) {
            return std::strcmp(l.layerName, kValidationLayer) == 0;
        });

        if (found)
        {
            layers.push_back(kValidationLayer);
        }
        else
        {
            log::warn("Validation layer not found - install Vulkan SDK");
        }
    }

    VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.pApplicationName = desc.appName;
    app.pEngineName = "PathTracer";
    app.apiVersion = VK_API_VERSION_1_3;

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    messengerInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messengerInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    messengerInfo.pfnUserCallback = debugCallback;

    VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<u32>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    info.enabledLayerCount = static_cast<u32>(layers.size());
    info.ppEnabledLayerNames = layers.data();
    if (caps.debugUtils && !layers.empty()) info.pNext = &messengerInfo;

    VK_CHECK(vkCreateInstance(&info, nullptr, &instance));
    volkLoadInstance(instance);

    if (caps.debugUtils && !layers.empty())
    {
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &messengerInfo, nullptr, &m_messenger));
    }
}

void Context::pickPhysicalDevice()
{
    u32 count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    PT_ASSERT(count > 0, "No Vulkan supported GPU found");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    int bestScore = -1;
    for (VkPhysicalDevice pd : devices)
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);
        caps.maxAnisotropy = props.limits.maxSamplerAnisotropy;
        if (props.apiVersion < VK_API_VERSION_1_3) continue;

        const auto exts = deviceExtensions(pd);
        bool hasAll = true;

        for (const char* req : kRequiredDeviceExtensions)
        {
            if (!hasExtension(exts, req))
            {
                hasAll = false;
                break;
            }
        }

        if (!hasAll) {
            log::info("Skipping {} (missing ray tracing extensions)", props.deviceName);
            continue;
        }

        // as mentioned in Context.h, we will have one queue family for all graphics, compute, and present for now.
        u32 qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, qfs.data());

        u32 family = ~0u;
        for (u32 i = 0; i < qfCount; ++i) {
            VkBool32 present = VK_TRUE;
            if (surface) vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present);
            const VkQueueFlags need = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if ((qfs[i].queueFlags & need) == need && present) {
                family = i;
                break;
            }
        }
        if (family == ~0u) continue;

        const int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 1000 : 100;
        if (score > bestScore) {
            bestScore = score;
            physicalDevice = pd;
            queueFamily = family;
        }
    }

    PT_ASSERT(physicalDevice != VK_NULL_HANDLE, "No GPU with Vulkan 1.3 + KHR ray tracing found");

    // Optional extensions
    const auto exts = deviceExtensions(physicalDevice);
    m_deviceExtensions.assign(std::begin(kRequiredDeviceExtensions), std::end(kRequiredDeviceExtensions));
    auto tryEnable = [&](const char* name) {
        if (!hasExtension(exts, name)) return false;
        m_deviceExtensions.push_back(name);
        return true;
        };
    caps.rayTracingMaintenance1 = tryEnable(VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME);
    caps.rayTracingPositionFetch = tryEnable(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME);
    caps.memoryBudget = tryEnable(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
#if defined(VK_EXT_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME)
    caps.shaderExecutionReorder = tryEnable(VK_EXT_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME);
#endif
    if (!caps.shaderExecutionReorder) {
        caps.shaderExecutionReorder = tryEnable(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME);
    }
    // TODO(SER): chain the matching *InvocationReorderFeatures struct in createDevice() when you use it.

    // Properties
    VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    props2.pNext = &caps.rtPipeline;
    caps.rtPipeline.pNext = &caps.accelStruct;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
    caps.properties = props2.properties;
    caps.rtPipeline.pNext = nullptr;

    log::info("GPU: {} (Vulkan {}.{}.{})", caps.properties.deviceName, VK_API_VERSION_MAJOR(caps.properties.apiVersion),
        VK_API_VERSION_MINOR(caps.properties.apiVersion), VK_API_VERSION_PATCH(caps.properties.apiVersion));
    log::info("  SER: {}, position fetch: {}, RT maintenance1: {}", caps.shaderExecutionReorder,
        caps.rayTracingPositionFetch, caps.rayTracingMaintenance1);
}

void Context::createDevice()
{
    // --- Query what is supported -----------------------------------------
    VkPhysicalDeviceVulkan11Features s11{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    VkPhysicalDeviceVulkan12Features s12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceVulkan13Features s13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    s11.pNext = &s12;
    s12.pNext = &s13;
    VkPhysicalDeviceFeatures2 supported{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    supported.pNext = &s11;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &supported);

    PT_ASSERT(s12.bufferDeviceAddress && s12.descriptorIndexing && s12.scalarBlockLayout && s12.timelineSemaphore,
        "Missing Vulkan 1.2 features");
    PT_ASSERT(s13.synchronization2 && s13.dynamicRendering, "Missing Vulkan 1.3 features");

    // --- Enable what we need ---------------------------------------------
    VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    features.features.shaderInt64 = supported.features.shaderInt64;
    features.features.shaderInt16 = supported.features.shaderInt16;
    features.features.samplerAnisotropy = supported.features.samplerAnisotropy;
    features.features.shaderStorageImageWriteWithoutFormat = supported.features.shaderStorageImageWriteWithoutFormat;
    features.features.shaderStorageImageReadWithoutFormat = supported.features.shaderStorageImageReadWithoutFormat;

    VkPhysicalDeviceVulkan11Features f11{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    f11.storageBuffer16BitAccess = s11.storageBuffer16BitAccess;
    f11.shaderDrawParameters = s11.shaderDrawParameters;

    VkPhysicalDeviceVulkan12Features f12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    f12.bufferDeviceAddress = VK_TRUE;
    f12.scalarBlockLayout = VK_TRUE;
    f12.timelineSemaphore = VK_TRUE;
    f12.hostQueryReset = s12.hostQueryReset;
    f12.shaderFloat16 = s12.shaderFloat16;
    f12.shaderInt8 = s12.shaderInt8;
    f12.storageBuffer8BitAccess = s12.storageBuffer8BitAccess;
    f12.vulkanMemoryModel = s12.vulkanMemoryModel;
    f12.vulkanMemoryModelDeviceScope = s12.vulkanMemoryModelDeviceScope;
    // Bindless
    f12.descriptorIndexing = VK_TRUE;
    f12.runtimeDescriptorArray = s12.runtimeDescriptorArray;
    f12.descriptorBindingPartiallyBound = s12.descriptorBindingPartiallyBound;
    f12.descriptorBindingVariableDescriptorCount = s12.descriptorBindingVariableDescriptorCount;
    f12.descriptorBindingSampledImageUpdateAfterBind = s12.descriptorBindingSampledImageUpdateAfterBind;
    f12.descriptorBindingStorageImageUpdateAfterBind = s12.descriptorBindingStorageImageUpdateAfterBind;
    f12.descriptorBindingUpdateUnusedWhilePending = s12.descriptorBindingUpdateUnusedWhilePending;
    f12.shaderSampledImageArrayNonUniformIndexing = s12.shaderSampledImageArrayNonUniformIndexing;
    f12.shaderStorageImageArrayNonUniformIndexing = s12.shaderStorageImageArrayNonUniformIndexing;
    f12.shaderStorageBufferArrayNonUniformIndexing = s12.shaderStorageBufferArrayNonUniformIndexing;

    VkPhysicalDeviceVulkan13Features f13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    f13.synchronization2 = VK_TRUE;
    f13.dynamicRendering = VK_TRUE;
    f13.maintenance4 = s13.maintenance4;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR fAS{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    fAS.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR fRT{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
    fRT.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceRayQueryFeaturesKHR fRQ{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    fRQ.rayQuery = VK_TRUE;

    VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR fRTM1{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR };
    fRTM1.rayTracingMaintenance1 = VK_TRUE;  // query rayTracingPipelineTraceRaysIndirect2 before enabling it

    VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR fPF{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR };
    fPF.rayTracingPositionFetch = VK_TRUE;

    // Chain: features -> 1.1 -> 1.2 -> 1.3 -> AS -> RT pipeline -> ray query [-> optional]
    features.pNext = &f11;
    f11.pNext = &f12;
    f12.pNext = &f13;
    f13.pNext = &fAS;
    fAS.pNext = &fRT;
    fRT.pNext = &fRQ;
    void** tail = &fRQ.pNext;
    if (caps.rayTracingMaintenance1) { *tail = &fRTM1; tail = &fRTM1.pNext; }
    if (caps.rayTracingPositionFetch) { *tail = &fPF; tail = &fPF.pNext; }

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    info.pNext = &features;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = static_cast<u32>(m_deviceExtensions.size());
    info.ppEnabledExtensionNames = m_deviceExtensions.data();

    VK_CHECK(vkCreateDevice(physicalDevice, &info, nullptr, &device));
    volkLoadDevice(device);  // direct device function pointers (skips loader dispatch)
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
}

void Context::createAllocator() {
    VmaVulkanFunctions fns{};
    fns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    fns.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo info{};
    info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    if (caps.memoryBudget) info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    info.vulkanApiVersion = VK_API_VERSION_1_3;
    info.physicalDevice = physicalDevice;
    info.device = device;
    info.instance = instance;
    info.pVulkanFunctions = &fns;
    VK_CHECK(vmaCreateAllocator(&info, &allocator));
}

void Context::setName(VkObjectType type, u64 handle, const char* name) const {
    if (!caps.debugUtils || !vkSetDebugUtilsObjectNameEXT) return;
    VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(device, &info);
}

}// namespace pt::vk