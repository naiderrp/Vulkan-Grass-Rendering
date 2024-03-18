#pragma once
#include "config.hpp"
#include "tools.hpp"
#include <ranges>
#include "logging.hpp"
#include "queues.hpp"
#include "swapchain_details.hpp"

#include <set>
#include <fstream>
#include <sstream>

#include "vertex.hpp"
#include "blade.hpp"

const char* TEXTURE_PATH = "grass.jpg";
constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct plane_push_constant {
	alignas(16) glm::mat4 model_matrix;
	alignas(16) glm::mat4 view_matrix;
	alignas(16) glm::mat4 projection_matrix;
};

class device_context {
public:
	device_context(const std::vector<vertex>& plane, const std::vector<uint32_t>& plane_indices, const std::vector<blade>& grass)
		: blades_num_(grass.size())
	{
		init_window();
		init_vulkan(plane, plane_indices, grass);
	}

	~device_context() {
		cleanup();
	}

public:	
	float aspect_ratio() const {
		return swapchain_extent.width / static_cast<float>(swapchain_extent.height);
	}

private:
	void init_window() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window_ = glfwCreateWindow(tools::params::WIDTH, tools::params::HEIGHT, "grass", nullptr, nullptr);
	}

	void init_vulkan(const std::vector<vertex> &plane, const std::vector<uint32_t> &plane_indices, const std::vector<blade> &grass) {
		create_instance();
		create_surface();
		setup_debug_messenger();
		pick_pysical_device();
		create_logical_device();

		create_swapchain();
		create_image_views();
		create_render_pass();
		create_plane_descriptor_set_layout();

		create_plane_graphics_pipeline();
		create_grass_tessellation_pipeline();

		create_command_pool();

		create_depth_resources();
		create_framebuffers();

		create_texture_image();
		create_texture_image_view();
		create_texture_sampler();

		create_vertex_buffer(plane);
		create_index_buffer(plane_indices);

		create_grass_vertex_buffer(grass);
		create_culled_grass_buffer(grass);
		create_indirect_commands_buffer(grass);

		create_uniform_buffers();

		create_descriptor_pool();
		create_descriptor_sets();

		create_compute_descritpor_set_layout();
		create_compute_descriptor_sets();
		create_compute_pipeline();
		get_compute_queue();

		create_command_buffers();

		create_sync_objects();
	}
	
	void cleanup() {
		logical_device_.waitIdle();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			logical_device_.destroySemaphore(image_available_semaphores[i]);
			logical_device_.destroySemaphore(render_finished_semaphores[i]);
			logical_device_.destroyFence(in_flight_fences[i]);
		}

		logical_device_.destroyCommandPool(command_pool);

		cleanup_swapchain();

		logical_device_.destroySampler(texture_sampler);
		logical_device_.destroyImageView(texture_image_view);
		logical_device_.destroyImage(texture_image);
		logical_device_.freeMemory(texture_image_memory);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			logical_device_.destroyBuffer(uniform_buffers[i]);
			logical_device_.unmapMemory(uniform_buffers_memory[i]);
			logical_device_.freeMemory(uniform_buffers_memory[i]);
		}

		logical_device_.destroyDescriptorPool(descriptor_pool);
		logical_device_.destroyDescriptorSetLayout(plane_descriptor_set_layout);

		logical_device_.destroyBuffer(plane_index_buffer_);
		logical_device_.freeMemory(plane_index_buffer_memory_);

		logical_device_.destroyBuffer(plane_vertex_buffer_);
		logical_device_.freeMemory(plane_vertex_buffer_memory_);

		logical_device_.destroyBuffer(blades_buffer);
		logical_device_.freeMemory(blades_buffer_memory);

		logical_device_.destroyBuffer(culled_blades_buffer);
		logical_device_.freeMemory(culled_blades_buffer_memory);

		logical_device_.destroyBuffer(indirect_draw_commands_buffer_);
		logical_device_.freeMemory(indirect_draw_commands_buffer_memory_);


		logical_device_.destroyPipelineLayout(plane_pipeline_layout_);
		logical_device_.destroyPipeline(plane_graphics_pipeline_);
		logical_device_.destroyRenderPass(render_pass);

		logical_device_.destroyPipelineLayout(grass_pipeline_layout_);
		logical_device_.destroyPipeline(grass_pipeline_);

		logical_device_.destroyDescriptorSetLayout(compute_set_layout_);
		
		logical_device_.destroyPipelineLayout(compute_pipeline_layout_);
		logical_device_.destroyPipeline(compute_pipeline_);

		logical_device_.destroy();
		instance_.destroySurfaceKHR(surface_);

		vk_tools::logging::DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_);
		instance_.destroy();

		glfwDestroyWindow(window_);
		glfwTerminate();
	}

	void create_instance() {
		auto requested_extensions = get_required_extensions();
		std::vector<const char*> requested_layers = { "VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_monitor" };

		auto createInfo = vk::InstanceCreateInfo{};
		createInfo.ppEnabledExtensionNames = requested_extensions.data();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requested_extensions.size());
		createInfo.enabledLayerCount = static_cast<uint32_t>(requested_layers.size());
		createInfo.ppEnabledLayerNames = requested_layers.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		populate_debug_messenger_create_info(debugCreateInfo);
		createInfo.pNext = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);

		instance_ = vk::createInstance(createInfo);
	}

	void create_surface() {
		VkSurfaceKHR c_style_surface;

		if (glfwCreateWindowSurface(instance_, window_, nullptr, &c_style_surface) != VK_SUCCESS) 
			throw std::runtime_error("Failed to abstract glfw surface for Vulkan.");
		
		surface_ = c_style_surface;
	}

	auto get_required_extensions(bool debug = true) -> std::vector<const char*> {
		uint32_t glfwExtensionCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (debug) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		return extensions;
	}

	void setup_debug_messenger() {
		VkDebugUtilsMessengerCreateInfoEXT create_info;
		populate_debug_messenger_create_info(create_info);
		vk_tools::logging::CreateDebugUtilsMessengerEXT(instance_, create_info, debug_messenger_);
	}

	void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
		create_info = VkDebugUtilsMessengerCreateInfoEXT{};
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

		create_info.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

		create_info.pfnUserCallback = vk_tools::logging::debugCallback;
	}

	void pick_pysical_device() {
		physical_device_ = instance_.enumeratePhysicalDevices().front();
	}

	void create_logical_device() {
		auto properties = physical_device_.getProperties();
		auto indices = findQueueFamilies(physical_device_, surface_);

		std::set<int> unique_queue_families = { indices.graphics_family, indices.present_family };;

		std::vector< vk::DeviceQueueCreateInfo> queue_create_infos;

		for (auto queue_family : unique_queue_families) {

			auto queue_create_info = vk::DeviceQueueCreateInfo{};

			queue_create_info.queueFamilyIndex = queue_family;
			queue_create_info.queueCount = 1;

			auto priority = 1.0f;
			queue_create_info.pQueuePriorities = &priority;

			queue_create_infos.emplace_back(queue_create_info);
		}

		auto device_create_info = vk::DeviceCreateInfo{};
		device_create_info.pQueueCreateInfos = queue_create_infos.data();
		device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());

		device_create_info.enabledExtensionCount = static_cast<uint32_t>(tools::requested_extensions.size());
		device_create_info.ppEnabledExtensionNames = tools::requested_extensions.data();

		auto features = physical_device_.getFeatures();
		features.samplerAnisotropy = true;

		device_create_info.pEnabledFeatures = &features;

		logical_device_ = physical_device_.createDevice(device_create_info);

		graphics_queue_ = logical_device_.getQueue(indices.graphics_family, 0);
		present_queue_ = logical_device_.getQueue(indices.present_family, 0);
	}

	void create_swapchain() {
		auto swapchain_properties = vk_tools::query_swapchain_support_details(physical_device_, surface_);
		auto surface_format = vk_tools::choose_surface_format(swapchain_properties.formats);
		auto present_mode = vk_tools::choose_present_mode(swapchain_properties.present_modes);
		auto extent = vk_tools::choose_swap_extent(swapchain_properties.capabilities, window_);

		auto image_count = ++swapchain_properties.capabilities.minImageCount;

		//0 is a special value that means that there is no maximum
		if (auto max_image_count = swapchain_properties.capabilities.maxImageCount;
			(max_image_count > 0) && (image_count > max_image_count))
			image_count = max_image_count;

		auto create_info = vk::SwapchainCreateInfoKHR{};

		create_info.surface = surface_;
		create_info.minImageCount = image_count;
		create_info.imageFormat = surface_format.format;
		create_info.imageExtent = extent;
		create_info.imageArrayLayers = 1;
		create_info.imageColorSpace = surface_format.colorSpace;

		create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

		auto indices = findQueueFamilies(physical_device_, surface_);

		if (indices.graphics_family != indices.present_family) {
			create_info.imageSharingMode = vk::SharingMode::eConcurrent;

			uint32_t queue_family_indices[] = {
				static_cast<uint32_t>(indices.graphics_family),
				static_cast<uint32_t>(indices.present_family)
			};

			create_info.queueFamilyIndexCount = 2;
			create_info.pQueueFamilyIndices = queue_family_indices;
		}
		else
			create_info.imageSharingMode = vk::SharingMode::eExclusive;

		create_info.preTransform = swapchain_properties.capabilities.currentTransform;
		create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		create_info.presentMode = present_mode; //speaks for itself
		create_info.clipped = true;

		swapchain_ = logical_device_.createSwapchainKHR(create_info);

		swapchain_images = logical_device_.getSwapchainImagesKHR(swapchain_);
		swapchain_image_format_ = surface_format.format;
		swapchain_extent = extent;
	}

	void create_image_views() {
		for (size_t i = 0; i < swapchain_images.size(); ++i)
			swapchain_image_views.emplace_back(create_image_view(swapchain_images[i], swapchain_image_format_, vk::ImageAspectFlagBits::eColor));
	}

	void create_render_pass() {
		vk::AttachmentDescription color_attachment;
		color_attachment.format = swapchain_image_format_;
		color_attachment.samples = vk::SampleCountFlagBits::e1;
		
		color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
		color_attachment.storeOp = vk::AttachmentStoreOp::eStore;

		color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		
		color_attachment.initialLayout = vk::ImageLayout::eUndefined;
		color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

		vk::AttachmentReference color_attachment_ref{};
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;


		vk::AttachmentDescription depth_attachment{};
		depth_attachment.format = find_depth_format();
		depth_attachment.samples = vk::SampleCountFlagBits::e1;
		depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
		depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		
		depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;

		depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
		depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

		vk::AttachmentReference depth_attachment_ref{};
		depth_attachment_ref.attachment = 1;
		depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		vk::SubpassDescription subpass{};
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.pColorAttachments = &color_attachment_ref;
		subpass.colorAttachmentCount = 1;
		subpass.pDepthStencilAttachment = &depth_attachment_ref;

		vk::SubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		dependency.srcStageMask =
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
		dependency.srcAccessMask = vk::AccessFlagBits::eNone;

		dependency.dstStageMask =
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests;
		dependency.dstAccessMask =
			vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

		std::array<vk::AttachmentDescription, 2> attachments = { color_attachment, depth_attachment };

		vk::RenderPassCreateInfo render_pass_create_info{};
		render_pass_create_info.attachmentCount = attachments.size();
		render_pass_create_info.pAttachments = attachments.data();
		render_pass_create_info.pSubpasses = &subpass;
		render_pass_create_info.subpassCount = 1;
		render_pass_create_info.dependencyCount = 1;
		render_pass_create_info.pDependencies = &dependency;

		render_pass = logical_device_.createRenderPass(render_pass_create_info);
	}

	void create_plane_descriptor_set_layout() {
		vk::DescriptorSetLayoutBinding ubo_layout_binding{};
		ubo_layout_binding.binding = 0;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
		ubo_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

		vk::DescriptorSetLayoutBinding sampler_layout_binding{};
		sampler_layout_binding.binding = 1;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		sampler_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		std::vector<vk::DescriptorSetLayoutBinding> bindings = { ubo_layout_binding, sampler_layout_binding };

		vk::DescriptorSetLayoutCreateInfo layout_info{};
		layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
		layout_info.pBindings = bindings.data();
		plane_descriptor_set_layout = logical_device_.createDescriptorSetLayout(layout_info);
	}

	void create_plane_graphics_pipeline() {
		auto vert_shader_code = read_file("plane.vert.spv");
		auto frag_shader_code = read_file("plane.frag.spv");
		
		auto vert_shader_module = create_shader_module(vert_shader_code);
		auto frag_shader_module = create_shader_module(frag_shader_code);

		vk::PipelineShaderStageCreateInfo vert_shader_stage_create_info{};
		vert_shader_stage_create_info.module = vert_shader_module;
		vert_shader_stage_create_info.stage = vk::ShaderStageFlagBits::eVertex;
		vert_shader_stage_create_info.pName = "main";
		vert_shader_stage_create_info.pSpecializationInfo = nullptr;

		vk::PipelineShaderStageCreateInfo frag_shader_stage_create_info{};
		frag_shader_stage_create_info.module = frag_shader_module;
		frag_shader_stage_create_info.stage = vk::ShaderStageFlagBits::eFragment;
		frag_shader_stage_create_info.pName = "main";

		vk::PipelineShaderStageCreateInfo shader_stages[] = {
			vert_shader_stage_create_info, frag_shader_stage_create_info
		};

		std::vector<vk::DynamicState> dynamic_states = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{};
		dynamic_state_create_info.pDynamicStates = dynamic_states.data();
		dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());

		vk::PipelineVertexInputStateCreateInfo vertex_input_create_info{};

		auto binding_description = vertex::get_binding_description();
		auto attribute_descriptions = vertex::get_attribute_descriptions();

		vertex_input_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
		vertex_input_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();
		vertex_input_create_info.vertexBindingDescriptionCount = 1;
		vertex_input_create_info.pVertexBindingDescriptions = &binding_description;

		vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
		input_assembly_create_info.topology = vk::PrimitiveTopology::eTriangleList;

		vk::Viewport viewport{};
		viewport.height = static_cast<float>(swapchain_extent.height);
		viewport.width = static_cast<float>(swapchain_extent.width);
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = swapchain_extent;

		vk::PipelineViewportStateCreateInfo viewport_state_create_info{};

		viewport_state_create_info.scissorCount = 1;
		viewport_state_create_info.viewportCount = 1;
		viewport_state_create_info.pViewports = &viewport;
		viewport_state_create_info.pScissors = &scissor;


		vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info{};

		rasterization_state_create_info.depthClampEnable = false;
		rasterization_state_create_info.rasterizerDiscardEnable = false;
		rasterization_state_create_info.polygonMode = vk::PolygonMode::eFill;
		rasterization_state_create_info.lineWidth = 1.0f;
		rasterization_state_create_info.cullMode = vk::CullModeFlagBits::eBack;
		rasterization_state_create_info.frontFace = vk::FrontFace::eCounterClockwise;
		rasterization_state_create_info.depthBiasEnable = false; 

		vk::PipelineMultisampleStateCreateInfo multisample_state_create_info{};
		multisample_state_create_info.minSampleShading = 1;
		multisample_state_create_info.sampleShadingEnable = false;
		multisample_state_create_info.rasterizationSamples = vk::SampleCountFlagBits::e1;

		vk::PipelineColorBlendAttachmentState color_blend_attachment{};
		color_blend_attachment.blendEnable = false;
		color_blend_attachment.colorWriteMask = /*RGBA*/
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

		color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
		color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
		color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;

		color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
		color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eOne;
		color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eZero;

		/*

		if (blendEnable) {
			finalColor.rgb = (srcColorBlendFactor * newColor.rgb)
			<colorBlendOp> (dstColorBlendFactor * oldColor.rgb);

			finalColor.a = (srcAlphaBlendFactor * newColor.a)
			<alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);

		} else {
			finalColor = newColor;
		}

		finalColor = finalColor & colorWriteMask;

		*/

		vk::PipelineColorBlendStateCreateInfo color_blending{};

		color_blending.logicOpEnable = false;
		color_blending.logicOp = vk::LogicOp::eCopy;
		color_blending.attachmentCount = 1;
		color_blending.pAttachments = &color_blend_attachment;
		color_blending.blendConstants[0] = 0.0f;
		color_blending.blendConstants[1] = 0.0f;
		color_blending.blendConstants[2] = 0.0f;
		color_blending.blendConstants[3] = 0.0f;

		vk::PipelineDepthStencilStateCreateInfo depth_stencil_info{};

		depth_stencil_info.depthTestEnable = true;
		depth_stencil_info.depthWriteEnable = true;

		depth_stencil_info.depthCompareOp = vk::CompareOp::eLess;

		depth_stencil_info.depthBoundsTestEnable = false;
		depth_stencil_info.minDepthBounds = 0.0f;
		depth_stencil_info.maxDepthBounds = 1.0f;

		depth_stencil_info.stencilTestEnable = false;

		vk::PipelineLayoutCreateInfo pipeline_layout_create_info{};
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.setLayoutCount = 1;
		pipeline_layout_create_info.pSetLayouts = &plane_descriptor_set_layout;
		
		vk::PushConstantRange push_constant_range{};
		push_constant_range.size = sizeof(plane_push_constant);
		push_constant_range.offset = 0;
		push_constant_range.stageFlags = vk::ShaderStageFlagBits::eVertex;

		pipeline_layout_create_info.pushConstantRangeCount = 1;
		pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;

		plane_pipeline_layout_ = logical_device_.createPipelineLayout(pipeline_layout_create_info);

		vk::GraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = shader_stages;

		pipeline_info.pVertexInputState = &vertex_input_create_info;
		pipeline_info.pInputAssemblyState = &input_assembly_create_info;
		pipeline_info.pViewportState = &viewport_state_create_info;
		pipeline_info.pRasterizationState = &rasterization_state_create_info;
		pipeline_info.pMultisampleState = &multisample_state_create_info;
		pipeline_info.pDynamicState = &dynamic_state_create_info;
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.layout = plane_pipeline_layout_;
		pipeline_info.pDepthStencilState = &depth_stencil_info;
		pipeline_info.renderPass = render_pass;
		pipeline_info.subpass = 0;

		plane_graphics_pipeline_ = logical_device_.createGraphicsPipeline(nullptr, pipeline_info).value;

		logical_device_.destroyShaderModule(vert_shader_module);
		logical_device_.destroyShaderModule(frag_shader_module);
	}

	void create_grass_tessellation_pipeline() {

		auto vert_shader_code = read_file("grass.vert.spv");
		auto frag_shader_code = read_file("grass.frag.spv");

		auto TCS_shader_code = read_file("grass.tesc.spv");
		auto TES_shader_code = read_file("grass.tese.spv");

		auto vert_shader_module = create_shader_module(vert_shader_code);
		auto frag_shader_module = create_shader_module(frag_shader_code);
		auto TCS_shader_module = create_shader_module(TCS_shader_code);
		auto TES_shader_module = create_shader_module(TES_shader_code);

		vk::PipelineShaderStageCreateInfo vert_shader_stage_create_info{};
		vert_shader_stage_create_info.module = vert_shader_module;
		vert_shader_stage_create_info.stage = vk::ShaderStageFlagBits::eVertex;
		vert_shader_stage_create_info.pName = "main";

		vk::PipelineShaderStageCreateInfo frag_shader_stage_create_info{};
		frag_shader_stage_create_info.module = frag_shader_module;
		frag_shader_stage_create_info.stage = vk::ShaderStageFlagBits::eFragment;
		frag_shader_stage_create_info.pName = "main";

		vk::PipelineShaderStageCreateInfo TCS_shader_stage_create_info{};
		TCS_shader_stage_create_info.module = TCS_shader_module;
		TCS_shader_stage_create_info.stage = vk::ShaderStageFlagBits::eTessellationControl;
		TCS_shader_stage_create_info.pName = "main";

		vk::PipelineShaderStageCreateInfo TES_shader_stage_create_info{};
		TES_shader_stage_create_info.module = TES_shader_module;
		TES_shader_stage_create_info.stage = vk::ShaderStageFlagBits::eTessellationEvaluation;
		TES_shader_stage_create_info.pName = "main";


		vk::PipelineShaderStageCreateInfo shader_stages[] = {
			vert_shader_stage_create_info, frag_shader_stage_create_info,
			TCS_shader_stage_create_info, TES_shader_stage_create_info
		};

		std::vector<vk::DynamicState> dynamic_states = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{};
		dynamic_state_create_info.pDynamicStates = dynamic_states.data();
		dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());

		vk::PipelineVertexInputStateCreateInfo vertex_input_create_info{};

		auto binding_description = blade::binding_description();
		auto attribute_descriptions = blade::attribute_descriptions();

		vertex_input_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
		vertex_input_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();
		vertex_input_create_info.vertexBindingDescriptionCount = 1;
		vertex_input_create_info.pVertexBindingDescriptions = &binding_description;

		vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
		input_assembly_create_info.topology = vk::PrimitiveTopology::ePatchList;

		vk::Viewport viewport{};
		viewport.height = static_cast<float>(swapchain_extent.height);
		viewport.width = static_cast<float>(swapchain_extent.width);
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = swapchain_extent;

		vk::PipelineViewportStateCreateInfo viewport_state_create_info{};
		viewport_state_create_info.scissorCount = 1;
		viewport_state_create_info.viewportCount = 1;
		viewport_state_create_info.pViewports = &viewport;
		viewport_state_create_info.pScissors = &scissor;

		vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info{};
		rasterization_state_create_info.depthClampEnable = false;
		rasterization_state_create_info.rasterizerDiscardEnable = false; 
		rasterization_state_create_info.polygonMode = vk::PolygonMode::eFill;
		rasterization_state_create_info.lineWidth = 1.0f;
		rasterization_state_create_info.cullMode = vk::CullModeFlagBits::eBack;
		rasterization_state_create_info.frontFace = vk::FrontFace::eCounterClockwise;
		rasterization_state_create_info.depthBiasEnable = false; 

		vk::PipelineMultisampleStateCreateInfo multisample_state_create_info{};
		multisample_state_create_info.minSampleShading = 1;
		multisample_state_create_info.sampleShadingEnable = false;
		multisample_state_create_info.rasterizationSamples = vk::SampleCountFlagBits::e1;

		vk::PipelineColorBlendAttachmentState color_blend_attachment{};
		color_blend_attachment.blendEnable = false;
		color_blend_attachment.colorWriteMask = /*RGBA*/
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

		color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
		color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
		color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;

		color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
		color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eOne;
		color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eZero;

		vk::PipelineColorBlendStateCreateInfo color_blending{};
		color_blending.logicOpEnable = false;
		color_blending.logicOp = vk::LogicOp::eCopy;
		color_blending.attachmentCount = 1;
		color_blending.pAttachments = &color_blend_attachment;
		color_blending.blendConstants[0] = 0.0f;
		color_blending.blendConstants[1] = 0.0f;
		color_blending.blendConstants[2] = 0.0f;
		color_blending.blendConstants[3] = 0.0f;

		vk::PipelineDepthStencilStateCreateInfo depth_stencil_info{};
		depth_stencil_info.depthTestEnable = true;
		depth_stencil_info.depthWriteEnable = true;
		depth_stencil_info.depthCompareOp = vk::CompareOp::eLess;

		depth_stencil_info.depthBoundsTestEnable = false;
		depth_stencil_info.minDepthBounds = 0.0f;
		depth_stencil_info.maxDepthBounds = 1.0f;

		depth_stencil_info.stencilTestEnable = false;


		vk::PipelineTessellationStateCreateInfo tessellation_state_info{};
		tessellation_state_info.patchControlPoints = 1;

		vk::PushConstantRange vertex_range{};
		vertex_range.offset = 0;
		vertex_range.size = sizeof(glm::mat4);
		vertex_range.stageFlags = vk::ShaderStageFlagBits::eVertex;

		vk::PushConstantRange TES_range{};
		TES_range.offset = sizeof(glm::mat4);
		TES_range.size = 2 * sizeof(glm::mat4);
		TES_range.stageFlags = vk::ShaderStageFlagBits::eTessellationEvaluation;

		vk::PushConstantRange push_constant_ranges[] = { vertex_range, TES_range };

		vk::PipelineLayoutCreateInfo pipeline_layout_create_info{};
		pipeline_layout_create_info.pushConstantRangeCount = sizeof(push_constant_ranges) / sizeof(push_constant_ranges[0]);
		pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges;
		pipeline_layout_create_info.setLayoutCount = 0;
		pipeline_layout_create_info.pSetLayouts = nullptr;

		grass_pipeline_layout_ = logical_device_.createPipelineLayout(pipeline_layout_create_info);

		vk::GraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.stageCount = sizeof(shader_stages) / sizeof(shader_stages[0]);
		pipeline_info.pStages = shader_stages;

		pipeline_info.pVertexInputState = &vertex_input_create_info;
		pipeline_info.pInputAssemblyState = &input_assembly_create_info;
		pipeline_info.pViewportState = &viewport_state_create_info;
		pipeline_info.pRasterizationState = &rasterization_state_create_info;
		pipeline_info.pMultisampleState = &multisample_state_create_info;
		pipeline_info.pDynamicState = &dynamic_state_create_info;
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.pTessellationState = &tessellation_state_info;
		pipeline_info.layout = grass_pipeline_layout_;
		pipeline_info.pDepthStencilState = &depth_stencil_info;
		pipeline_info.renderPass = render_pass;
		pipeline_info.subpass = 0;

		grass_pipeline_ = logical_device_.createGraphicsPipeline(nullptr, pipeline_info).value;

		logical_device_.destroyShaderModule(vert_shader_module);
		logical_device_.destroyShaderModule(frag_shader_module);
		logical_device_.destroyShaderModule(TCS_shader_module);
		logical_device_.destroyShaderModule(TES_shader_module);
	}

	static std::vector<char> read_file(const std::string& path) {
		//ate: start reading at the end of file so we could use the	
		//read position to determine the size of the file

		//binary: read the file as binary file (avoid text transformations)
		std::ifstream file(path, std::ios::ate | std::ios::binary);

		if (!file.is_open()) throw std::runtime_error("failed to open file!");

		const auto file_size = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(file_size);

		file.seekg(0);
		file.read(buffer.data(), file_size);

		file.close();

		return buffer;
	}

	vk::ShaderModule create_shader_module(std::vector<char>& shader_code) {
		vk::ShaderModuleCreateInfo create_info{};
		create_info.pCode = reinterpret_cast<uint32_t*>(shader_code.data());
		create_info.codeSize = shader_code.size();

		return logical_device_.createShaderModule(create_info);
	}

	void create_framebuffers() {

		for (const auto& image_view : swapchain_image_views) {
			// the color attachment differs for each swap chain frame,
			// but the same depth image can be used by all of them 
			// beacuse only a single subpass is running at the same 
			// time due to our semaphores.

			vk::ImageView attachments[] = { image_view, depth_image_view };

			vk::FramebufferCreateInfo frame_buffer_info{};

			//only render passes that use the same number and type of attachments
			frame_buffer_info.renderPass = render_pass;

			frame_buffer_info.attachmentCount = sizeof(attachments) / sizeof(vk::ImageView);
			frame_buffer_info.pAttachments = attachments;

			//swapchain images are single images
			frame_buffer_info.layers = 1;

			frame_buffer_info.width = swapchain_extent.width;
			frame_buffer_info.height = swapchain_extent.height;

			swapchain_framebuffers.emplace_back(logical_device_.createFramebuffer(frame_buffer_info));
		}
	}

	void create_command_pool() {
		auto queue_family_indices = findQueueFamilies(physical_device_, surface_);

		vk::CommandPoolCreateInfo pool_info{};
		pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		pool_info.queueFamilyIndex = queue_family_indices.graphics_family;

		command_pool = logical_device_.createCommandPool(pool_info);
	}

	void create_depth_resources() {
		auto depth_format = find_depth_format();
		create_image(swapchain_extent.width, swapchain_extent.height, depth_format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depth_image, depth_image_memory);
		depth_image_view = create_image_view(depth_image, depth_format, vk::ImageAspectFlagBits::eDepth);
	}

	bool has_stencil_component(const vk::Format& format) {
		return format == vk::Format::eD24UnormS8Uint || format == vk::Format::eD32SfloatS8Uint;
	}

	vk::Format find_depth_format() {
		return find_supported_format(
			{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment
		);
	}

	vk::Format find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags desirable_features) {
		for (auto format : candidates) {
			auto properties = physical_device_.getFormatProperties(format);
			if (tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & desirable_features))
				return format;
			if (tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & desirable_features))
				return format;
		}

		throw std::runtime_error("failed to find desirable format!");
	}

	void create_image(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& image_memory) {

		vk::ImageCreateInfo image_info{};
		image_info.arrayLayers = 1;
		image_info.mipLevels = 1;
		image_info.sharingMode = vk::SharingMode::eExclusive;
		image_info.extent.depth = 1;
		image_info.extent.width = width;
		image_info.extent.height = height;
		image_info.imageType = vk::ImageType::e2D;
		image_info.format = format;
		image_info.tiling = tiling;
		image_info.initialLayout = vk::ImageLayout::eUndefined;
		image_info.usage = usage;
		image_info.samples = vk::SampleCountFlagBits::e1;

		image = logical_device_.createImage(image_info);

		vk::MemoryRequirements mem_requirements = logical_device_.getImageMemoryRequirements(image);

		vk::MemoryAllocateInfo alloc_info{};
		alloc_info.allocationSize = mem_requirements.size;
		alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);

		image_memory = logical_device_.allocateMemory(alloc_info);

		logical_device_.bindImageMemory(image, image_memory, 0);
	}

	void create_texture_image() {
		int tex_width;
		int tex_height;
		int tex_channels; 

		auto pixels = stbi_load(TEXTURE_PATH, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

		if (!pixels) throw std::runtime_error("failed to load texture image!");

		vk::DeviceSize image_size = tex_width * tex_height * 4;

		vk::Buffer staging_buffer;
		vk::DeviceMemory staging_buffer_memory;

		create_buffer(
			image_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			staging_buffer,
			staging_buffer_memory
		);

		auto pdata = logical_device_.mapMemory(staging_buffer_memory, 0, image_size);
		std::memcpy(pdata, pixels, image_size);
		logical_device_.unmapMemory(staging_buffer_memory);

		stbi_image_free(pixels);

		create_image(
			tex_width,
			tex_height,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			texture_image,
			texture_image_memory
		);

		transition_image_layout(
			texture_image,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			vk::AccessFlags(), // vk::AccessFlagBits::eHostWrite
			vk::AccessFlagBits::eTransferWrite
		);

		copy_buffer_to_image(staging_buffer, texture_image, tex_width, tex_height);

		transition_image_layout(
			texture_image,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead
		);

		logical_device_.destroyBuffer(staging_buffer);
		logical_device_.freeMemory(staging_buffer_memory);
	}

	void transition_image_layout(vk::Image& image, vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::ImageAspectFlags image_aspect,
		vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage, vk::AccessFlags src_access_mask, vk::AccessFlags dst_access_mask) {

		auto transition_commands = begin_single_time_commands();

		vk::ImageMemoryBarrier barrier{};
		barrier.image = image;
		barrier.newLayout = new_layout;
		barrier.oldLayout = old_layout;

		// not the default value!
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.srcAccessMask = src_access_mask; // which types of operations that involve the resource must happen before the barrier
		barrier.dstAccessMask = dst_access_mask; // which operations that involve the resource must wait on barrier

		barrier.subresourceRange.aspectMask = image_aspect;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		transition_commands.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);

		end_single_time_commads(transition_commands);
	}

	void copy_buffer_to_image(vk::Buffer& buffer, vk::Image& image, uint32_t width, uint32_t height) {
		auto transfer_comands = begin_single_time_commands();

		vk::BufferImageCopy region{};
		region.imageExtent = vk::Extent3D(width, height, 1);
		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.layerCount = 1;

		transfer_comands.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

		end_single_time_commads(transfer_comands);
	}

	[[ nodiscard ]]
	vk::ImageView create_image_view(vk::Image& image, vk::Format format, vk::ImageAspectFlags aspect_flags) {

		vk::ImageViewCreateInfo view_info{};
		view_info.viewType = vk::ImageViewType::e2D;
		view_info.image = image;
		view_info.format = format;
		view_info.subresourceRange.aspectMask = aspect_flags;
		view_info.subresourceRange.layerCount = 1;
		view_info.subresourceRange.levelCount = 1;
		view_info.components = vk::ComponentSwizzle::eIdentity;

		return logical_device_.createImageView(view_info);
	}

	void create_texture_image_view() {
		texture_image_view = create_image_view(texture_image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
	}

	void create_texture_sampler() {
		vk::SamplerCreateInfo sampler_info{};

		sampler_info.magFilter = vk::Filter::eNearest;
		sampler_info.minFilter = vk::Filter::eLinear;

		sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;

		// there is no reason not to use this unless performance is a concern
		sampler_info.anisotropyEnable = true;

		auto device_properties = physical_device_.getProperties();
		sampler_info.maxAnisotropy = device_properties.limits.maxSamplerAnisotropy;
		sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
		sampler_info.unnormalizedCoordinates = false;
		sampler_info.compareEnable = false;
		sampler_info.compareOp = vk::CompareOp::eAlways;
		sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
		sampler_info.mipLodBias = 0.0f;
		sampler_info.maxLod = 0.0f;
		sampler_info.minLod = 0.0f;

		texture_sampler = logical_device_.createSampler(sampler_info);
	}

	vk::CommandBuffer begin_single_time_commands() {
		vk::CommandBufferAllocateInfo alloc_info{};
		alloc_info.commandBufferCount = 1;
		alloc_info.level = vk::CommandBufferLevel::ePrimary;
		alloc_info.commandPool = command_pool;

		auto command_buffer = logical_device_.allocateCommandBuffers(alloc_info)[0];

		vk::CommandBufferBeginInfo being_info{};
		being_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		command_buffer.begin(being_info);

		return command_buffer;
	}

	void end_single_time_commads(vk::CommandBuffer& command_buffer) {
		command_buffer.end();

		vk::SubmitInfo submit_info{};
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;

		graphics_queue_.submit(submit_info);
		graphics_queue_.waitIdle(); // PROFILING!!

		logical_device_.freeCommandBuffers(command_pool, command_buffer);
	}

	uint32_t find_memory_type(uint32_t supported_types_mask, vk::MemoryPropertyFlags properties) {
		auto supported_properties = physical_device_.getMemoryProperties();
		for (uint32_t i = 0; i < supported_properties.memoryTypeCount; ++i)
			if (supported_types_mask & (1 << i)
				&&
				(supported_properties.memoryTypes[i].propertyFlags & properties))
				return i;

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& buffer_memory) {
		vk::BufferCreateInfo buffer_info{};
		buffer_info.size = size;
		buffer_info.usage = usage;
		buffer_info.sharingMode = vk::SharingMode::eExclusive; //the buffer will only be used from the graphics queue
	
		buffer = logical_device_.createBuffer(buffer_info);

		auto memory_requirements = logical_device_.getBufferMemoryRequirements(buffer);
		
		vk::MemoryAllocateInfo alloc_info{};
		alloc_info.allocationSize = memory_requirements.size;
		alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

		buffer_memory = logical_device_.allocateMemory(alloc_info);
		logical_device_.bindBufferMemory(buffer, buffer_memory, 0);
	}

	void copy_buffer(vk::Buffer& dst, vk::Buffer& src, vk::DeviceSize size) {
		auto transfer_commands = begin_single_time_commands();
		transfer_commands.copyBuffer(src, dst, vk::BufferCopy(0, 0, size));
		end_single_time_commads(transfer_commands);
	}

	void create_vertex_buffer(const std::vector<vertex>& vertices) {
		const vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

		vk::Buffer staging_buffer;
		vk::DeviceMemory staging_buffer_memory;

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
			staging_buffer,
			staging_buffer_memory
		);

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			plane_vertex_buffer_,
			plane_vertex_buffer_memory_
		);

		auto pstaging_data = logical_device_.mapMemory(staging_buffer_memory, 0, buffer_size);
		std::memcpy(pstaging_data, vertices.data(), buffer_size);
		logical_device_.unmapMemory(staging_buffer_memory);

		copy_buffer(plane_vertex_buffer_, staging_buffer, buffer_size);

		logical_device_.destroyBuffer(staging_buffer);
		logical_device_.freeMemory(staging_buffer_memory);
	}

	void create_index_buffer(const std::vector<uint32_t> &indices) {
		const vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();

		vk::Buffer staging_buffer;
		vk::DeviceMemory staging_buffer_memory;

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			staging_buffer,
			staging_buffer_memory
		);

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			plane_index_buffer_,
			plane_index_buffer_memory_
		);

		auto pdata = logical_device_.mapMemory(staging_buffer_memory, 0, buffer_size);
		std::memcpy(pdata, indices.data(), buffer_size);
		logical_device_.unmapMemory(staging_buffer_memory);

		copy_buffer(plane_index_buffer_, staging_buffer, buffer_size);

		logical_device_.destroyBuffer(staging_buffer);
		logical_device_.freeMemory(staging_buffer_memory);
	}

	void create_grass_vertex_buffer(const std::vector<blade> &blades) {
		const vk::DeviceSize buffer_size = sizeof(blades[0]) * blades.size();

		vk::Buffer staging_buffer;
		vk::DeviceMemory staging_buffer_memory;

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
			staging_buffer,
			staging_buffer_memory
		);

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			blades_buffer,
			blades_buffer_memory
		);

		auto pstaging_data = logical_device_.mapMemory(staging_buffer_memory, 0, buffer_size);
		std::memcpy(pstaging_data, blades.data(), buffer_size);
		logical_device_.unmapMemory(staging_buffer_memory);

		copy_buffer(blades_buffer, staging_buffer, buffer_size);

		logical_device_.destroyBuffer(staging_buffer);
		logical_device_.freeMemory(staging_buffer_memory);
	}

	void create_culled_grass_buffer(const std::vector<blade>& blades) {
		const vk::DeviceSize buffer_size = sizeof(blades[0]) * blades.size();

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible,
			culled_blades_buffer,
			culled_blades_buffer_memory
		);
	}

	void create_indirect_commands_buffer(const std::vector<blade>& blades) {
		const vk::DeviceSize buffer_size = sizeof(blade_draw_indirect);

		blade_draw_indirect indirect_data;
		indirect_data.first_instance = 0;
		indirect_data.instance_count = 1;
		indirect_data.first_vertex = 0;
		indirect_data.vertex_count = blades_num_;

		vk::Buffer staging_buffer;
		vk::DeviceMemory staging_buffer_memory;

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
			staging_buffer,
			staging_buffer_memory
		);

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			indirect_draw_commands_buffer_,
			indirect_draw_commands_buffer_memory_
		);

		auto pstaging_data = logical_device_.mapMemory(staging_buffer_memory, 0, buffer_size);
		std::memcpy(pstaging_data, &indirect_data, buffer_size);
		logical_device_.unmapMemory(staging_buffer_memory);

		copy_buffer(indirect_draw_commands_buffer_, staging_buffer, buffer_size);

		logical_device_.destroyBuffer(staging_buffer);
		logical_device_.freeMemory(staging_buffer_memory);
	}

	void create_uniform_buffers() {
		vk::DeviceSize buffer_size = sizeof(uniform_buffer_object);

		uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
		uniform_buffers_memory.resize(MAX_FRAMES_IN_FLIGHT);
		uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			create_buffer(
				buffer_size,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				uniform_buffers[i],
				uniform_buffers_memory[i]
			);
			uniform_buffers_mapped[i] = logical_device_.mapMemory(uniform_buffers_memory[i], 0, buffer_size);
		}
	}

	void create_descriptor_pool() {
		std::array<vk::DescriptorPoolSize, 4> pool_sizes{};

		pool_sizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		pool_sizes[0].type = vk::DescriptorType::eUniformBuffer;

		pool_sizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		pool_sizes[1].type = vk::DescriptorType::eCombinedImageSampler;

		pool_sizes[2].type = vk::DescriptorType::eStorageBuffer;
		pool_sizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		pool_sizes[3].type = vk::DescriptorType::eStorageBuffer;
		pool_sizes[3].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		vk::DescriptorPoolCreateInfo pool_info{};
		pool_info.maxSets = pool_sizes.size() + 1;
		pool_info.poolSizeCount = pool_sizes.size();
		pool_info.pPoolSizes = pool_sizes.data();

		descriptor_pool = logical_device_.createDescriptorPool(pool_info);
	}

	void create_descriptor_sets() {
		vk::DescriptorSetAllocateInfo alloc_info{};
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, plane_descriptor_set_layout);

		alloc_info.pSetLayouts = layouts.data();

		descriptor_sets = logical_device_.allocateDescriptorSets(alloc_info);

		vk::DescriptorBufferInfo buffer_info{};
		buffer_info.offset = 0;
		buffer_info.range = sizeof(uniform_buffer_object);
		
		vk::DescriptorImageInfo image_info{};
		image_info.sampler = texture_sampler;
		image_info.imageView = texture_image_view;
		image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		std::array<vk::WriteDescriptorSet, 2> descriptor_writes{};

		descriptor_writes[0].dstBinding = 0;
		descriptor_writes[0].dstArrayElement = 0; 
		descriptor_writes[0].descriptorCount = 1; 
		descriptor_writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
		descriptor_writes[0].pBufferInfo = &buffer_info; 

		descriptor_writes[1].dstBinding = 1;
		descriptor_writes[1].dstArrayElement = 0;
		descriptor_writes[1].descriptorCount = 1; 
		descriptor_writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptor_writes[1].pImageInfo = &image_info;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {

			buffer_info.buffer = uniform_buffers[i];
			descriptor_writes[0].dstSet = descriptor_sets[i];
			descriptor_writes[1].dstSet = descriptor_sets[i];

			logical_device_.updateDescriptorSets(descriptor_writes, {});
		}
	}

	void create_command_buffers() {
		vk::CommandBufferAllocateInfo alloc_info{};
		alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
		alloc_info.commandPool = command_pool;
		alloc_info.level = decltype(alloc_info.level)::ePrimary;

		command_buffers = logical_device_.allocateCommandBuffers(alloc_info);

		vk::CommandBufferAllocateInfo compute_alloc_info{};
		compute_alloc_info.commandPool = command_pool;
		compute_alloc_info.level = vk::CommandBufferLevel::ePrimary;
		compute_alloc_info.commandBufferCount = 1;

		compute_command_buffer_ = logical_device_.allocateCommandBuffers(compute_alloc_info).front();
	}

	void create_sync_objects() {
		vk::SemaphoreCreateInfo semaphore_info{};
		
		vk::FenceCreateInfo fence_info{};
		fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			image_available_semaphores.emplace_back(logical_device_.createSemaphore(semaphore_info));
			render_finished_semaphores.emplace_back(logical_device_.createSemaphore(semaphore_info));
			in_flight_fences.emplace_back(logical_device_.createFence(fence_info));
		}
	}

	void cleanup_swapchain() {

		logical_device_.destroyImageView(depth_image_view);
		logical_device_.destroyImage(depth_image);
		logical_device_.freeMemory(depth_image_memory);

		for (auto& framebuffer : swapchain_framebuffers)
			logical_device_.destroyFramebuffer(framebuffer);

		for (auto& image_view : swapchain_image_views)
			logical_device_.destroyImageView(image_view);

		logical_device_.destroySwapchainKHR(swapchain_);
	}

	void create_compute_pipeline() {
		auto shader_code = read_file("grass.comp.spv");
		vk::ShaderModule shader_module = create_shader_module(shader_code);
		
		vk::PipelineShaderStageCreateInfo shader_stage_info{};
		shader_stage_info.module = shader_module;
		shader_stage_info.pName = "main";
		shader_stage_info.stage = vk::ShaderStageFlagBits::eCompute;

		vk::PipelineLayoutCreateInfo layout_info{};
		
		vk::PushConstantRange range{};
		range.offset = 0;
		range.size = sizeof(blade_compute_push_data);
		range.stageFlags = vk::ShaderStageFlagBits::eCompute;

		layout_info.pPushConstantRanges = &range;
		layout_info.pushConstantRangeCount = 1;

		vk::DescriptorSetLayout descriptor_set_layouts[] = { compute_set_layout_ };

		layout_info.pSetLayouts = descriptor_set_layouts;
		layout_info.setLayoutCount = sizeof(descriptor_set_layouts) / sizeof(descriptor_set_layouts[0]);
		
		compute_pipeline_layout_ = logical_device_.createPipelineLayout(layout_info);

		vk::ComputePipelineCreateInfo create_info{};
		create_info.layout = compute_pipeline_layout_;
		create_info.stage = shader_stage_info;

		compute_pipeline_ = logical_device_.createComputePipeline(nullptr, create_info).value;

		logical_device_.destroyShaderModule(shader_module);
	}

	void get_compute_queue() {
		compute_queue_ = logical_device_.getQueue(0, 0);
	}

	void create_compute_descritpor_set_layout() {
		vk::DescriptorSetLayoutBinding all_blades_binding{};
		all_blades_binding.binding = 0;
		all_blades_binding.descriptorCount = 1;
		all_blades_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
		all_blades_binding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding culled_blades_binding{};
		culled_blades_binding.binding = 1;
		culled_blades_binding.descriptorCount = 1;
		culled_blades_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
		culled_blades_binding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding indirect_draw_params_binding{};
		indirect_draw_params_binding.binding = 2;
		indirect_draw_params_binding.descriptorCount = 1;
		indirect_draw_params_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
		indirect_draw_params_binding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding bindings[] = { 
			all_blades_binding, culled_blades_binding, indirect_draw_params_binding 
		};

		vk::DescriptorSetLayoutCreateInfo create_info{};
		create_info.bindingCount = sizeof(bindings) / sizeof(bindings[0]);
		create_info.pBindings = bindings;

		compute_set_layout_ = logical_device_.createDescriptorSetLayout(create_info);
	}

	void create_compute_descriptor_sets() {
		vk::DescriptorSetLayout layouts[] = { compute_set_layout_ };
		vk::DescriptorSetAllocateInfo alloc_info{ descriptor_pool, 1, &compute_set_layout_ };
	
		compute_descriptor_sets_ = logical_device_.allocateDescriptorSets(alloc_info);

		std::vector<vk::WriteDescriptorSet> descriptor_writes(3);

		vk::DescriptorBufferInfo all_blades{};
		all_blades.buffer = blades_buffer;
		all_blades.range = sizeof(blade) * blades_num_;

		descriptor_writes[0].descriptorCount = 1;
		descriptor_writes[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		descriptor_writes[0].dstBinding = 0;
		descriptor_writes[0].dstSet = compute_descriptor_sets_[0];
		descriptor_writes[0].pBufferInfo = &all_blades;

		vk::DescriptorBufferInfo culled_blades{};
		culled_blades.buffer = culled_blades_buffer;
		culled_blades.range = sizeof(blade) * blades_num_;

		descriptor_writes[1].descriptorCount = 1;
		descriptor_writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;
		descriptor_writes[1].dstBinding = 1;
		descriptor_writes[1].dstSet = compute_descriptor_sets_[0];
		descriptor_writes[1].pBufferInfo = &culled_blades;

		vk::DescriptorBufferInfo indirect_params{};
		indirect_params.buffer = indirect_draw_commands_buffer_;
		indirect_params.range = sizeof(blade_draw_indirect);

		descriptor_writes[2].descriptorCount = 1;
		descriptor_writes[2].descriptorType = vk::DescriptorType::eStorageBuffer;
		descriptor_writes[2].dstBinding = 2;
		descriptor_writes[2].dstSet = compute_descriptor_sets_[0];
		descriptor_writes[2].pBufferInfo = &indirect_params;

		logical_device_.updateDescriptorSets(descriptor_writes, {});
	}

public:
	GLFWwindow* window_ = nullptr;

	vk::Instance instance_ = nullptr;
	vk::SurfaceKHR surface_;
	VkDebugUtilsMessengerEXT debug_messenger_ = nullptr;

	vk::PhysicalDevice physical_device_ = nullptr;
	vk::Device logical_device_ = nullptr;

	//retrieving queue handles
	vk::Queue graphics_queue_ = nullptr;
	vk::Queue present_queue_ = nullptr;
	vk::Queue compute_queue_ = nullptr;

	vk::SwapchainKHR swapchain_;
	vk::Format swapchain_image_format_;
	vk::Extent2D swapchain_extent;

	//An image view is sufficient to start using an image as a texture
	std::vector<vk::Image> swapchain_images;
	std::vector<vk::ImageView> swapchain_image_views;

	vk::RenderPass render_pass;
	vk::DescriptorSetLayout plane_descriptor_set_layout;
	vk::PipelineLayout plane_pipeline_layout_;
	vk::Pipeline plane_graphics_pipeline_;

	std::vector<vk::Framebuffer> swapchain_framebuffers;

	vk::CommandPool command_pool; //for drawing
	std::vector<vk::CommandBuffer> command_buffers;
	vk::CommandBuffer compute_command_buffer_;

	std::vector<vk::Semaphore> image_available_semaphores; //an image has been acquired from the swapchain and is ready for rendering
	std::vector<vk::Semaphore> render_finished_semaphores; //rendering has finished 
	std::vector<vk::Fence> in_flight_fences; //to make sure only one frame is rendering at a time

	uint32_t current_frame;
	
	// PROFILING
	// a single vk::Buffer that contains both vertex and index buffers.
	// use aliasing to access one of them

	vk::Buffer plane_vertex_buffer_;
	vk::DeviceMemory plane_vertex_buffer_memory_;

	vk::Buffer plane_index_buffer_;
	vk::DeviceMemory plane_index_buffer_memory_;

	std::vector<vk::Buffer> uniform_buffers;
	std::vector<vk::DeviceMemory> uniform_buffers_memory;
	std::vector<void*> uniform_buffers_mapped;

	vk::DescriptorPool descriptor_pool;
	std::vector<vk::DescriptorSet> descriptor_sets;

	vk::Image texture_image;
	vk::DeviceMemory texture_image_memory;
	vk::ImageView texture_image_view;
	vk::Sampler texture_sampler;

	vk::Image depth_image;
	vk::DeviceMemory depth_image_memory;
	vk::ImageView depth_image_view;

	vk::DescriptorSetLayout compute_set_layout_;

	std::vector<vk::DescriptorSet> compute_descriptor_sets_;
	std::vector<vk::DescriptorSet> grass_descriptor_set_;

	vk::PipelineLayout compute_pipeline_layout_;
	vk::PipelineLayout grass_pipeline_layout_;

	vk::Pipeline compute_pipeline_;
	vk::Pipeline grass_pipeline_;

	vk::Buffer blades_buffer;
	vk::DeviceMemory blades_buffer_memory;

	vk::Buffer culled_blades_buffer;
	vk::DeviceMemory culled_blades_buffer_memory;

	vk::Buffer indirect_draw_commands_buffer_;
	vk::DeviceMemory indirect_draw_commands_buffer_memory_;

	uint32_t blades_num_ = 0;
};