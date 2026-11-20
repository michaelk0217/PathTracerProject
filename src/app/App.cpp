#include "App.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string_view>

#include "../vk/Context.h"
#include "../renderer/Renderer.h"
#include <scene/SceneLoader.h>

namespace pt
{
App::App()
{
    pt::Window::Desc windowDesc{};
    m_window = std::make_unique<Window>(windowDesc);

    pt::vk::ContextDesc contextDesc{};
    contextDesc.instanceExtensions = m_window->requiredVulkanInstanceExtensions();
    contextDesc.createSurface = [&](VkInstance i) { return m_window->createVulkanSurface(i); };

    m_ctx = std::make_unique<vk::Context>(contextDesc);

#if defined(PT_SHADER_DIR)
    const std::filesystem::path shaderDir = PT_SHADER_DIR;
#else
    const std::filesystem::path shaderDir = "shaders";
#endif

    m_renderer = std::make_unique<Renderer>(*m_ctx, m_window->framebufferExtent(), shaderDir, m_settings.vsync);
    
    // m_scene = makeCornellBox();

    m_scene = loadGltf(std::string(PT_ASSET_DIR) + "/scenes/sponza/Sponza.gltf");
}

App::~App()
{
    m_renderer.reset();
    m_ctx.reset();
    m_window.reset();
}

int App::run()
{
	using clock = std::chrono::steady_clock;
	auto last = clock::now();



    while (m_window->pollEvents()) {
        const auto now = clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        if (m_window->isMinimized()) continue;
        if (m_window->consumeResized()) m_renderer->resize(m_window->framebufferExtent());

        updateCamera(dt);
        drawUi();
        m_renderer->renderFrame(m_scene, m_settings);
    }

	return EXIT_SUCCESS;
}

void App::updateCamera(float deltaTime)
{
    const InputState& in = m_window->input();
    vec3 dir(0.0f);
    if (in.forward) dir.z += 1.0f;
    if (in.back) dir.z -= 1.0f;
    if (in.right) dir.x += 1.0f;
    if (in.left) dir.x -= 1.0f;
    if (in.up) dir.y += 1.0f;
    if (in.down) dir.y -= 1.0f;

    bool moved = false;
    if (dir != vec3(0.0f))
    {
        m_scene.camera.move(glm::normalize(dir), deltaTime * (in.fast ? 4.0f : 1.0f));
        moved = true;
    }
    if (in.mouseDelta != vec2(0.0f))
    {
        constexpr float kSensitivity = 0.1f;
        m_scene.camera.rotate(in.mouseDelta.x * kSensitivity, -in.mouseDelta.y * kSensitivity);
        moved = true;
    }

}

void App::drawUi()
{

}
}