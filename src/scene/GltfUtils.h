#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "Scene.h"
#include <unordered_set>

#include <tiny_gltf.h>

constexpr int DUMMY_COLOR_TEX_INDEX = 0;
constexpr int DUMMY_NORMAL_TEX_INDEX = 1;
constexpr int DUMMY_METALROUGH_TEX_INDEX = 2;

enum L_TEXTURE_TYPE {
    L_RGB,
    L_UNORM
};

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

struct GltfMesh {
    std::string meshName;
    uint32_t numNodes = 0;

    std::vector<GltfNode*> parentNodes;

    std::vector<pt::Material> materials;
    std::vector<pt::Texture> textures;
};

namespace gltfload {
    extern void loadNode(const tinygltf::Model* model, const tinygltf::Node& nodeIn, GltfMesh* mesh, int32_t nodeIndex, GltfNode* parent);
    extern GltfMesh* loadGLTFFile(const std::string& filepath);
}