
#pragma once 

#include <vulkan/vulkan.h>

namespace vkutil {

	/// <summary>
	/// Transitions an image from one layout to another using a command buffer.
	/// </summary>
	/// <param name="cmd"> The command buffer to record the transition into.</param>
	/// <param name="image"> The image to transition.</param>
	/// <param name="currentLayout"> The current layout of the image.</param>
	/// <param name="newLayout"> The new layout to transition the image to.</param>
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
	
	/// <summary>
	/// Copies the contents of one image to another image using a command buffer.
	/// </summary>
	/// <param name="cmd"> The command buffer to record the copy into.</param>
	/// <param name="source"> The source image to copy from.</param>
	/// <param name="destination"> The destination image to copy to.</param>
	/// <param name="srcSize"> The size of the source image.</param>
	/// <param name="dstSize"> The size of the destination image.</param>
	void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

}; // end of vk_images.h
