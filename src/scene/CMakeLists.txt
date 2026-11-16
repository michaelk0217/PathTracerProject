# scene: CPU-side source of truth. Must NOT link Vulkan - the build enforces it.
pt_add_module(pt_scene)
target_link_libraries(pt_scene PUBLIC pt_core PUBLIC third_party::tinygltf)