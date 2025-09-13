// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

/****************************************************************
* ENGINE STRUCTS
* 
* This section contains all the structs used by the Vulkan engine.
****************************************************************/

/// <summary>
/// Represents the initialization state of the Vulkan engine
/// </summary>
struct Init {
	//VkInstance _instance;										// Vulkan instance 
	vkb::Instance				_vkbInstance;					// Vulkan Bootstrap instance wrapper
	VkDebugUtilsMessengerEXT	_debugMessenger;				// debug messenger for validation layers
	vkb::PhysicalDevice			_vkbPhysicalDevice;				// physical device (GPU)
	//VkPhysicalDevice _physicalDevice;							// physical device (GPU)
	//VkDevice _device;											// Vulkan logical device for commands 
	vkb::Device					_vkbDevice;						// Vulkan Bootstrap device wrapper
	VkSurfaceKHR				_surface;						// Vulkan window surface

	//VkSwapchainKHR _swapchain;								// swapchain for presenting images to the window
	vkb::Swapchain				_vkbSwapchain;					// Vulkan Bootstrap swapchain wrapper
	VkFormat					_swapchainImageFormat;			// format of the swapchain images

	std::vector<VkImage>		_swapchainImages;				// swapchain images
	std::vector<VkImageView>	_swapchainImageViews;			// image views for the swapchain images
	VkExtent2D					_swapchainExtent;				// extent of the swapchain images

	struct SDL_Window* _window{ nullptr }; 			// window handle 

};

/// <summary>
/// A queue that holds functions to be executed later, typically for resource cleanup.
/// </summary>
struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};


/// <summary>
/// Represents per-frame data used for rendering and synchronization.
/// </summary>
struct FrameData {
	VkCommandPool		_commandPool;								// command pool for allocating command buffers
	VkCommandBuffer		_mainCommandBuffer;							// primary command buffer for rendering commands

	VkSemaphore			_swapchainSemaphore, _renderSemaphore;		// semaphores for syncronization
	VkFence				_renderFence;								// fence for syncronization
	DeletionQueue		_deletionQueue;								// deletion queue for resource cleanup

};




constexpr int FRAME_OVERLAP = 2;								// number of frames to use in the swapchain


/// <summary>
/// VulkanEngine class encapsulates the Vulkan rendering engine functionality.
/// </summary>
class VulkanEngine {
public:

	/****************************************************************
	* ENGINE VARIABLES
	* 
	* This section contains all the variables used by the Vulkan engine.
	*****************************************************************/

	


	bool		_isInitialized{ false };								// true if the engine is initialized	
	int			_frameNumber{ 0 };										// current frame number		
	bool		stop_rendering{ false };								// true if the engine should stop rendering
	VkExtent2D	_windowExtent{ 1700 , 900 };							// window extent (width and height)

	FrameData	_frames[FRAME_OVERLAP];									// per-frame data

	/// <summary>
	/// Gets the current frame data based on the frame number.
	/// </summary>
	FrameData& getCurrentFrame() { return _frames[_frameNumber % FRAME_OVERLAP]; } 

	VkQueue		_graphicsQueue;										// graphics queue for rendering commands
	uint32_t	_graphicsQueueFamily;								// family index of the graphics queue

	/// <summary>
	/// Retrieves a reference to the singleton instance of VulkanEngine.
	/// </summary>
	/// <returns>A reference to the singleton VulkanEngine instance.</returns>
	static VulkanEngine& Get();


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


	Init			_init;											// initialization state of the engine
	DeletionQueue	_mainDeletionQueue;								// main deletion queue for resource cleanup
	VmaAllocator	_allocator;										// Vulkan memory allocator

	// Draw Resources
	AllocatedImage	_drawImage; 										// image used for drawing
	VkExtent2D		_drawExtent;										// extent of the draw image


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

	/// <summary>
	/// Draws the background using the provided command buffer.
	/// </summary>
	/// <param name="cmd">The command buffer to record the drawing commands into.</param>
	void drawBackground(VkCommandBuffer cmd);

}; // end of vk_engine.h
