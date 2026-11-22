#include "scene/SceneLoader.h"

#include "core/Assert.h"

namespace pt {

//#############################################
// AI Generated code for procedural cornell box
//#############################################

namespace {    
// Quad from corner `o` spanned by edges `u` and `v`; normal = normalize(cross(u, v)).
Mesh makeQuad(const char* name, vec3 o, vec3 u, vec3 v, u32 material) {
    const vec3 n = glm::normalize(glm::cross(u, v));
    Mesh m;
    m.name = name;
    m.materialIndex = material;
    m.vertices = {
        {o, n, {0, 0}},
        {o + u, n, {1, 0}},
        {o + u + v, n, {1, 1}},
        {o + v, n, {0, 1}},
    };
    m.indices = { 0, 1, 2, 0, 2, 3 };
    return m;
}

Mesh makeBox(const char* name, vec3 mn, vec3 mx, u32 material) {
    Mesh box;
    box.name = name;
    box.materialIndex = material;
    const vec3 d = mx - mn;
    const Mesh faces[] = {
        makeQuad("", mn, {0, 0, d.z}, {0, d.y, 0}, material),                      // -X
        makeQuad("", {mx.x, mn.y, mn.z}, {0, d.y, 0}, {0, 0, d.z}, material),      // +X
        makeQuad("", mn, {d.x, 0, 0}, {0, 0, d.z}, material),                      // -Y
        makeQuad("", {mn.x, mx.y, mn.z}, {0, 0, d.z}, {d.x, 0, 0}, material),      // +Y
        makeQuad("", mn, {0, d.y, 0}, {d.x, 0, 0}, material),                      // -Z
        makeQuad("", {mn.x, mn.y, mx.z}, {d.x, 0, 0}, {0, d.y, 0}, material),      // +Z
    };
    for (const Mesh& f : faces) {
        const u32 base = static_cast<u32>(box.vertices.size());
        box.vertices.insert(box.vertices.end(), f.vertices.begin(), f.vertices.end());
        for (u32 i : f.indices) box.indices.push_back(base + i);
    }
    return box;
}

// AI-assisted setup for the square light shared by both test scenes
void addCeilingLight(Scene& scene, vec3 center, float width, vec3 emission) {
    const u32 material = scene.addMaterial({ .name = "light", .baseColor = vec3(0.0f), .emission = emission });
    const vec3 corner = center - vec3(width * 0.5f, 0.0f, width * 0.5f);
    scene.addInstance(scene.addMesh(makeQuad("light", corner, {width, 0, 0}, {0, 0, width}, material)));
    scene.lightMaterial = static_cast<i32>(material);
    scene.lightPosition = center;
    scene.lightWidth = width;
}

} // namespace

void setUpSponzaLighting(Scene& scene, bool useSun) {
    if (useSun) {
        // AI-assisted sun setup with the direction pointing from the surface toward the sun
        scene.directionToSun = glm::normalize(vec3(-0.48f, 1.0f, -0.32f));
        scene.sunIrradiance = {5.2f, 4.9f, 4.4f};
    } else {
        addCeilingLight(scene, {-2.0f, 6.8f, -0.3f}, 16.0f, {36.0f, 34.0f, 31.0f});
    }
    scene.camera.position = {-8.4f, 1.7f, -0.15f};
    scene.camera.yaw = 0.0f;
    scene.camera.pitch = 6.0f;
    scene.camera.verticalFovDeg = 55.0f;
}

Scene makeCornellBox() {
    Scene s;
    const u32 white = s.addMaterial({ .name = "white", .baseColor = {0.73f, 0.73f, 0.73f} });
    const u32 red = s.addMaterial({ .name = "red", .baseColor = {0.65f, 0.05f, 0.05f} });
    const u32 green = s.addMaterial({ .name = "green", .baseColor = {0.12f, 0.45f, 0.15f} });

    // Room spans [-1,1] x [0,2] x [-1,1], open towards +Z (camera side).
    auto add = [&](Mesh m) { s.addInstance(s.addMesh(std::move(m))); };
    add(makeQuad("floor", { -1, 0, -1 }, { 0, 0, 2 }, { 2, 0, 0 }, white));
    add(makeQuad("ceiling", { -1, 2, -1 }, { 2, 0, 0 }, { 0, 0, 2 }, white));
    add(makeQuad("back", { -1, 0, -1 }, { 2, 0, 0 }, { 0, 2, 0 }, white));
    add(makeQuad("left", { -1, 0, -1 }, { 0, 2, 0 }, { 0, 0, 2 }, red));
    add(makeQuad("right", { 1, 0, -1 }, { 0, 0, 2 }, { 0, 2, 0 }, green));
    addCeilingLight(s, {0.0f, 1.98f, 0.0f}, 0.5f, {17.0f, 12.0f, 4.0f});
    add(makeBox("tallBox", { -0.6f, 0.0f, -0.6f }, { -0.1f, 1.2f, -0.1f }, white));

    s.camera.position = { 0.0f, 1.0f, 3.4f };
    s.camera.yaw = -90.0f;
    s.camera.pitch = 0.0f;
    s.camera.verticalFovDeg = 40.0f;
    s.markDirty(DirtyAll);
    return s;
}

template <typename AddFunction>
void addGLTFMeshToScene(GltfNode* currentNode, Scene* scene, AddFunction&& add) {
    glm::mat4 currentMatrix = currentNode->getMatrix();
    for (auto p : currentNode->primitives) {
        Mesh currentMesh;
        currentMesh.vertices = std::move(p.vertices);
        currentMesh.indices = std::move(p.indices);
        currentMesh.materialIndex = p.materialIndex;
        add(currentMesh, currentMatrix);
    }

    for (auto child : currentNode->children) {
        addGLTFMeshToScene(child, scene, std::forward<AddFunction>(add));
    }
}

pt::Texture loadDummyTexture(const std::string& filepath, VkFormat format) {
    pt::Texture tex;
    int texWidth = 0;
    int texHeight = 0;
    int texChannels = 0;
    const std::string imagePath = (std::filesystem::path(PT_DUMMY_IMAGES_DIR) / filepath).string();

    // AI CODE
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
        stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha),
        stbi_image_free);

    if (texWidth <= 0 || texHeight <= 0 ||
        static_cast<size_t>(texWidth) >
        std::numeric_limits<size_t>::max() / 4 / static_cast<size_t>(texHeight)) {
        throw std::runtime_error("invalid dimensions for dummy image " + imagePath);
    }

    const size_t byteCount = static_cast<size_t>(texWidth) * texHeight * 4;
    // 

    tex.width = texWidth;
    tex.height = texHeight;
    tex.format = format;
    tex.pixels.assign(decoded.get(), decoded.get() + byteCount);
    tex.mipLevels = 1;
    tex.sampler.magFilter = VK_FILTER_NEAREST;
    tex.sampler.minFilter = tex.sampler.magFilter;
    tex.sampler.wrapU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    tex.sampler.wrapV = tex.sampler.wrapU;

    return tex;
}

Scene loadGltf(const std::string& path) {
     Scene s;
     
     auto add = [&](Mesh m, glm::mat4 matrix) { s.addInstance(s.addMesh(std::move(m)), matrix); };

     GltfMesh* mesh = gltfload::loadGLTFFile(path);
     for (auto node : mesh->parentNodes) {
         addGLTFMeshToScene(node, &s, add);
     }

     s.textures.push_back(loadDummyTexture("dummyColor.png", VK_FORMAT_R8G8B8A8_SRGB));
     s.textures.push_back(loadDummyTexture("dummyNormal.png", VK_FORMAT_R8G8B8A8_SRGB));
     s.textures.push_back(loadDummyTexture("dummyMetallicRoughness.png", VK_FORMAT_R8G8B8A8_SRGB));

     for (auto& tex : mesh->textures) {
         s.textures.push_back(std::move(tex));
     }

     for (auto& m : mesh->materials) {
         s.addMaterial(m);
     }
     
     s.camera.position = { 0.0f, 1.0f, 3.4f };
     s.camera.yaw = -90.0f;
     s.camera.pitch = 0.0f;
     s.camera.verticalFovDeg = 40.0f;
     s.markDirty(DirtyAll);
     return s;
}

} // namespace pt