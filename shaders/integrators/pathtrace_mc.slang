#include "common/bindings.slangh"


struct HitPayload
{
    float  hitT; // < 0 => miss
    float3 position;
    float2 uv;
    float3 normal;
    uint   materialIndex;
};

struct ShadowPayload
{
    uint visible;
};

static const float pi = 3.14159265;

// Both panels face down into their scene
static const float3 lightNormal = float3(0.0, -1.0, 0.0);

// PCG reference: Nathan Reed, Hash Functions for GPU Rendering
float nextRandom(inout uint state)
{
    // PCG mixes the pixel seed so nearby pixels sample different spots
    state = state * 747796405u + 2891336453u;
    uint bits = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    bits ^= bits >> 22u;
    return float(bits >> 8u) / 16777216.0;
}

float3 estimateDirectLighting(HitPayload hit, float3 albedo, float3 emission, uint seed, FrameUniforms frame)
{
    float lightArea = frame.lightWidth * frame.lightWidth;
    float total = 0.0;

    for (uint i = 0; i < frame.lightSampleCount; ++i) {
        float x = nextRandom(seed) - 0.5;
        float z = nextRandom(seed) - 0.5;
        float3 lightPoint = frame.lightPosition + float3(x, 0.0, z) * frame.lightWidth;
        float3 toLight = lightPoint - hit.position;
        float distanceSquared = dot(toLight, toLight);
        if (distanceSquared <= 0.0) continue;

        float3 direction = toLight * rsqrt(distanceSquared);
        float surfaceCos = max(dot(hit.normal, direction), 0.0);
        float lightCos = max(dot(lightNormal, -direction), 0.0);

        // Eyal's shadow check goes here before adding the sample
        total += surfaceCos * lightCos / distanceSquared;
    }

    // Uniform sampling gives each point a density of one over the light area
    return (albedo / pi) * emission * lightArea * (total / float(frame.lightSampleCount));
}

[shader("raygeneration")]
void rayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims  = DispatchRaysDimensions().xy;
    FrameUniforms frame = pc.frame[0];

    RayDesc ray = primaryRay(frame, pixel, dims, float2(0.5, 0.5));

    HitPayload p;
    p.hitT = -1.0;
    //       accel   flags          mask  sbtOffset sbtStride missIndex
    TraceRay(gTlas, RAY_FLAG_NONE, 0xFF, 0,        0,        0,        ray, p);

    float3 color = float3(0.0);
    if (p.hitT >= 0.0) {
        GpuMaterial material = pc.materials[p.materialIndex];
        float3 albedo = material.baseColor;
        if (material.baseColorTexture >= 0) {
            albedo *= gTextures[NonUniformResourceIndex(material.baseColorTexture)].SampleLevel(p.uv, 0).rgb;
        }

        if (any(frame.sunIrradiance > 0.0)) {
            // AI-assisted direct sunlight with the same direction at every hit
            float surfaceCos = max(dot(p.normal, frame.directionToSun), 0.0);
            color = (albedo / pi) * frame.sunIrradiance * surfaceCos;
        } else if (frame.lightMaterial < 0) {
            color = albedo;
        } else if (p.materialIndex == uint(frame.lightMaterial)) {
            // The ceiling panel gives off light on the side facing the floor
            color = dot(lightNormal, -ray.Direction) > 0.0 ? material.emission : float3(0.0);
        } else {
            uint seed = pixel.y * dims.x + pixel.x;
            float3 emission = pc.materials[frame.lightMaterial].emission;
            color = estimateDirectLighting(p, albedo, emission, seed, frame);
        }
    }

    gImages[kSlotAccumulation][pixel] = float4(color, 1.0);
}

[shader("miss")]
void miss(inout HitPayload p)
{
    p.hitT = -1.0;
}

[shader("miss")]
void shadowMiss(inout ShadowPayload p)
{
    p.visible = 1;
}

[shader("closesthit")]
void closestHit(inout HitPayload p, in BuiltInTriangleIntersectionAttributes attr)
{
    SurfaceHit s = fetchSurface(InstanceID(), PrimitiveIndex(), attr.barycentrics, ObjectToWorld3x4(),
                                WorldRayOrigin(), WorldRayDirection(), RayTCurrent());
    p.hitT = RayTCurrent();
    p.position = s.position;
    p.normal = s.normal;
    p.uv = s.uv;
    p.materialIndex = s.materialIndex;
}