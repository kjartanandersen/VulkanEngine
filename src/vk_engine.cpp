//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include <chrono>
#include <thread>

#ifndef NDEBUG
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif


VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

	// initialize vulkan
	initVulkan();

	initSwapchain();

	initCommands();

	initSyncStructures();
	

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {

        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    // nothing yet
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}

void VulkanEngine::initVulkan()
{
	/**********************************************************
	* VULKAN INITIALIZATION
    *
	* This section initializes the Vulkan instance using VkBootstrap, and sets up
	* the debug messenger.
	***********************************************************/

	// Use Vulkan Bootstrap to create the instance
	vkb::InstanceBuilder builder;

	// Create the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Vulkan Engine")
        .request_validation_layers(enableValidationLayers)
        .require_api_version(1, 3, 0)
        .use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = inst_ret.value();
    

	_instance = vkb_inst.instance;
	_debugMessenger = vkb_inst.debug_messenger;

    /**********************************************************
	* VULKAN SURFACE CREATION AND DEVICE SELECTION
    *
	* This section creates the Vulkan surface using SDL and selects a suitable physical device (GPU).
    **********************************************************/

	// Create the Vulkan surface using SDL
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	// Vulkan 1.3 features we want to use
	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	// Vulkan 1.2 features we want to use
	VkPhysicalDeviceVulkan12Features features12 = {};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = VK_TRUE;
	features12.descriptorIndexing = VK_TRUE;

    // Select a GPU 
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1,3)
		.set_required_features_13(features13)
        .set_required_features_12(features12)
		.set_surface(_surface)
        .select()
		.value();

	// Create the logical device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle
	_device = vkbDevice.device;
	_physicalDevice = physicalDevice.physical_device;

}
void VulkanEngine::initSwapchain()
{
    //nothing yet
}
void VulkanEngine::initCommands()
{
    //nothing yet
}
void VulkanEngine::initSyncStructures()
{
    //nothing yet
}

// end of vk_engine.cpp