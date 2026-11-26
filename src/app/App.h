#pragma once

#include <memory>
#include <array>

#include "../vk/Common.h"

#include "../platform/Window.h"
#include "../renderer/RenderSettings.h"
#include <scene/Scene.h>
#include <scene/SceneLoader.h>

namespace pt
{
namespace vk { class Context; }
class Renderer;
class UiLayer;

enum class SceneId : int {CornellBox, Sponza};
//enum class SponzaLighting : int {AlbedoOnly, CeilingLight, Sun, ManyLights}; //moved to sceneloader with a name SceneLighting

class App : NonCopyable
{
public:
	//explicit App(StartupScene scene = StartupScene::SponzaTextures, RenderSettings settings = {});
	App(RenderSettings settings = {});
	~App();
	int run();

private:
	void updateCamera(float deltaTime);
	void drawUi();

	void loadScene(SceneId id, SceneLighting lighting);

	// THIS IS ALSO CREATION ORDER --> Destroy opposite order (window last)
	std::unique_ptr<Window> m_window;
	std::unique_ptr<vk::Context> m_ctx;
	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<UiLayer> m_ui;
	Scene m_scene;
	RenderSettings m_settings;

	SceneId m_sceneId = SceneId::CornellBox;
	SceneLighting m_sponzaLighting = SceneLighting::CeilingLight;
	bool m_reloadScene = false;

	std::array<float, 120> m_frameTimesMs{};
	u32 m_frameTimeHead = 0;
};

}