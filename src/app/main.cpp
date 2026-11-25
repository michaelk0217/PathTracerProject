// Dependency smoke test - proves SDL3, volk, VMA headers, GLM and the Vulkan loader all work.
// Replace this file with your real entry point once you start writing the app.
//#include <cstdio>
//
//#include <volk.h>
//#include <vk_mem_alloc.h>   // header only here; define VMA_IMPLEMENTATION in exactly one .cpp later
//#include <glm/glm.hpp>
//#include <SDL3/SDL.h>
//#include <SDL3/SDL_main.h>
//#include <SDL3/SDL_vulkan.h>



#include <SDL3/SDL_main.h>
#include "App.h"

int main(int argc, char** argv) {
   
    pt::App app;
    return app.run();



    //if (volkInitialize() != VK_SUCCESS) {
    //    std::printf("volkInitialize failed: no Vulkan loader / driver found\n");
    //    return 1;
    //}
    //const uint32_t v = volkGetInstanceVersion();
    //std::printf("Vulkan loader %u.%u.%u\n", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));

    //const glm::vec3 a(1.0f, 2.0f, 3.0f);
    //std::printf("GLM dot = %.1f\n", glm::dot(a, a));
    //std::printf("VMA header version %d\n", VMA_VULKAN_VERSION);

    //if (!SDL_Init(SDL_INIT_VIDEO)) {
    //    std::printf("SDL_Init failed: %s\n", SDL_GetError());
    //    return 1;
    //}
    //SDL_Window* window = SDL_CreateWindow("PathTracer - dependency check (Esc to quit)", 1280, 720,
    //                                      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    //if (!window) {
    //    std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    //    SDL_Quit();
    //    return 1;
    //}
    //Uint32 extCount = 0;
    //SDL_Vulkan_GetInstanceExtensions(&extCount);
    //std::printf("SDL %d.%d.%d ok, %u Vulkan instance extensions required by the window\n", SDL_MAJOR_VERSION,
    //            SDL_MINOR_VERSION, SDL_MICRO_VERSION, extCount);

    //for (bool running = true; running;) {
    //    SDL_Event e;
    //    while (SDL_PollEvent(&e)) {
    //        if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) running = false;
    //    }
    //    SDL_Delay(16);
    //}
    //SDL_DestroyWindow(window);
    //SDL_Quit();
    //return 0;
}
