//> includes
#include "vk_engine.h"

// SDL
#include <SDL.h>
#include <SDL_vulkan.h>

// libs
#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>

// VK Bootstrap
#include "VkBootstrap.h"

// utilities
#include <chrono>
#include <thread>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

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

    _init._window = SDL_CreateWindow(
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

        // Make sure GPU has stopped doing its thing
        vkDeviceWaitIdle(_init._vkbDevice.device);

        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
			// Destroy command pool
            vkDestroyCommandPool(_init._vkbDevice.device, _frames[i]._commandPool, nullptr);

            // Destroy sync objects
            vkDestroyFence(_init._vkbDevice.device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_init._vkbDevice.device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_init._vkbDevice.device, _frames[i]._swapchainSemaphore, nullptr);

			// flush the deletion queue for the frame
			_frames[i]._deletionQueue.flush();
        }

		// Flush the main deletion queue
		_mainDeletionQueue.flush();

		// destroy the swapchain
        destroySwapchain();

		// destroy the logical device
        vkb::destroy_device(_init._vkbDevice);
        
		// destroy the surface
        vkb::destroy_surface(_init._vkbInstance, _init._surface);

		// destroy_instance also destroys the debug messenger
		vkb::destroy_instance(_init._vkbInstance);

		// destroy the window
        SDL_DestroyWindow(_init._window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    
    // wait until the gpu has finished rendering the last frame. Timeout of 1 second

    VK_CHECK(vkWaitForFences(_init._vkbDevice.device, 1, &getCurrentFrame()._renderFence, 
        true, 1000000000));
    
	// reset the deletion queue for this frame
	getCurrentFrame()._deletionQueue.flush();
    
	// reset the fence to the unsignaled state
    VK_CHECK(vkResetFences(_init._vkbDevice.device, 1, &getCurrentFrame()._renderFence));

    //request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_init._vkbDevice.device, _init._vkbSwapchain.swapchain, 
        1000000000, getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex));

    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    //begin the command buffer recording
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _drawExtent.width = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_GENERAL);

    drawBackground(cmd);

    //transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, 
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vkutil::transition_image(cmd, _init._swapchainImages[swapchainImageIndex], 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(cmd, _drawImage.image, 
        _init._swapchainImages[swapchainImageIndex], _drawExtent, _init._swapchainExtent);

    // set swapchain image layout to Present so we can show it on the screen
    vkutil::transition_image(cmd, _init._swapchainImages[swapchainImageIndex], 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Prepare the submission to the queue
    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame()._swapchainSemaphore);

    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, getCurrentFrame()._renderFence));

    // Prepare present

    // Could be made into an initializer
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_init._vkbSwapchain.swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &getCurrentFrame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    // Increase the number of frames drawn
    _frameNumber++;




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

    uint32_t apiVer;

	// check if the system supports at least Vulkan 1.3
	vkEnumerateInstanceVersion(&apiVer);
    if (VK_VERSION_MAJOR(apiVer) < 1 || (VK_VERSION_MAJOR(apiVer) == 1 && VK_VERSION_MINOR(apiVer) < 3)) {
        fmt::print("Vulkan 1.3 not supported, found version {}.{}.{}\n",
            VK_VERSION_MAJOR(apiVer),
            VK_VERSION_MINOR(apiVer),
            VK_VERSION_PATCH(apiVer));
        exit(-1);
    }
    else {
        fmt::print("Found Vulkan API version {}.{}.{}\n",
            VK_VERSION_MAJOR(apiVer),
            VK_VERSION_MINOR(apiVer),
            VK_VERSION_PATCH(apiVer));
	}

	// Use Vulkan Bootstrap to create the instance
	vkb::InstanceBuilder builder;

	// Create the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Vulkan Engine")
        .request_validation_layers(enableValidationLayers)
        .require_api_version(1, 3, 0)
        .use_default_debug_messenger()
		.build();
    
	vkb::Instance vkb_inst = inst_ret.value();
	// store the instance and debug messenger handles
    _init._vkbInstance = vkb_inst;
    //init._instance = vkb_inst.instance;
    _init._debugMessenger = vkb_inst.debug_messenger;

    /**********************************************************
	* VULKAN SURFACE CREATION AND DEVICE SELECTION
    *
	* This section creates the Vulkan surface using SDL and selects a suitable physical device (GPU).
    **********************************************************/

	// Create the Vulkan surface using SDL, throw error if it fails
    if (SDL_Vulkan_CreateSurface(_init._window, _init._vkbInstance.instance, &_init._surface) == SDL_FALSE)
    {
		fmt::print("Failed to create Vulkan surface.\n");
		exit(-1);
    }


    /**********************************************************
	* PHYSICAL AND LOGICAL DEVICE SELECTION
    *
	* This section selects a suitable physical device (GPU) and creates a logical device.
    **********************************************************/

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

    

   auto physicalDevices = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(_init._surface)
        .prefer_gpu_device_type()
        .select_devices();

   if (!physicalDevices) {
       fmt::print("Failed to select physical device: {}\n", physicalDevices.error().message());
       exit(-1);
   }

   vkb::PhysicalDevice selectedDevice;

   // Try to find a discrete GPU
   for (auto& pd : physicalDevices.value()) {
       fmt::print("Available GPU: {}\n", pd.name);
       if (pd.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
           selectedDevice = pd;
           break;
	   }
   }

   fmt::println("");

   if (selectedDevice == nullptr) {
	   fmt::print("No discrete GPU found, using first available GPU: {}\n", physicalDevices.value()[0].name);
	   selectedDevice = physicalDevices.value()[0];
   }

	fmt::print("Selected GPU: {}\n", selectedDevice.properties.deviceName);

	// Create the logical device
	vkb::DeviceBuilder deviceBuilder{ selectedDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle
	//init._device = vkbDevice.device;
    _init._vkbDevice = vkbDevice;
    _init._vkbPhysicalDevice = selectedDevice;

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();


	/**********************************************************
	* MEMORY ALLOCATOR CREATION
    * 
	* This section creates the Vulkan Memory Allocator (VMA) for efficient memory management.
	***********************************************************/

	// Create the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = selectedDevice.physical_device;
	allocatorInfo.device = vkbDevice.device;
	allocatorInfo.instance = vkb_inst.instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_allocator);
        });

}

void VulkanEngine::initSwapchain()
{
	createSwapchain(_windowExtent.width, _windowExtent.height);

	// create the draw image
    VkExtent3D drawImageExtent = {
        _init._swapchainExtent.width,
        _init._swapchainExtent.height,
        1
	};

	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages =         VK_IMAGE_USAGE_TRANSFER_SRC_BIT  
                                              | VK_IMAGE_USAGE_TRANSFER_DST_BIT  
                                              | VK_IMAGE_USAGE_STORAGE_BIT 
                                              | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info =   vkinit::image_create_info(_drawImage.imageFormat, 
                                    drawImageUsages, drawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, 
        &_drawImage.image, &_drawImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, 
        _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_init._vkbDevice.device, &rview_info, nullptr, &_drawImage.imageView));

    //add to deletion queues
    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_init._vkbDevice.device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        });



}
void VulkanEngine::initCommands()
{
    
    // Create command pool for commands submitted to the graphics queue
    // We also want the pool to allow for resetting of individual command buffers

    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateCommandPool(_init._vkbDevice.device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        // Allocate the default command buffer that we will use for rendering

        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_init._vkbDevice.device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
    }


}
void VulkanEngine::initSyncStructures()
{
    
    //create syncronization structures
    //one fence to control when the gpu has finished rendering the frame,
    //and 2 semaphores to syncronize rendering with swapchain
    //we want the fence to start signalled so we can wait on it on the first frame

    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {

        VK_CHECK(vkCreateFence(_init._vkbDevice.device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_init._vkbDevice.device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_init._vkbDevice.device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

    }

}

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height)
{

    /**********************************************************
	* SWAPCHAIN CREATION
    *
	* This section creates the swapchain using VkBootstrap, specifying the desired format,
    **********************************************************/

	// Use Vulkan Bootstrap to create the swapchain
	vkb::SwapchainBuilder swapchainBuilder{ _init._vkbPhysicalDevice, _init._vkbDevice.device, _init._surface };

    _init._swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

    auto vkbSwapchain = swapchainBuilder 
		.set_desired_format(VkSurfaceFormatKHR{ .format = _init._swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) // 8-bit SRGB
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // vsync
        .set_desired_extent(width, height) // use window size
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT) // we want to blit to the swapchain
        .build()
        .value();

    _init._vkbSwapchain = vkbSwapchain;
    _init._swapchainExtent = vkbSwapchain.extent;

	// store swapchain and its related images
	//init._swapchain = vkbSwapchain.swapchain;
    _init._swapchainImages = vkbSwapchain.get_images().value();
    _init._swapchainImageViews = vkbSwapchain.get_image_views().value();


}

void VulkanEngine::destroySwapchain()
{
    _init._vkbSwapchain.destroy_image_views(_init._swapchainImageViews);
    vkb::destroy_swapchain(_init._vkbSwapchain);


	
}

void VulkanEngine::drawBackground(VkCommandBuffer cmd)
{
    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearColorValue clearValue;
    float flash = std::abs(std::sin(_frameNumber / 120.f));
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    //clear image
    vkCmdClearColorImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}

// end of vk_engine.cpp