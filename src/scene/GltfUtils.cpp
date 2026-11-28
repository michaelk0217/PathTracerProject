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
                p.materialIndex = (gltfPrim.material < 0 ? 0 : gltfPrim.material + 1);
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

    pt::Texture loadTexture(tinygltf::Model* model, VkFormat format, uint32_t imageIndex) {
        tinygltf::Image& curImage = model->images[imageIndex];
        pt::Texture tex{};
        tex.pixels.resize(curImage.width * curImage.height * 4);

        switch (curImage.component) {
        case 4:
            memcpy(tex.pixels.data(), curImage.image.data(), tex.pixels.size());
            break;
        case 3:
        {
            std::vector<unsigned char> rgb;
            rgb.resize(curImage.width * curImage.height * 3);
            memcpy(rgb.data(), curImage.image.data(), rgb.size());
            for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
                tex.pixels[j * 4] = rgb[j * 3];
                tex.pixels[j * 4 + 1] = rgb[j * 3 + 1];
                tex.pixels[j * 4 + 2] = rgb[j * 3 + 2];
                tex.pixels[j * 4 + 3] = 255;
            }
            break;
        }
        case 1:
        {
            std::vector<unsigned char> r;
            r.resize(curImage.width * curImage.height);
            memcpy(r.data(), curImage.image.data(), r.size());
            for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
                tex.pixels[j * 4] = tex.pixels[j * 4 + 1] = tex.pixels[j * 4 + 2] = tex.pixels[j * 4 + 3] = r[j];
            }
            break;
        }
        }

        tex.width = curImage.width;
        tex.height = curImage.height;

        tex.format = format;
        tex.sampler.minFilter = VK_FILTER_LINEAR;
        tex.sampler.magFilter = tex.sampler.minFilter;
        tex.sampler.wrapU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        tex.sampler.wrapV = tex.sampler.wrapU;

        tex.mipLevels = static_cast<uint32_t>(std::floor(std::log2((std::max)(curImage.width, curImage.height)))) + 1;

        return tex;
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
            throw std::runtime_error("Failed to load glTF file: " + filepath);
        }

        const tinygltf::Scene& scene = model->scenes[model->defaultScene];
        for (const auto& nodeIndex : scene.nodes) {
            const tinygltf::Node& node = model->nodes[nodeIndex];
            gltfload::loadNode(model, node, mesh, -1, nullptr);
        }

        std::vector<uint32_t> textureIndices(model->textures.size());
        for (size_t i = 0; i < model->textures.size(); i++) {
            textureIndices[i] = model->textures[i].source;
        }

        std::unordered_set<uint32_t> srgbImages;

        for (size_t i = 0; i < model->materials.size(); i++) {
            pt::Material material;
            material.metallic = 1.0f;
            material.roughness = 1.0f;
            tinygltf::Material gltfMat = model->materials[i];
            if (gltfMat.values.find("baseColorFactor") != gltfMat.values.end()) {
                material.baseColor = glm::make_vec3(gltfMat.values["baseColorFactor"].ColorFactor().data());
            }
            if (gltfMat.values.find("baseColorTexture") != gltfMat.values.end()) {
                const uint32_t image = textureIndices[gltfMat.values["baseColorTexture"].TextureIndex()];
                material.baseColorTexture = image + 3; // +3 for the three dummy images loaded at first
                srgbImages.insert(image);
            }
            else { material.baseColorTexture = DUMMY_COLOR_TEX_INDEX; }

            if (gltfMat.additionalValues.find("normalTexture") != gltfMat.additionalValues.end()) {
                material.normalTexture = textureIndices[gltfMat.additionalValues["normalTexture"].TextureIndex()] + 3;
            }
            else { material.normalTexture = DUMMY_NORMAL_TEX_INDEX; }

            if (gltfMat.values.find("metallicRoughnessTexture") != gltfMat.values.end()) {
                material.metallicRoughnessTexture = textureIndices[gltfMat.values["metallicRoughnessTexture"].TextureIndex()] + 3;
            }
            else { material.metallicRoughnessTexture = DUMMY_METALROUGH_TEX_INDEX; }

            material.metallic = static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor);
            material.roughness = static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor);

            if (gltfMat.emissiveFactor.size() == 3)
            {
                material.emission = glm::vec3(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2]);
            }

            // ai
            if (gltfMat.emissiveTexture.index >= 0) {
                const uint32_t image = textureIndices[gltfMat.emissiveTexture.index];
                material.emissiveTexture = static_cast<uint32_t>(image) + 3;
                srgbImages.insert(image);
            }
            auto strength = gltfMat.extensions.find("KHR_materials_emissive_strength");
            if (strength != gltfMat.extensions.end() && strength->second.Has("emissiveStrength"))
                material.emission *= static_cast<float>(strength->second.Get("emissiveStrength").GetNumberAsDouble());

            mesh->materials.push_back(material);
        }

        for (uint32_t i = 0; i < model->images.size(); i++) {
            const VkFormat format = srgbImages.count(i) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            // VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            mesh->textures.push_back(loadTexture(model, format, i));
        }

        delete model;
        return mesh;
    }
}