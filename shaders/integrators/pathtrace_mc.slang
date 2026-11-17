// Monte Carlo path tracer (to be implemented) (ray tracing pipeline).

// right now: primary rays only


// #######################
// we are gona turn this ray tracing shader into monte carlo
// ######################

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
        // Debug view: normals mapped from [-1,1] to [0,1].
        // color = p.normal * 0.5 + 0.5;
        // Albedo view instead: color = pc.materials[p.materialIndex].baseColor;
        GpuMaterial m = pc.materials[p.materialIndex];
        color = m.baseColor * gTextures[NonUniformResourceIndex(m.baseColorTexture)].SampleLevel(p.uv,0).rgb;
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