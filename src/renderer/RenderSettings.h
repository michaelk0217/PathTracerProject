#pragma once

#include <core/Types.h>

namespace pt
{
// Data edited by UI and read by renderer
struct RenderSettings
{
	u32 maxBounces = 8;
	u32 lightSampleCount = 16;
	float exposure = 1.0f;
	u32 debugView = 0;
	bool vsync = true;
};
}