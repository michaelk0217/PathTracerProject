#pragma once

#include <filesystem>
#include <optional>

#include <scene/Scene.h>
#include "GltfUtils.h"

namespace pt
{
// procedural cornell box for lightweight reference scene (ai generated)
Scene makeCornellBox();

Scene loadGltf(const std::string& path);
}