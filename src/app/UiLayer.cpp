#include <app/UiLayer.h>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <core/Assert.h>
#include <vk/Context.h>

namespace pt
{
UiLayer::UiLayer(const vk::Context& ctx, SDL_Window* window, VkFormat swapchainFormat, u32 imageCount)
	: m_colorFormat(swapchainFormat)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    const float scale = SDL_GetWindowDisplayScale(window);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;

    ImGui_ImplSDL3_InitForVulkan(window);

    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = VK_API_VERSION_1_3;
    info.Instance = ctx.instance;
    info.PhysicalDevice = ctx.physicalDevice;
    info.Device = ctx.device;
    info.QueueFamily = ctx.queueFamily;
    info.Queue = ctx.queue;
    info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    info.MinImageCount = 2;
    info.ImageCount = imageCount;
    info.UseDynamicRendering = true;
    auto& rendering = info.PipelineInfoMain.PipelineRenderingCreateInfo;
    rendering = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &m_colorFormat;
    info.CheckVkResultFn = [](VkResult r) { PT_ASSERT(r >= 0, "ImGui Vulkan backend error"); };
    ImGui_ImplVulkan_Init(&info);
}

UiLayer::~UiLayer()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

bool UiLayer::processEvent(const SDL_Event& e)
{
    ImGui_ImplSDL3_ProcessEvent(&e);
    const ImGuiIO& io = ImGui::GetIO();
    switch (e.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_WHEEL:
        return io.WantCaptureMouse;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
        return io.WantCaptureKeyboard;
    default:
        return false;
    }
}

void UiLayer::beginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void UiLayer::endFrame() { ImGui::Render(); }

void UiLayer::record(VkCommandBuffer cmd, VkImageView target, Extent2D extent) const
{
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData) return;

    VkRenderingAttachmentInfo color{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    color.imageView = target;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo ri{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    ri.renderArea = { { 0, 0 }, { extent.width, extent.height } };
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &color;

    vkCmdBeginRendering(cmd, &ri);
    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
    vkCmdEndRendering(cmd);
}
}