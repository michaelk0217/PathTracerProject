#pragma once

#include <memory>

#include "../vk/Common.h"

#include "../platform/Window.h"
#include "../renderer/RenderSettings.h"
#include <scene/Scene.h>

namespace pt
{
namespace vk { class Context; }
class Renderer;

enum class StartupScene { SponzaTextures, SponzaLit, SponzaSun, Cornell };

class App : NonCopyable
{
public:
	explicit App(StartupScene scene = StartupScene::SponzaTextures, RenderSettings settings = {});
	~App();
	int run();

private:
	void updateCamera(float deltaTime);
	void drawUi(); // for imgui later

	// THIS IS ALSO CREATION ORDER --> Destroy opposite order (window last)
	std::unique_ptr<Window> m_window;
	std::unique_ptr<vk::Context> m_ctx;
	std::unique_ptr<Renderer> m_renderer;
	Scene m_scene;
	RenderSettings m_settings;
};

}