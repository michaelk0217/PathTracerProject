#include "App.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string_view>

#include <imgui.h>

#include "../vk/Context.h"
#include "../renderer/Renderer.h"
#include <scene/SceneLoader.h>
#include <app/UiLayer.h>

namespace pt
{
App::App(RenderSettings settings) : m_settings(settings)
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
    
    m_ui = std::make_unique<UiLayer>(*m_ctx, m_window->sdlHandle(), m_renderer->swapchainFormat(), m_renderer->swapchainImageCount());
    
    m_window->setEventHook(&UiLayer::processEvent);

    loadScene(m_sceneId, m_sponzaLighting);
}

App::~App()
{
    m_ctx->waitIdle();

    m_window->setEventHook({});
    m_ui.reset();
    m_renderer.reset();
    m_ctx.reset();
    m_window.reset();
}

void App::loadScene(SceneId id, SponzaLighting lighting)
{
    if (id == SceneId::CornellBox)
    {
        m_scene = makeCornellBox();
    }
    else
    {
        m_scene = loadGltf(std::string(PT_ASSET_DIR) + "/scenes/sponza/Sponza.gltf");
        if (lighting != SponzaLighting::AlbedoOnly)
        {
            setUpSponzaLighting(m_scene, lighting == SponzaLighting::Sun);
        }
    }
    m_sceneId = id;
    m_sponzaLighting = lighting;
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

        if (m_reloadScene)
        {
            loadScene(m_sceneId, m_sponzaLighting);
            m_reloadScene = false;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (m_window->input().rotating) io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        else io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

        m_ui->beginFrame();
        drawUi();
        m_ui->endFrame();

        if (!io.WantCaptureKeyboard) updateCamera(dt);

        m_renderer->renderFrame(m_scene, m_settings,
            [this](VkCommandBuffer cmd, VkImageView view, Extent2D extent) { m_ui->record(cmd, view, extent); });
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
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({ 12.0f, 12.0f }, ImGuiCond_FirstUseEver);
    ImGui::Begin("Path Tracer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // --- Performance ---
    m_frameTimesMs[m_frameTimeHead] = io.DeltaTime * 1000.0f;
    m_frameTimeHead = (m_frameTimeHead + 1) % m_frameTimesMs.size();

    const Extent2D fb = m_window->framebufferExtent();
    ImGui::Text("%.1f FPS  (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::PlotLines("##frametimes", m_frameTimesMs.data(), int(m_frameTimesMs.size()), int(m_frameTimeHead),
        "frame time (ms)", 0.0f, 33.3f, ImVec2(0.0f, 50.0f));
    ImGui::Text("%u x %u%s", fb.width, fb.height, m_settings.vsync ? "  (vsync on: FPS capped)" : "");


    // --- Accumulation ---
    ImGui::Text("%u spp", m_renderer->accumulatedFrames());
    ImGui::SameLine();
    if (ImGui::Button("Reset")) m_renderer->resetAccumulation();

    // --- Scene ---
    ImGui::SeparatorText("Scene");
    int sceneIndex = int(m_sceneId);
    const char* sceneNames[] = { "Cornell box", "Sponza" };
    if (ImGui::Combo("Scene", &sceneIndex, sceneNames, IM_ARRAYSIZE(sceneNames))) 
    {
        m_sceneId = SceneId(sceneIndex);
        m_reloadScene = true;
    }

    if (m_sceneId == SceneId::Sponza) 
    {
        int lighting = int(m_sponzaLighting);
        bool changed = false;
        changed |= ImGui::RadioButton("Albedo", &lighting, int(SponzaLighting::AlbedoOnly));   ImGui::SameLine();
        changed |= ImGui::RadioButton("Area light", &lighting, int(SponzaLighting::CeilingLight)); ImGui::SameLine();
        changed |= ImGui::RadioButton("Sun", &lighting, int(SponzaLighting::Sun));
        if (changed) { m_sponzaLighting = SponzaLighting(lighting); m_reloadScene = true; }
    }

    if (m_scene.lightMaterial >= 0) 
    {
        const u32 minSamples = 1, maxSamples = 64;
        ImGui::SliderScalar("Light samples", ImGuiDataType_U32, &m_settings.lightSampleCount, &minSamples, &maxSamples);
    }

    const u32 minBounces = 0;
    const u32 maxBounces = 16;
    ImGui::SliderScalar("Max Bounces", ImGuiDataType_U32, &m_settings.maxBounces, &minBounces, &maxBounces);


    ImGui::End();
}
}