# ---------------------------------------------------------------------------
# Third-party dependencies - taken from the installed Vulkan SDK
#
#   Vulkan headers + loader lib : SDK core               (FindVulkan)
#   volk                        : SDK optional "Volk"     (Include/Volk, Lib/volk.lib)
#   VMA                         : SDK optional "VMA"      (Include/vma/vk_mem_alloc.h)
#   GLM                         : SDK optional "GLM"      (Include/glm)
#   SDL3                        : SDK optional "SDL"      (Include/SDL3, Lib/SDL3.lib, Bin/SDL3.dll)
#   Slang                       : SDK core               (Bin/slangc.exe, optional runtime lib)
#   Dear ImGui                  : NOT in the SDK -> FetchContent (PT_WITH_IMGUI)
#
# Missing optional components? Run "maintenancetool.exe" in the SDK folder
# (or re-run the installer) and tick Volk / VMA / GLM / SDL.
#
# Targets provided to the rest of the build:
#   Vulkan::Headers  volk::volk  GPUOpen::VMA  glm::glm  pt::SDL3  [imgui]  [slang::runtime]
# ---------------------------------------------------------------------------

# --- Locate the SDK ---------------------------------------------------------
if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR
        "VULKAN_SDK environment variable is not set.\n"
        "Install the Vulkan SDK (it sets VULKAN_SDK), then restart Visual Studio / your terminal.")
endif()
file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" PT_VULKAN_SDK)

# Windows SDK uses Include/Lib/Bin, Linux/macOS use include/lib/bin.
set(_sdk_inc ${PT_VULKAN_SDK}/Include ${PT_VULKAN_SDK}/include)
set(_sdk_lib ${PT_VULKAN_SDK}/Lib ${PT_VULKAN_SDK}/lib)
set(_sdk_bin ${PT_VULKAN_SDK}/Bin ${PT_VULKAN_SDK}/bin)
set(_sdk_cmake ${PT_VULKAN_SDK}/cmake ${PT_VULKAN_SDK}/Lib/cmake ${PT_VULKAN_SDK}/lib/cmake)

function(pt_require var what component)
    if(NOT ${var})
        message(FATAL_ERROR
            "${what} not found in the Vulkan SDK (${PT_VULKAN_SDK}).\n"
            "Install the optional SDK component \"${component}\" via the SDK maintenance tool.")
    endif()
endfunction()

# --- Vulkan headers ---------------------------------------------------------
# The loader library is found too, but we never link it: volk loads vulkan-1.dll at runtime.
find_package(Vulkan 1.3 REQUIRED)
message(STATUS "Vulkan SDK ${Vulkan_VERSION}: ${PT_VULKAN_SDK}")

# --- volk -------------------------------------------------------------------
# Preferred: compile the SDK's volk.c ourselves (matches our CRT / Debug-Release settings).
# Fallback : link the SDK's prebuilt volk.lib (volkd.lib for Debug if present).
find_path(PT_VOLK_INCLUDE_DIR volk.h HINTS ${_sdk_inc} PATH_SUFFIXES Volk volk NO_DEFAULT_PATH)
pt_require(PT_VOLK_INCLUDE_DIR "volk.h" "Volk")
find_file(PT_VOLK_SOURCE volk.c HINTS ${PT_VOLK_INCLUDE_DIR} NO_DEFAULT_PATH)

if(PT_VOLK_SOURCE)
    add_library(volk STATIC ${PT_VOLK_SOURCE} ${PT_VOLK_INCLUDE_DIR}/volk.h)
    target_include_directories(volk SYSTEM PUBLIC ${PT_VOLK_INCLUDE_DIR})
    target_link_libraries(volk PUBLIC Vulkan::Headers)
    if(NOT WIN32)
        target_link_libraries(volk PRIVATE ${CMAKE_DL_LIBS})
    endif()
    set_target_properties(volk PROPERTIES FOLDER "ThirdParty")
    set(_volk_kind "compiled from SDK source")
else()
    find_library(PT_VOLK_LIBRARY NAMES volk HINTS ${_sdk_lib} NO_DEFAULT_PATH)
    find_library(PT_VOLK_LIBRARY_DEBUG NAMES volkd HINTS ${_sdk_lib} NO_DEFAULT_PATH)
    pt_require(PT_VOLK_LIBRARY "volk library" "Volk")
    if(NOT PT_VOLK_LIBRARY_DEBUG)
        set(PT_VOLK_LIBRARY_DEBUG ${PT_VOLK_LIBRARY})
    endif()
    add_library(volk INTERFACE)
    target_include_directories(volk SYSTEM INTERFACE ${PT_VOLK_INCLUDE_DIR})
    target_link_libraries(volk INTERFACE Vulkan::Headers
        $<$<CONFIG:Debug>:${PT_VOLK_LIBRARY_DEBUG}>
        $<$<NOT:$<CONFIG:Debug>>:${PT_VOLK_LIBRARY}>
        ${CMAKE_DL_LIBS})
    set(_volk_kind "prebuilt SDK library")
endif()
# Every TU sees Vulkan only through volk's function pointers.
target_compile_definitions(volk INTERFACE VK_NO_PROTOTYPES)
add_library(volk::volk ALIAS volk)

# --- Vulkan Memory Allocator (header-only) ----------------------------------
find_path(PT_VMA_INCLUDE_DIR vk_mem_alloc.h HINTS ${_sdk_inc} PATH_SUFFIXES vma VMA NO_DEFAULT_PATH)
pt_require(PT_VMA_INCLUDE_DIR "vk_mem_alloc.h" "Vulkan Memory Allocator")
add_library(vma INTERFACE)
target_include_directories(vma SYSTEM INTERFACE ${PT_VMA_INCLUDE_DIR})
target_link_libraries(vma INTERFACE volk::volk)
# VMA fetches Vulkan entry points at runtime (we hand it volk's vkGet*ProcAddr).
target_compile_definitions(vma INTERFACE VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=1)
add_library(GPUOpen::VMA ALIAS vma)

# --- GLM (header-only) ------------------------------------------------------
find_path(PT_GLM_INCLUDE_DIR glm/glm.hpp HINTS ${_sdk_inc} NO_DEFAULT_PATH)
pt_require(PT_GLM_INCLUDE_DIR "glm/glm.hpp" "GLM")
add_library(glm_headers INTERFACE)
target_include_directories(glm_headers SYSTEM INTERFACE ${PT_GLM_INCLUDE_DIR})
target_compile_definitions(glm_headers INTERFACE
    GLM_FORCE_DEPTH_ZERO_TO_ONE   # Vulkan clip space
    GLM_FORCE_RADIANS
    GLM_ENABLE_EXPERIMENTAL)
add_library(glm::glm ALIAS glm_headers)

# --- SDL3 -------------------------------------------------------------------
# 1) Use the SDK's SDL3 CMake package if it ships one; 2) otherwise import the files by hand.
find_package(SDL3 CONFIG QUIET HINTS ${_sdk_cmake} ${PT_VULKAN_SDK} NO_DEFAULT_PATH)
add_library(pt_sdl3 INTERFACE)
if(TARGET SDL3::SDL3)
    target_link_libraries(pt_sdl3 INTERFACE SDL3::SDL3)
    set(_sdl_kind "SDL3 CMake package ${SDL3_VERSION}")
else()
    find_path(PT_SDL3_INCLUDE_DIR SDL3/SDL.h HINTS ${_sdk_inc} NO_DEFAULT_PATH)
    find_library(PT_SDL3_LIBRARY NAMES SDL3 HINTS ${_sdk_lib} NO_DEFAULT_PATH)   # import lib on Windows
    pt_require(PT_SDL3_INCLUDE_DIR "SDL3/SDL.h" "SDL")
    pt_require(PT_SDL3_LIBRARY "SDL3 library" "SDL")

    add_library(pt_sdl3_imported SHARED IMPORTED GLOBAL)
    set_target_properties(pt_sdl3_imported PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PT_SDL3_INCLUDE_DIR})
    if(WIN32)
        find_file(PT_SDL3_DLL SDL3.dll HINTS ${_sdk_bin} NO_DEFAULT_PATH)
        pt_require(PT_SDL3_DLL "SDL3.dll" "SDL")
        set_target_properties(pt_sdl3_imported PROPERTIES
            IMPORTED_IMPLIB   ${PT_SDL3_LIBRARY}
            IMPORTED_LOCATION ${PT_SDL3_DLL})
    else()
        set_target_properties(pt_sdl3_imported PROPERTIES IMPORTED_LOCATION ${PT_SDL3_LIBRARY})
    endif()
    target_link_libraries(pt_sdl3 INTERFACE pt_sdl3_imported)
    set(_sdl_kind "imported from ${PT_SDL3_LIBRARY}")
endif()
add_library(pt::SDL3 ALIAS pt_sdl3)

# --- Slang ------------------------------------------------------------------
find_program(SLANGC_EXECUTABLE slangc HINTS ${_sdk_bin} NO_DEFAULT_PATH)
if(NOT SLANGC_EXECUTABLE)
    message(WARNING "slangc not found in ${PT_VULKAN_SDK}/Bin - shaders will not be compiled.")
endif()

# Optional: Slang runtime library (for in-app shader hot-reload). The SDK ships a CMake package.
find_package(slang CONFIG QUIET HINTS ${_sdk_cmake} NO_DEFAULT_PATH)
if(TARGET slang::slang)
    add_library(pt_slang_runtime INTERFACE)
    target_link_libraries(pt_slang_runtime INTERFACE slang::slang)
    target_compile_definitions(pt_slang_runtime INTERFACE PT_HAS_SLANG_RUNTIME=1)
    add_library(slang::runtime ALIAS pt_slang_runtime)
endif()

# --- Dear ImGui (not in the SDK) --------------------------------------------
if(PT_WITH_IMGUI)
    include(FetchContent)
    set(PT_IMGUI_TAG "v1.93.0")
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        ${PT_IMGUI_TAG}
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  "__no_cmake__")   # download only; we build it below
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
    target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)
    target_link_libraries(imgui PUBLIC volk::volk pt::SDL3)
    set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")
endif()

# --- TINYGLTF ---------------------------------------------------------------
add_library(tinygltf STATIC ${PROJECT_SOURCE_DIR}/third_party/tinygltf/tinygltf_impl.cpp)
target_include_directories(tinygltf SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/tinygltf)
target_compile_features(tinygltf PUBLIC cxx_std_17)
set_target_properties(tinygltf PROPERTIES FOLDER "ThirdParty")
if(MSVC)
    target_compile_options(tinygltf PRIVATE /W0)
endif()
add_library(third_party::tinygltf ALIAS tinygltf)

# --- Summary ----------------------------------------------------------------
message(STATUS "  volk : ${_volk_kind} (${PT_VOLK_INCLUDE_DIR})")
message(STATUS "  VMA  : ${PT_VMA_INCLUDE_DIR}")
message(STATUS "  GLM  : ${PT_GLM_INCLUDE_DIR}")
message(STATUS "  SDL3 : ${_sdl_kind}")
message(STATUS "  slangc: ${SLANGC_EXECUTABLE}")
if(TARGET slang::runtime)
    message(STATUS "  Slang runtime: found (PT_HAS_SLANG_RUNTIME)")
endif()
