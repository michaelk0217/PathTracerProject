#include "common/bindings.slangh"
#include "common/sampling.slangh"
#include "common/bsdf.slangh"
#include "common/lights.slangh"

struct HitPayload
{
    float  hitT; // < 0 => miss
    float3 position;
    float2 uv;
    float3 normal;
    float3 geometricNormal;
    float3 tangent;
    float3 bitangent;
    uint materialIndex;
    bool frontFace;
};

struct ShadowPayload
{
    bool hit;
};


// Both panels face down into their scene
static const float3 lightNormal = float3(0.0, -1.0, 0.0);

static const uint kRussianRouletteStart = 3;

struct Surface
{
    float3 position;
    float3 normal;
    float3 geometricNormal;
    float3 wo;
    BsdfParams bsdf;
    float3 albedo;
    float3 emission;
    bool isLight;
};

Surface evaluateSurface(HitPayload p, FrameUniforms frame, float3 wo)
{
    GpuMaterial m = pc.materials[p.materialIndex];
    Surface s;
    s.position = p.position;
    s.normal = p.normal;
    s.geometricNormal = p.geometricNormal;
    s.wo = wo;

    float3 baseColor = m.baseColor;
    if (m.baseColorTexture >= 0)
    {
        baseColor *= gTextures[NonUniformResourceIndex(m.baseColorTexture)].SampleLevel(p.uv, 0).rgb;
    }

    float roughness = m.roughness;
    float metallic = m.metallic;
    if (m.metallicRoughnessTexture >= 0)
    {
        float4 mr = gTextures[NonUniformResourceIndex(m.metallicRoughnessTexture)].SampleLevel(p.uv, 0);
        roughness *= mr.g;
        metallic *= mr.b;
    }

    s.normal = p.normal;
    if (m.normalTexture >= 0 && dot(p.tangent, p.tangent) > 0.0)
    {
        float3 tn = gTextures[NonUniformResourceIndex(m.normalTexture)].SampleLevel(p.uv, 0).xyz * 2.0 - 1.0;
        float3 mapped = normalize(p.tangent * tn.x + p.bitangent * tn.y + p.normal * tn.z);
        if (dot(mapped, wo) > 0.0) s.normal = mapped; // cuz normal map can tilt away from viewer
    }

    s.bsdf.baseColor = baseColor;
    s.bsdf.metallic = saturate(metallic);
    s.bsdf.roughness = saturate(roughness);
    s.albedo = baseColor;
    s.emission = m.emission;
    // s.isLight = frame.lightMaterial >= 0 && p.materialIndex == uint(frame.lightMaterial);
    s.isLight = any(m.emission > 0.0);

    return s;
}

// for shadows
// ret true if nothing blocks the origin + dir *maxDist
bool isVisible(float3 origin, float3 direction, float maxDistance)
{
    ShadowPayload sp;
    sp.hit = true;
    RayDesc r = makeRay(origin, direction, 0.0, maxDistance);
    TraceRay(gTlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
             0xFF, 0, 0, 1, r, sp);
    return !sp.hit;
}

// integrating emissive triangles and refactoring sampleAreaLight
// NEE over all emissive triangles: Lo = mean over samples of  f · Le · V · cosS · cosL / (r² · pdfArea)
float3 sampleEmissiveLights(Surface s, inout uint rng, uint sampleCount, FrameUniforms frame)
{
    float3 origin = offsetRayOrigin(s.position, s.geometricNormal);
    float3 total = float3(0.0);

    for (uint i = 0; i < sampleCount; ++i)
    {
        LightSample ls = sampleLight(rng, frame.lightCount);
        if (ls.pdfArea <= 0.0) continue;

        float3 toLight = ls.position - origin;
        float dist2 = dot(toLight, toLight);
        if (dist2 <= 1e-8) continue;
        float dist = sqrt(dist2);
        float3 dir = toLight / dist;

        float cosSurface = dot(s.normal, dir);
        float cosLight = dot(ls.normal, -dir); // > 0: emitting side
        if (cosSurface <= 0.0 || cosLight <= 0.0) continue;

        float3 f = bsdfEval(s.bsdf, s.normal, s.wo, dir);
        if (all(f <= 0.0)) continue; // skip the shadow ray if it cant contribute

        if (isVisible(origin, dir, dist * 0.999))
            total += f * ls.emission * (cosSurface * cosLight / (dist2 * ls.pdfArea));
    }
    return total / float(sampleCount);
}

// refactored estimateDirectLighting function below
// using NEE
// float3 sampleAreaLight(Surface s, inout uint rng, uint sampleCount, FrameUniforms frame)
// {
//     float3 Le = pc.materials[frame.lightMaterial].emission;
//     float lightArea = frame.lightWidth * frame.lightWidth;
//     float3 origin = offsetRayOrigin(s.position, s.geometricNormal);

//     float3 total = float3(0.0);
//     for (uint i = 0; i < sampleCount; ++i)
//     {
//         float2 u = nextRandom2(rng);
//         float3 lightPoint = frame.lightPosition + float3(u.x - 0.5, 0.0, u.y - 0.5) * frame.lightWidth;
//         float3 toLight = lightPoint - origin;
//         float dist2 = dot(toLight, toLight);
//         if (dist2 <= 1e-8) continue;
//         float dist = sqrt(dist2);
//         float3 dir = toLight / dist;

//         float cosSurface = dot(s.normal, dir);
//         float cosLight = dot(lightNormal, -dir);
//         if (cosSurface <= 0.0 || cosLight <= 0.0) continue;

//         // NEE by shadow ray
//         if (isVisible(origin, dir, dist * 0.999))
//         {
//             // total += cosSurface * cosLight / dist2;
//             total += bsdfEval(s.bsdf, s.normal, s.wo, dir) * (cosSurface * cosLight / dist2);
//         }
//     }
//     // return (s.albedo / kPi) * Le * lightArea * (total / float(sampleCount));
//     return Le * lightArea * (total / float(sampleCount));
// }

// float3 estimateDirectLighting(HitPayload hit, float3 albedo, float3 emission, inout uint rng, FrameUniforms frame)
// {
//     float lightArea = frame.lightWidth * frame.lightWidth;
//     float total = 0.0;

//     for (uint i = 0; i < frame.lightSampleCount; ++i) {
//         float x = nextRandom(rng) - 0.5;
//         float z = nextRandom(rng) - 0.5;
//         float3 lightPoint = frame.lightPosition + float3(x, 0.0, z) * frame.lightWidth;
//         float3 toLight = lightPoint - hit.position;
//         float distanceSquared = dot(toLight, toLight);
//         if (distanceSquared <= 0.0) continue;

//         float lightDist = sqrt(distanceSquared);
//         float3 direction = toLight * rsqrt(distanceSquared);
//         float surfaceCos = max(dot(hit.normal, direction), 0.0);
//         float lightCos = max(dot(lightNormal, -direction), 0.0);

//         // Eyal's shadow check goes here before adding the sample
//         //shadowRay from surface to lightpoint
//         uint flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
//         ShadowPayload shadowP;
//         shadowP.hit = true;
//         float3 offsetOrigin = hit.position + .0001 * hit.normal;
//         RayDesc shadowRay = makeRay(offsetOrigin, normalize(direction), .0001, lightDist -.001);
//         TraceRay(gTlas, flags, 0xFF, 0, 0, 1, shadowRay, shadowP);
//         if (shadowP.hit == false){
//            total += surfaceCos * lightCos / distanceSquared;
//         }
//     }
//     // Uniform sampling gives each point a density of one over the light area
//     return (albedo / pi) * emission * lightArea * (total / float(frame.lightSampleCount));
// }

// refactored sun sampling
float3 sampleSun(Surface s, FrameUniforms frame)
{
    float cosSurface = dot(s.normal, frame.directionToSun);
    if (cosSurface <= 0.0) return float3(0.0);

    float3 origin = offsetRayOrigin(s.position, s.geometricNormal);
    if (!isVisible(origin, frame.directionToSun, 1e30)) return float3(0.0);
    // return (s.albedo / kPi) * frame.sunIrradiance * cosSurface;
    return bsdfEval(s.bsdf, s.normal, s.wo, frame.directionToSun) * frame.sunIrradiance * cosSurface;
}

float3 directLighting(Surface s, inout uint rng, uint sampleCount, FrameUniforms frame)
{
    float3 L = float3(0.0);
    if (frame.lightCount > 0)
    {
        L += sampleEmissiveLights(s, rng, sampleCount, frame);
    }
    if (any(frame.sunIrradiance > 0.0))
    {
        L += sampleSun(s, frame);
    }

    return L;
}



[shader("raygeneration")]
void rayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims  = DispatchRaysDimensions().xy;
    FrameUniforms frame = pc.frame[0];

    uint rng = initRng(pixel, dims, frame.frameIndex);

    // float2 jitter = float2(nextRandom(rng), nextRandom(rng));
    // nextRandom2 (jitter) basically samples randomly within a pixel, not dead center. 
    // this smooths out as you pick up more samples. free anti-ailiasing
    RayDesc ray = primaryRay(frame, pixel, dims, nextRandom2(rng));

    const bool hasLights = frame.lightCount >= 0 || any(frame.sunIrradiance > 0.0) || any(frame.skyRadiance > 0.0);

    float3 radiance = float3(0.0); // amount of light the path collects (our color)
    float3 throughput = float3(1.0); // how much of later-found light reaches camera

    for (uint bounce = 0; bounce <= frame.maxBounces; ++bounce)
    {
        HitPayload p;
        p.hitT = -1.0;
        //       accel   flags          mask  sbtOffset sbtStride missIndex
        TraceRay(gTlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, p);

        if (p.hitT < 0.0) // escaped. add sky val
        {
            radiance += throughput * frame.skyRadiance;
            break;
        }

        Surface s = evaluateSurface(p, frame, -ray.Direction);

        // --------DEBUG VIEW-------
        if (bounce == 0 && frame.debugView != 0)
        {
            if (frame.debugView == 1) radiance = s.albedo;
            else if (frame.debugView == 2) radiance = s.normal * 0.5 + 0.5;
            else if (frame.debugView == 3) radiance = float3(s.bsdf.roughness);
            else if (frame.debugView == 4) radiance = float3(s.bsdf.metallic);
            break;
        }
        // ------------------------

        if (!hasLights) { radiance = s.albedo; break;}

        if (s.isLight)
        {
            if (bounce == 0 && p.frontFace)
            {
                radiance += throughput * s.emission;
            }
            break;
        }

        uint lightSamples = (bounce == 0) ? frame.lightSampleCount : 1u;
        radiance += throughput * directLighting(s, rng, lightSamples, frame);

        if (bounce == frame.maxBounces) break;

        // float3 dir = sampleCosineHemisphere(s.normal, nextRandom2(rng));
        // if (dot(dir, s.geometricNormal) <= 0.0) break;

        BsdfSample bs = bsdfSample(s.bsdf, s.normal, s.wo, nextRandom3(rng));
        if (!bs.valid) break;
        if (dot(bs.wi, s.geometricNormal) <= 0.0) break;

        // throughput *= s.albedo;
        throughput *= bs.weight;

        // russian roulette
        // ending low contribution paths ithout bias
        if (bounce >= kRussianRouletteStart)
        {
            float q = clamp(max(throughput.r, max(throughput.g, throughput.b)), 0.05, 0.95);
            if (nextRandom(rng) >= q) break;
            throughput /= q;
        }

        ray = makeRay(offsetRayOrigin(s.position, s.geometricNormal), bs.wi, 0.0, 1e30);
    }


    

    // float3 color = float3(0.0);
    // if (p.hitT >= 0.0) {
    //     GpuMaterial material = pc.materials[p.materialIndex];
    //     float3 albedo = material.baseColor;
    //     if (material.baseColorTexture >= 0) {
    //         albedo *= gTextures[NonUniformResourceIndex(material.baseColorTexture)].SampleLevel(p.uv, 0).rgb;
    //     }

    //     if (any(frame.sunIrradiance > 0.0)) {
    //         // AI-assisted direct sunlight with the same direction at every hit
    //         float surfaceCos = max(dot(p.normal, frame.directionToSun), 0.0);
    //         color = (albedo / pi) * frame.sunIrradiance * surfaceCos;
    //     } else if (frame.lightMaterial < 0) {
    //         color = albedo;
    //     } else if (p.materialIndex == uint(frame.lightMaterial)) {
    //         // The ceiling panel gives off light on the side facing the floor
    //         color = dot(lightNormal, -ray.Direction) > 0.0 ? material.emission : float3(0.0);
    //     } else {
    //         // uint seed = pixel.y * dims.x + pixel.x;
    //         float3 emission = pc.materials[frame.lightMaterial].emission;
    //         color = estimateDirectLighting(p, albedo, emission, rng, frame);
    //     }
    // }

    // progressive accumulation: using averaging
    float3 result = radiance;
    if (frame.accumulatedFrames > 0)
    {
        float3 prev = gImages[kSlotAccumulation][pixel].rgb;
        result = lerp(prev, radiance, 1.0 / float(frame.accumulatedFrames + 1));
    }
    gImages[kSlotAccumulation][pixel] = float4(result, 1.0);
}

[shader("miss")]
void miss(inout HitPayload p)
{
    p.hitT = -1.0;
}

[shader("miss")]
void shadowMiss(inout ShadowPayload p)
{
    p.hit = false;
}

[shader("closesthit")]
void closestHit(inout HitPayload p, in BuiltInTriangleIntersectionAttributes attr)
{
    SurfaceHit s = fetchSurface(InstanceID(), PrimitiveIndex(), attr.barycentrics, ObjectToWorld3x4(),
                                WorldRayOrigin(), WorldRayDirection(), RayTCurrent());
    p.hitT = RayTCurrent();
    p.position = s.position;
    p.normal = s.normal;
    p.geometricNormal = s.geometricNormal;
    p.uv = s.uv;
    p.materialIndex = s.materialIndex;
    p.tangent = s.tangent;
    p.bitangent = s.bitangent;
    p.frontFace = s.frontFace;
}