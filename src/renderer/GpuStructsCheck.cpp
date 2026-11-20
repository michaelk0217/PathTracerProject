// sanity check file

#include "shared/GpuStructs.h" // refer to this file 

namespace pt::gpu {
static_assert(sizeof(GpuVertex) == 32);
static_assert(sizeof(GpuMaterial) == 52);
static_assert(sizeof(GpuMeshInfo) == 24);
static_assert(sizeof(FrameUniforms) == 148);
static_assert(sizeof(PushConstants) <= 128);
} // namespace pt::gpu