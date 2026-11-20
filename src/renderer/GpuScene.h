#pragma once

#include <vector>

#include <scene/Scene.h>
#include <shared/GpuStructs.h>
#include <vk/AccelStruct.h>
#include <vk/Resources.h>
#include <vk/Bindless.h>

#include "vk/Sampler.h"

namespace pt
{
namespace vk { class Context; }

class GpuScene : NonCopyable
{
public: 
	// explicit GpuScene(const vk::Context& ctx);
	explicit GpuScene(const vk::Context& ctx, vk::BindlessDescriptors& bindless);
	~GpuScene();


	// uploads whatever dirty flags say its changed and then clears them
	bool sync(Scene& scene);

	VkDeviceAddress meshInfoAddress() const { return m_meshInfos.address(); }
	VkDeviceAddress materialAddress() const { return m_materials.address(); }

	u32 meshCount() const { return static_cast<u32>(m_vertexOffset.size()); }
	vk::BlasInput blasInput(u32 meshIndex) const;

	VkDeviceAddress blasAddress(u32 meshIndex) const { return m_blas[meshIndex].address; }   // for 4d
	
	VkAccelerationStructureKHR tlas() const { return m_tlas.handle; }
	bool consumeTlasChanged() { const bool c = m_tlasChanged; m_tlasChanged = false; return c; }


private:
	void uploadTextures(const Scene& scene);
	void uploadGeometry(const Scene& scene); // vert, indices, per-mesh offsets
	void uploadMaterials(const Scene& scene);
	void uploadMeshInfos(const Scene& scene); // one GpuMeshInfo per instance
	

	void buildBlases();
	void buildTlas(const Scene& scene);
	void destroyBlases();

	const vk::Context& m_ctx;
	
	vk::Buffer m_vertices; // gpu::GpuVertex[] (all meshes back to back)
	vk::Buffer m_indices; // u32[]
	vk::Buffer m_meshInfos; // gpu::GpuMeshInfo[instanceCount]
	vk::Buffer m_materials; // gpu::GpuMaterial[]
	
	std::vector<u32> m_vertexOffset, m_vertexCount;
	std::vector<u32> m_indexOffset, m_triangleCount;

	std::vector<vk::AccelStruct> m_blas; // one per mesh
	vk::AccelStruct m_tlas;
	bool m_tlasChanged = false;

	vk::BindlessDescriptors& m_bindless;
	std::vector<vk::Image2D> m_textures;
	std::vector<pt::vk::Sampler> m_samplers;
};


}