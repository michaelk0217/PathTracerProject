#include <renderer/GpuScene.h>

#include <algorithm>

#include <vk/Context.h>
#include <vk/Immediate.h>

namespace pt
{
namespace
{

// ai generated function
template <class T>
vk::Buffer makeUploadBuffer(const vk::Context& ctx, const std::vector<T>& data, VkBufferUsageFlags usage,
    const char* name)
{
    const VkDeviceSize bytes = sizeof(T) * data.size();
    vk::Buffer buffer(ctx, std::max<VkDeviceSize>(bytes, 16), usage, vk::MemoryUsage::Upload, name);
    if (bytes > 0) buffer.upload(data.data(), bytes);
    return buffer;
}

constexpr VkBufferUsageFlags kGeometryUsage =
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
}

// GpuScene::GpuScene(const vk::Context& ctx) : m_ctx(ctx) {};
GpuScene::GpuScene(const vk::Context& ctx, vk::BindlessDescriptors& bindless) : m_ctx(ctx), m_bindless(bindless) {};

GpuScene::~GpuScene()
{
    m_tlas.destroy(m_ctx.device);
    destroyBlases();
}

bool GpuScene::sync(Scene& scene)
{
    const u32 dirty = scene.dirtyFlags();
    if (dirty == DirtyNone) return false;

    if (dirty & (DirtyGeometry | DirtyMaterials | DirtyInstances | DirtyTextures)) m_ctx.waitIdle();
    
    if (dirty & DirtyGeometry) { 
        uploadGeometry(scene);
        buildBlases();
    }

    if (dirty & DirtyMaterials) uploadMaterials(scene);
    if (dirty & (DirtyGeometry | DirtyInstances)) uploadMeshInfos(scene);
    if (dirty & (DirtyGeometry | DirtyInstances | DirtyMaterials)) uploadLights(scene);
    if (dirty & (DirtyGeometry | DirtyInstances)) buildTlas(scene);
    if (dirty & (DirtyGeometry | DirtyMaterials | DirtyInstances))
    {
        log::info("GpuScene: {} meshes, {} instances, {} materials", meshCount(),
            scene.instances.size(), scene.materials.size());
    }

    if (dirty & DirtyTextures) uploadTextures(scene);

    scene.clearDirty();
    return true;
}

void GpuScene::uploadGeometry(const Scene& scene)
{
    std::vector<gpu::GpuVertex> vertices;
    std::vector<u32> indices;
    m_vertexOffset.clear();
    m_vertexCount.clear();
    m_indexOffset.clear();
    m_triangleCount.clear();

    for (const Mesh& mesh : scene.meshes)
    {
        PT_ASSERT(mesh.indices.size() % 3 == 0, "mesh indices must be trinagles");

        m_vertexOffset.push_back(static_cast<u32>(vertices.size()));
        m_vertexCount.push_back(static_cast<u32>(mesh.vertices.size()));
        m_indexOffset.push_back(static_cast<u32>(indices.size()));
        m_triangleCount.push_back(static_cast<u32>(mesh.indices.size() / 3));

        for (const Vertex& v : mesh.vertices)
        {
            vertices.push_back({ v.position, v.normal, v.uv });
        }
        indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());
    }

    m_vertices = makeUploadBuffer(m_ctx, vertices, kGeometryUsage, "SceneVertices");
    m_indices = makeUploadBuffer(m_ctx, indices, kGeometryUsage, "SceneIndices");
}

void GpuScene::uploadMaterials(const Scene& scene)
{
    std::vector<gpu::GpuMaterial> mats;
    mats.reserve(scene.materials.size());
    for (const Material& m : scene.materials) {
        gpu::GpuMaterial g{};
        g.baseColor = m.baseColor;
        g.roughness = m.roughness;
        g.emission = m.emission;
        g.metallic = m.metallic;
        g.ior = m.ior;
        g.transmission = m.transmission;
        g.baseColorTexture = m.baseColorTexture;
        g.normalTexture = m.normalTexture;
        g.metallicRoughnessTexture = m.metallicRoughnessTexture;
        mats.push_back(g);
    }
    m_materials = makeUploadBuffer(m_ctx, mats, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Materials");
}

void GpuScene::uploadMeshInfos(const Scene& scene)
{
    // One entry per INSTANCE (not per mesh): in 4d, instance i gets customIndex = i,
    // so InstanceID() in a hit shader indexes this table directly.
    std::vector<gpu::GpuMeshInfo> infos;
    infos.reserve(scene.instances.size());
    for (const Instance& inst : scene.instances) {
        const u32 m = inst.meshIndex;
        gpu::GpuMeshInfo info{};
        info.vertices = m_vertices.address() + VkDeviceSize(m_vertexOffset[m]) * sizeof(gpu::GpuVertex);
        info.indices = m_indices.address() + VkDeviceSize(m_indexOffset[m]) * sizeof(u32);
        info.materialIndex = scene.meshes[m].materialIndex;
        info.triangleCount = m_triangleCount[m];
        infos.push_back(info);
    }
    m_meshInfos = makeUploadBuffer(m_ctx, infos, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "MeshInfos");
}


// ai generated
void GpuScene::uploadLights(const Scene& scene)
{
    std::vector<gpu::GpuLight> lights;
    float totalPower = 0.0f;

    for (const Instance& inst : scene.instances) {
        const Mesh& mesh = scene.meshes[inst.meshIndex];
        const vec3 Le = scene.materials[mesh.materialIndex].emission;
        const float lum = glm::dot(Le, vec3(0.2126f, 0.7152f, 0.0722f));
        if (lum <= 0.0f) continue;                                  // not emissive

        for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            gpu::GpuLight L{};
            L.p0 = vec3(inst.transform * vec4(mesh.vertices[mesh.indices[t + 0]].position, 1.0f));
            L.p1 = vec3(inst.transform * vec4(mesh.vertices[mesh.indices[t + 1]].position, 1.0f));
            L.p2 = vec3(inst.transform * vec4(mesh.vertices[mesh.indices[t + 2]].position, 1.0f));
            L.emission = Le;
            L.area = 0.5f * glm::length(glm::cross(L.p1 - L.p0, L.p2 - L.p0));
            if (L.area <= 0.0f) continue;                           // degenerate triangle

            L.pmf = lum * L.area;                                   // power (normalized below)
            totalPower += L.pmf;
            lights.push_back(L);
        }
    }

    float running = 0.0f;
    for (gpu::GpuLight& L : lights) {
        L.pmf /= totalPower;
        running += L.pmf;
        L.cdf = running;
    }
    if (!lights.empty()) lights.back().cdf = 1.0f;                  // guard against float rounding

    m_lightCount = static_cast<u32>(lights.size());
    m_lights = makeUploadBuffer(m_ctx, lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Lights");
    log::info("GpuScene: {} emissive triangles", m_lightCount);
}

vk::BlasInput GpuScene::blasInput(u32 meshIndex) const
{
    vk::BlasInput in{};
    in.vertexAddress = m_vertices.address() + VkDeviceSize(m_vertexOffset[meshIndex]) * sizeof(gpu::GpuVertex);
    in.vertexStride = sizeof(gpu::GpuVertex);   // position is at offset 0
    in.vertexCount = m_vertexCount[meshIndex];
    in.indexAddress = m_indices.address() + VkDeviceSize(m_indexOffset[meshIndex]) * sizeof(u32);
    in.triangleCount = m_triangleCount[meshIndex];
    in.opaque = true;
    return in;
}


void GpuScene::destroyBlases()
{
    for (vk::AccelStruct& b : m_blas) b.destroy(m_ctx.device);
    m_blas.clear();
}


// ai generated
void GpuScene::buildBlases()
{
    destroyBlases();
    m_blas.reserve(meshCount());

    std::vector<vk::Buffer> scratch;
    vk::submitImmediate(m_ctx, [&](VkCommandBuffer cmd) {
        for (u32 i = 0; i < meshCount(); ++i) m_blas.push_back(vk::buildBlas(m_ctx, cmd, blasInput(i), scratch));

        vk::memoryBarrier(cmd,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR |
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);
        });

    VkDeviceSize bytes = 0;
    for (const vk::AccelStruct& b : m_blas) {
        PT_ASSERT(b.address != 0, "BLAS has no device address");
        bytes += b.buffer.size();
    }
    log::info("GpuScene: built {} BLAS ({:.1f} KB)", m_blas.size(), double(bytes) / 1024.0);
}

// ai generated
void GpuScene::buildTlas(const Scene& scene)
{
    m_tlas.destroy(m_ctx.device);   // safe: sync() waited idle for Geometry/Instances changes

    // Instance i <-> GpuMeshInfo i: same order as uploadMeshInfos(), so InstanceID() indexes pc.meshes.
    std::vector<vk::TlasInstance> instances;
    instances.reserve(scene.instances.size());
    for (u32 i = 0; i < scene.instances.size(); ++i) {
        const Instance& inst = scene.instances[i];
        vk::TlasInstance t;
        t.transform = inst.transform;
        t.customIndex = i;
        t.blasAddress = blasAddress(inst.meshIndex);   // instances share their mesh's BLAS
        t.mask = 0xFF;
        instances.push_back(t);
    }

    std::vector<vk::Buffer> keepAlive;
    vk::submitImmediate(m_ctx, [&](VkCommandBuffer cmd) {
        m_tlas = vk::buildTlas(m_ctx, cmd, instances, keepAlive);
        // TLAS build writes -> ray tracing / compute (ray query) reads
        vk::memoryBarrier(cmd,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);
        });

    m_tlasChanged = true;
    log::info("GpuScene: built TLAS ({} instances, {:.1f} KB)", instances.size(),
        double(m_tlas.buffer.size()) / 1024.0);
}

void GpuScene::uploadTextures(const Scene& scene) {
    m_textures.clear();
    m_samplers.clear();

    m_samplers.reserve(scene.textures.size());
    for (auto& tex : scene.textures) {
        vk::Image2D image = vk::Image2D(m_ctx, Extent2D{ .width = tex.width, .height = tex.height }, tex.format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "texture_image");
        image.uploadFromPixelData(m_ctx, (void*) tex.pixels.data());
        m_textures.push_back(std::move(image));

        // in theory, sampler should be cached. but whatever, we only have like 5 textures rn
        // TODO: CACHE THEM LOOOOL
        pt::vk::Sampler::SamplerInfo s{};
        s.addressMode = tex.sampler.wrapU;
        s.maxAniso = m_ctx.caps.maxAnisotropy;
        s.mipLevels = tex.mipLevels;

        m_samplers.emplace_back();
        m_samplers.back().createSampler(m_ctx, s);
    }

    u32 slotIndex = 0;
    for (auto& t : m_textures) {
        m_bindless.setTexture(slotIndex, t.view(), m_samplers[slotIndex].getSampler());
        slotIndex++;
    }
}
}