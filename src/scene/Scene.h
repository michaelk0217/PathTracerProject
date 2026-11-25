#pragma once

#include <string>
#include <vector>

#include <core/Types.h>
#include <scene/Camera.h>

#include <iostream>
#include <vulkan/vulkan.hpp>

namespace pt
{
// cpu side of truth - GpuScene.h mirrors this on gpu
struct Vertex
{
	vec3 position;
	vec3 normal;
	vec2 uv;
};

struct Mesh {
	std::string name;
	std::vector<Vertex> vertices;
	std::vector<u32> indices;
	u32 materialIndex = 0;
};

struct Material
{
	std::string name;
	vec3 baseColor{ 1.f };
	float roughness = 1.0f;
	vec3 emission { 0.0f };
	float metallic = 0.0f;
	float ior = 1.5f;
	float transmission = 0.0f;
	i32 baseColorTexture = -1; // index into Scene::texturePaths, -1 = none
	i32 normalTexture = -1;
	i32 metallicRoughnessTexture = -1;
};

struct SamplerDesc
{
	VkFilter magFilter;
	VkFilter minFilter;
	VkSamplerAddressMode wrapU;
	VkSamplerAddressMode wrapV;
};

struct Texture
{
	u32 width = 0, height = 0;
	u32 mipLevels = 1;
	std::vector<u8> pixels;
	SamplerDesc sampler;
	VkFormat format;
};

struct Instance
{
	u32 meshIndex = 0;
	mat4 transform{ 1.0f };
};

enum DirtyFlags : u32
{
	DirtyNone = 0,
	DirtyGeometry = 1u << 0, // meshes changed -> rebuild BLAS + vertex/index buffs
	DirtyInstances = 1u << 1, // transforms changed -> refit/rebuild TLAS
	DirtyMaterials = 1u << 2, //material params changed, reupload material buffer
	DirtyTextures = 1u << 3, // textures changed, upload textures
	DirtyAll = 0xFFFFFFFFu,
};

class Scene
{
public:
	std::vector<Mesh> meshes;
	std::vector<Material> materials;
	std::vector<Instance> instances;
	std::vector<Texture> textures;
	Camera camera;
	i32 lightMaterial = -1;
	vec3 lightPosition{0.0f};
	float lightWidth = 0.0f;
	vec3 directionToSun{0.0f};
	vec3 sunIrradiance{0.0f};
	vec3 skyRadiance{ 0.0f };

	void markDirty(u32 flags) { m_dirty |= flags; }
	u32 dirtyFlags() const { return m_dirty; }
	void clearDirty() { m_dirty = DirtyNone; }

	u32 addMaterial(Material m);
	u32 addMesh(Mesh m); // returns mesh idx
	void addInstance(u32 meshIndex, const mat4& transform = mat4(1.0f));


private:
	u32 m_dirty = DirtyAll;
};

}