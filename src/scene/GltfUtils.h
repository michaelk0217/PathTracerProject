#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "Scene.h"

#include <tiny_gltf.h>

static std::string getFileExtension(const std::string& FileName) {
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

struct GltfTransform {
    GltfTransform* parent;

    glm::vec3 position{ 0.f, 0.f, 0.f };
    glm::quat rotation{ 1.f, 0.f, 0.f, 0.f };
    glm::vec3 scale{ 1.f, 1.f, 1.f };

    glm::mat4 getMatrix() const {
        glm::mat4 ret = glm::translate(glm::mat4(1.0f), position);
        ret *= glm::toMat4(rotation);
        ret = glm::scale(ret, scale);

        if (parent) {
            ret = parent->getMatrix() * ret;
        }

        return ret;
    }
};

struct GltfPrimitive {
    uint32_t materialIndex;

    std::vector<pt::Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct GltfNode {
    uint32_t nodeIndex;
    GltfTransform transform;

    std::vector<GltfPrimitive> primitives;
    std::vector<GltfNode*> children;
    GltfNode* parent;

    glm::mat4 getMatrix() {
        return transform.getMatrix();
    }
};

struct GltfTexture {
    uint32_t component;
    uint32_t width;
    uint32_t height;

    std::string texName;

    std::vector<unsigned char> textureData;
};

struct GltfMesh {
    std::string meshName;
    uint32_t numNodes = 0;

    std::vector<GltfNode*> parentNodes;

    // materials and textures
    std::vector<pt::Material> materials;
    std::vector<GltfTexture> textures;
};

namespace gltfload {
    extern void loadNode(const tinygltf::Model* model, const tinygltf::Node& nodeIn, GltfMesh* mesh, int32_t nodeIndex, GltfNode* parent);
    extern GltfMesh* loadGLTFFile(const std::string& filepath);
}