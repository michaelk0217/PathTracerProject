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

} // namespace

Scene makeCornellBox() {
    Scene s;
    const u32 white = s.addMaterial({ .name = "white", .baseColor = {0.73f, 0.73f, 0.73f} });
    const u32 red = s.addMaterial({ .name = "red", .baseColor = {0.65f, 0.05f, 0.05f} });
    const u32 green = s.addMaterial({ .name = "green", .baseColor = {0.12f, 0.45f, 0.15f} });
    const u32 light = s.addMaterial({ .name = "light", .baseColor = vec3(0.0f), .emission = {17.0f, 12.0f, 4.0f} });
    

    // Room spans [-1,1] x [0,2] x [-1,1], open towards +Z (camera side).
    auto add = [&](Mesh m) { s.addInstance(s.addMesh(std::move(m))); };
    add(makeQuad("floor", { -1, 0, -1 }, { 0, 0, 2 }, { 2, 0, 0 }, white));
    add(makeQuad("ceiling", { -1, 2, -1 }, { 2, 0, 0 }, { 0, 0, 2 }, white));
    add(makeQuad("back", { -1, 0, -1 }, { 2, 0, 0 }, { 0, 2, 0 }, white));
    add(makeQuad("left", { -1, 0, -1 }, { 0, 2, 0 }, { 0, 0, 2 }, red));
    add(makeQuad("right", { 1, 0, -1 }, { 0, 0, 2 }, { 0, 2, 0 }, green));
    add(makeQuad("light", { -0.25f, 1.98f, -0.25f }, { 0.5f, 0, 0 }, { 0, 0, 0.5f }, light));
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
        currentMesh.materialIndex = 0;
        add(currentMesh, currentMatrix);
    }

    for (auto child : currentNode->children) {
        addGLTFMeshToScene(child, scene, std::forward<AddFunction>(add));
    }
}

Scene loadGltf(const std::string& path) {
     Scene s;
     const u32 white = s.addMaterial({ .name = "white", .baseColor = {0.73f, 0.73f, 0.73f} });
     
     auto add = [&](Mesh m, glm::mat4 matrix) { s.addInstance(s.addMesh(std::move(m)), matrix); };

     GltfMesh* mesh = gltfload::loadGLTFFile(path);
     for (auto node : mesh->parentNodes) {
         addGLTFMeshToScene(node, &s, add);
     }
     
     s.camera.position = { 0.0f, 1.0f, 3.4f };
     s.camera.yaw = -90.0f;
     s.camera.pitch = 0.0f;
     s.camera.verticalFovDeg = 40.0f;
     s.markDirty(DirtyAll);
     return s;
}

} // namespace pt