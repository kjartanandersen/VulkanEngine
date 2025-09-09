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

	static VulkanEngine& Get(); 								// get the singleton instance of the engine


	/**************************************************************
	* ENGINE LIFECYCLE
	* 
	* This section covers the lifecycle of the Vulkan engine, including initialization,
	* cleanup, and the main draw loop.
	* 
	****************************************************************/

	/// <summary>
	/// Initializes the system or component.
	/// </summary>
	void init();

	/// <summary>
	/// Performs cleanup operations and releases any allocated resources.
	/// </summary>
	void cleanup();

	/// <summary>
	/// Performs a drawing operation.
	/// </summary>
	void draw();

	/// <summary>
	/// Executes the main operation or process.
	/// </summary>
	void run();

private:


	/// <summary>
	/// Represents the initialization state of the Vulkan engine
	/// </summary>
	struct Init {
		//VkInstance _instance;										// Vulkan instance 
		vkb::Instance _vkbInstance;								// Vulkan Bootstrap instance wrapper
		VkDebugUtilsMessengerEXT _debugMessenger;					// debug messenger for validation layers
		VkPhysicalDevice _physicalDevice;							// physical device (GPU)
		//VkDevice _device;											// Vulkan logical device for commands 
		vkb::Device _vkbDevice;									// Vulkan Bootstrap device wrapper
		VkSurfaceKHR _surface;										// Vulkan window surface

		//VkSwapchainKHR _swapchain;									// swapchain for presenting images to the window
		vkb::Swapchain _vkbSwapchain;							// Vulkan Bootstrap swapchain wrapper
		VkFormat _swapchainImageFormat;								// format of the swapchain images

		std::vector<VkImage> _swapchainImages;						// swapchain images
		std::vector<VkImageView> _swapchainImageViews;				// image views for the swapchain images
		VkExtent2D _swapchainExtent;								// extent of the swapchain images

		struct SDL_Window* _window{ nullptr }; 						// window handle 

	} _init;

	/***************************************************************
	* ENGINE PRIVATE METHODS
	* 
	* These methods are responsible for initializing various Vulkan components.
	***************************************************************/

	/// <summary>
	/// Initializes the Vulkan graphics API for use in the application.
	/// </summary>
	void initVulkan();			

	/// <summary>
	/// Initializes the swapchain and related resources.
	/// </summary>
	void initSwapchain();							
	
	/// <summary>
	/// Initializes the command pool and command buffers.
	/// </summary>
	void initCommands();
	
	/// <summary>
	/// Initializes synchronization structures.
	/// </summary>
	void initSyncStructures();

	/// <summary>
	/// Creates a swapchain with the specified width and height.
	/// </summary>
	/// <param name="width">The width of the swapchain in pixels.</param>
	/// <param name="height">The height of the swapchain in pixels.</param>
	void createSwapchain(uint32_t width, uint32_t height);

	/// <summary>
	/// Destroys the current swapchain and releases its associated resources.
	/// </summary>
	void destroySwapchain();

}; // end of vk_engine.h
