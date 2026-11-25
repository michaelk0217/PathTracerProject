#pragma once
#include <vk/Common.h>



struct SDL_Window;
union SDL_Event;

namespace pt
{
namespace vk { class Context; }

// owns imgui
class UiLayer : NonCopyable
{
public:
	UiLayer(const vk::Context& ctx, SDL_Window* window, VkFormat swapchainFormat, u32 imageCount);
	~UiLayer();

	static bool processEvent(const SDL_Event& e);
	void beginFrame();
	void endFrame();
	void record(VkCommandBuffer cmd, VkImageView target, Extent2D extent) const;

private:
	VkFormat m_colorFormat;
};

}