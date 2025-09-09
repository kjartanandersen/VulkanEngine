// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

class VulkanEngine {
public:

	/****************************************************************
	* ENGINE VARIABLES
	* 
	* This section contains all the variables used by the Vulkan engine.
	*****************************************************************/

	bool _isInitialized{ false };								// true if the engine is initialized	
	int _frameNumber{ 0 };										// current frame number		
	bool stop_rendering{ false };								// true if the engine should stop rendering
	VkExtent2D _windowExtent{ 1700 , 900 };						// window extent (width and height)

	VkInstance _instance;										// Vulkan instance 
	VkDebugUtilsMessengerEXT _debugMessenger;					// debug messenger for validation layers
	VkPhysicalDevice _physicalDevice;							// physical device (GPU)
	VkDevice _device;											// Vulkan logical device for commands 
	VkSurfaceKHR _surface;										// Vulkan window surface


	struct SDL_Window* _window{ nullptr }; 						// window handle 

	static VulkanEngine& Get(); 								// get the singleton instance of the engine


	/**************************************************************
	* ENGINE LIFECYCLE
	* 
	* This section covers the lifecycle of the Vulkan engine, including initialization,
	* cleanup, and the main draw loop.
	* 
	****************************************************************/

	// initializes the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:

	/***************************************************************
	* ENGINE PRIVATE METHODS
	* 
	* These methods are responsible for initializing various Vulkan components.
	***************************************************************/

	// initializes the Vulkan library and prepares it for drawing
	void initVulkan();										
	// initializes the SDL library and creates a window
	void initSwapchain();							
	// initializes the command buffers
	void initCommands();
	// initializes synchronization structures
	void initSyncStructures();
};
