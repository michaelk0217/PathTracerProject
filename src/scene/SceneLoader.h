#pragma once

#include <filesystem>
#include <optional>

#include <scene/Scene.h>
#include "GltfUtils.h"
#include <stb_image.h>

namespace pt
{
enum class SceneLighting : int { AlbedoOnly, CeilingLight, Sun, ManyLights };
// procedural cornell box for lightweight reference scene (ai generated)
Scene makeCornellBox();
void setUpSponzaLighting(Scene& scene, SceneLighting lighting);

Scene loadGltf(const std::string& path);
}