// #################
// AI GENERATED FILE - I asked to generate a list of structures needed, and make it agreeable between cpu and gpu(slang) code.
// #################


// Structs shared between C++ and Slang. Included by both:
//   C++ : #include "shared/GpuStructs.h"   (pt::gpu namespace)
//   Slang: #include "shared/GpuStructs.h"
// Layout rule: scalar block layout on both sides (-fvk-use-scalar-layout + scalarBlockLayout),
// so C++ struct sizes must match exactly. static_asserts live in renderer/GpuStructsCheck.cpp.
#ifndef PT_SHARED_GPU_STRUCTS_H
#define PT_SHARED_GPU_STRUCTS_H

#ifdef __cplusplus
#include <cstdint>
#include <glm/glm.hpp>


namespace pt::gpu {
    using float2 = glm::vec2;
    using float3 = glm::vec3;
    using float4 = glm::vec4;
    using uint2 = glm::uvec2;
    using uint = std::uint32_t;
    using float4x4 = glm::mat4;
    using std::uint64_t;
    // Device pointers: raw 64-bit address in C++, typed pointer in Slang.
#define PT_PTR(T) std::uint64_t
#define PT_CONST static constexpr
#else
#define PT_PTR(T) T*
#define PT_CONST static const
#endif

// --- Bindless storage-image slots (binding 1 of the global set) -------------
PT_CONST uint kSlotAccumulation = 0;
PT_CONST uint kSlotDisplay = 1;   // tonemapped output



struct GpuVertex {
    float3 position;
    float3 normal;
    float2 uv;
};

struct GpuMaterial {
    float3 baseColor;
    float  roughness;
    float3 emission;
    float  metallic;
    float  ior;
    float  transmission;
    int    baseColorTexture;   // -1 = none
    int    normalTexture;
    int    metallicRoughnessTexture;
    int    emissiveTexture;
};

// one entry per TLAS instance; InstanceID() (customIndex) indexes this table.
struct GpuMeshInfo {
    PT_PTR(GpuVertex) vertices;
    PT_PTR(uint)      indices;
    uint materialIndex;
    uint triangleCount;
};

struct GpuLight
{
    float3 p0;
    float3 p1;
    float3 p2;
    float3 emission; // radiance Le
    float area;
    float pmf;
    float cdf;
};



// Everything a frame needs, passed as push constants (<= 128 bytes guaranteed).
// Larger per-frame data goes into a FrameUniforms buffer referenced by pointer.
struct FrameUniforms {
    float4x4    viewInverse;
    float4x4    projInverse;
    float3      cameraPosition;
    uint        maxBounces;
    float       exposure;
    int         lightCount;
    uint        lightSampleCount;
    float3      directionToSun;
    float3      sunIrradiance;
    uint        frameIndex;
    uint        accumulatedFrames;
    float3      skyRadiance;
    uint        debugView;
    uint        tonemapper;
};

struct PushConstants {
    PT_PTR(FrameUniforms)       frame;
    PT_PTR(GpuMeshInfo)         meshes;
    PT_PTR(GpuMaterial)         materials;
    PT_PTR(GpuLight)            lights;
    uint2                       extent;
};

#ifdef __cplusplus
} // namespace pt::gpu
#endif

#endif // PT_SHARED_GPU_STRUCTS_H