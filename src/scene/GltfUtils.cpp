#include "GltfUtils.h"

namespace gltfload {
    void loadNode(const tinygltf::Model* model, const tinygltf::Node& nodeIn, GltfMesh* mesh, int32_t nodeIndex, GltfNode* parent) {
        GltfNode* node = new GltfNode();
        node->parent = parent;
        node->nodeIndex = nodeIndex;
        if (parent) node->transform.parent = &parent->transform;

        if (nodeIn.matrix.size() == 16) {
            glm::mat4 m = glm::make_mat4x4(nodeIn.matrix.data());

            glm::vec3 skew;
            glm::vec4 perspective;

            glm::decompose(m, node->transform.scale, node->transform.rotation, node->transform.position, skew, perspective);
        }
        else {
            if (nodeIn.translation.size() == 3) {
                node->transform.position = glm::make_vec3(nodeIn.translation.data());
            }
            if (nodeIn.rotation.size() == 4) {
                node->transform.rotation = glm::make_quat(nodeIn.rotation.data());
            }
            if (nodeIn.scale.size() == 3) {
                node->transform.scale = glm::make_vec3(nodeIn.scale.data());
            }
        }

        if (nodeIn.children.size() > 0) {
            for (size_t i = 0; i < nodeIn.children.size(); i++) {
                mesh->numNodes++;
                loadNode(model, model->nodes[nodeIn.children[i]], mesh, nodeIn.children[i], node);
            }
        }

        if (nodeIn.mesh > -1) {
            const tinygltf::Mesh gltfMesh = model->meshes[nodeIn.mesh];
            for (size_t i = 0; i < gltfMesh.primitives.size(); i++) {
                const tinygltf::Primitive& gltfPrim = gltfMesh.primitives[i];
                GltfPrimitive p;
                p.materialIndex = (gltfPrim.material < 0 ? 0 : gltfPrim.material);
                if (gltfPrim.material < 0) p.materialIndex = 0; // first material should ALWAYS be a dummy mat

                uint32_t currentNumIndices = 0;
                uint32_t currentNumVertices = 0;

                // FOR VERTICES
                const float* positionBuff = nullptr;
                const float* normalsBuff = nullptr;
                const float* uvBuff = nullptr;

                if (gltfPrim.attributes.find("POSITION") != gltfPrim.attributes.end()) {
                    const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("POSITION")->second];
                    const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                    positionBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    currentNumVertices = static_cast<uint32_t>(accessor.count);
                }
                if (gltfPrim.attributes.find("NORMAL") != gltfPrim.attributes.end()) {
                    const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                    normalsBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }
                if (gltfPrim.attributes.find("TEXCOORD_0") != gltfPrim.attributes.end()) {
                    const tinygltf::Accessor& accessor = model->accessors[gltfPrim.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                    uvBuff = reinterpret_cast<const float*>(&(model->buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                for (size_t vert = 0; vert < currentNumVertices; vert++) {
                    pt::Vertex v{};

                    v.position = glm::make_vec3(&positionBuff[vert * 3]);
                    v.normal = glm::normalize(glm::vec3(normalsBuff ? glm::make_vec3(&normalsBuff[vert * 3]) : glm::vec3(0.0f)));
                    v.uv = uvBuff ? glm::make_vec2(&uvBuff[vert * 2]) : glm::vec3(0.0f);

                    p.vertices.push_back(std::move(v));
                }

                const tinygltf::Accessor& accessor = model->accessors[gltfPrim.indices];
                const tinygltf::BufferView& view = model->bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model->buffers[view.buffer];

                switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                    for (size_t index = 0; index < accessor.count; index++) {
                        p.indices.push_back(buf[index]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                    const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                    for (size_t index = 0; index < accessor.count; index++) {
                        p.indices.push_back(buf[index]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                    const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
                    for (size_t index = 0; index < accessor.count; index++) {
                        p.indices.push_back(buf[index]);
                    }
                    break;
                }
                default:
                    std::cout << "index component type not supported" << std::endl;
                    throw std::runtime_error("index component type not supported");
                }

                node->primitives.push_back(std::move(p));
            }
        }

        if (parent) {
            parent->children.push_back(node);
        }
        else {
            mesh->parentNodes.push_back(node);
        }

        mesh->numNodes++;
    }

    GltfMesh* loadGLTFFile(const std::string& filepath) {
        GltfMesh* mesh = new GltfMesh();

        tinygltf::Model* model;
        model = new tinygltf::Model();
        tinygltf::TinyGLTF gltfContext;
        std::string error, warning;

        bool fileLoaded = false;
        if (getFileExtension(filepath) == "glb") {
            fileLoaded = gltfContext.LoadBinaryFromFile(model, &error, &warning, filepath);
        }
        else {
            fileLoaded = gltfContext.LoadASCIIFromFile(model, &error, &warning, filepath);
        }

        if (error.size() > 0) {
            std::cout << "Loading Errors: " << error << std::endl;
        }
        if (warning.size() > 0) {
            std::cout << "Loading Warnings: " << warning << std::endl;
        }

        if (!fileLoaded) {
            std::cout << "Failed to load glTF file: " << filepath << std::endl;
            throw std::runtime_error("[LIGHTLOADER] Failed to load glTF file: " + filepath);
        }

        const tinygltf::Scene& scene = model->scenes[model->defaultScene];
        for (const auto& nodeIndex : scene.nodes) {
            const tinygltf::Node& node = model->nodes[nodeIndex];
            gltfload::loadNode(model, node, mesh, -1, nullptr);
        }

        delete model;
        return mesh;
    }
}