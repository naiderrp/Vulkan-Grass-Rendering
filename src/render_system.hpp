#pragma once
#include "device_context.hpp"
#include "dimensional.hpp"
#include "camera.hpp"

#include <chrono>

camera camera_;

struct time_data_t {
	float delta_time = 0.0f;
	float total_time = 0.0f;
};

namespace {
	bool	leftMouseDown	= false;
	bool	rightMouseDown	= false;
	double	previousX		= 0.0;
	double	previousY		= 0.0;

	void mouseDownCallback(GLFWwindow* window, int button, int action, int mods) {
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			if (action == GLFW_PRESS) {
				leftMouseDown = true;
				glfwGetCursorPos(window, &previousX, &previousY);
			}
			else if (action == GLFW_RELEASE) {
				leftMouseDown = false;
			}
		}
		else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			if (action == GLFW_PRESS) {
				rightMouseDown = true;
				glfwGetCursorPos(window, &previousX, &previousY);
			}
			else if (action == GLFW_RELEASE) {
				rightMouseDown = false;
			}
		}
	}

	void mouseMoveCallback(GLFWwindow* window, double xPosition, double yPosition) {
		if (leftMouseDown) {
			double sensitivity = 0.5;
			float deltaX = static_cast<float>((previousX - xPosition) * sensitivity);
			float deltaY = static_cast<float>((previousY - yPosition) * sensitivity);

			camera_.update(deltaX, deltaY, 0.0f);

			previousX = xPosition;
			previousY = yPosition;
		}
		else if (rightMouseDown) {
			double deltaZ = static_cast<float>((previousY - yPosition) * 0.05);

			camera_.update(0.0f, 0.0f, deltaZ);

			previousY = yPosition;
		}
	}
}

class render_system {
public:
	void run() {
		glfwSetMouseButtonCallback(GPU_.window_, mouseDownCallback);
		glfwSetCursorPosCallback(GPU_.window_, mouseMoveCallback);

		camera_.set_view_direction(glm::vec3(1.f, 1.f, 1.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.0f, 1.0f, 0.0f));
		
		plane.transform.rotation = { -3.1415 / 2.f, -3.1415 / 2.f, 0. };
		plane.transform.scale = { 30.f, 30.f, 30.f };

		while (!glfwWindowShouldClose(GPU_.window_)) {
			glfwPollEvents();
			update_time();
			draw_frame();
		}
	}

private:
	void record_command_buffer(vk::CommandBuffer& commandBuffer, uint32_t image_index) {
		vk::CommandBufferBeginInfo begin_info{};
		
		commandBuffer.begin(begin_info);
		
		vk::RenderPassBeginInfo render_pass_info{};
		render_pass_info.renderPass = GPU_.render_pass;
		render_pass_info.framebuffer = GPU_.swapchain_framebuffers[image_index];
		render_pass_info.renderArea.offset = vk::Offset2D(0, 0);
		render_pass_info.renderArea.extent = GPU_.swapchain_extent;

		std::array<vk::ClearValue, 2> clear_values{};
		clear_values[0].color = vk::ClearColorValue{ 0.0f, 0.0f, 0.0f, 1.0f };
		clear_values[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

		render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
		render_pass_info.pClearValues = clear_values.data();

		std::vector<vk::BufferMemoryBarrier> compute_barriers(blades.size());

		for (int i = 0; i < blades.size(); ++i) {
			compute_barriers[i].srcAccessMask = vk::AccessFlagBits::eShaderWrite;
			compute_barriers[i].dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead;
			compute_barriers[i].srcQueueFamilyIndex = findQueueFamilies(GPU_.physical_device_, GPU_.surface_).compute_family;
			compute_barriers[i].dstQueueFamilyIndex = findQueueFamilies(GPU_.physical_device_, GPU_.surface_).graphics_family;
			
			compute_barriers[i].buffer = GPU_.indirect_draw_commands_buffer_;
			compute_barriers[i].offset = 0;
			compute_barriers[i].size = sizeof(blade_draw_indirect);
		}

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, compute_barriers, {});

		commandBuffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, GPU_.plane_graphics_pipeline_);

		commandBuffer.bindVertexBuffers(0, { GPU_.plane_vertex_buffer_ }, { 0 });
		commandBuffer.bindIndexBuffer(GPU_.plane_index_buffer_, 0, vk::IndexType::eUint32);

		plane_push_constant plane_push{
			plane.transform.model_matrix(), //model
			camera_.get_view(), //view
			{ camera::get_projection(GPU_.aspect_ratio()) } //proj
		};

		plane_push.projection_matrix[1][1] *= -1;

		commandBuffer.pushConstants(GPU_.plane_pipeline_layout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(plane_push), &plane_push);

		vk::Viewport viewport{};
		viewport.height = static_cast<float>(GPU_.swapchain_extent.height);
		viewport.width = static_cast<float>(GPU_.swapchain_extent.width);
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		commandBuffer.setViewport(0, viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = GPU_.swapchain_extent;

		commandBuffer.setScissor(0, scissor);

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			GPU_.plane_pipeline_layout_,
			0,
			GPU_.descriptor_sets[current_frame],
			{}
		);

		commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		blade_push_constant_data push{
			{glm::mat4(1.0f)}, //model
			{glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f))}, //view
			{ camera::get_projection(GPU_.aspect_ratio()) } //proj
		};
		push.view_matrix = camera_.get_view();
		push.projection_matrix[1][1] *= -1;

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, GPU_.grass_pipeline_);
		
		commandBuffer.bindVertexBuffers(0, GPU_.culled_blades_buffer, { 0 });

		commandBuffer.pushConstants(GPU_.grass_pipeline_layout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &push);

		glm::mat4 matrices[] = { push.view_matrix, push.projection_matrix };
		commandBuffer.pushConstants(GPU_.grass_pipeline_layout_, vk::ShaderStageFlagBits::eTessellationEvaluation, sizeof(glm::mat4), 2 * sizeof(glm::mat4), matrices);

		commandBuffer.drawIndirect(GPU_.indirect_draw_commands_buffer_, 0, 1, sizeof(blade_draw_indirect));

		commandBuffer.endRenderPass();
		commandBuffer.end();
	}

	void update_time() {
		static auto start_time = std::chrono::high_resolution_clock::now();
		auto current_time = std::chrono::high_resolution_clock::now();

		time_.delta_time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
		time_.total_time += time_.delta_time;

		start_time = current_time;
	}

	void record_compute_command_buffer() {
		vk::CommandBufferBeginInfo begin_info{};
		begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

		GPU_.compute_command_buffer_.begin(begin_info);

		GPU_.compute_command_buffer_.bindPipeline(vk::PipelineBindPoint::eCompute, GPU_.compute_pipeline_);

		blade_compute_push_data push{
			camera_.get_view(),
			glm::perspective(glm::radians(90.0f), GPU_.aspect_ratio(), 0.1f, 10.0f),
			time_.delta_time,
			time_.total_time
		};

		push.projection_matrix[1][1] *= -1;

		GPU_.compute_command_buffer_.pushConstants(GPU_.compute_pipeline_layout_, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push), &push);

		GPU_.compute_command_buffer_.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute, 
			GPU_.compute_pipeline_layout_, 
			0, 
			1, 
			&GPU_.compute_descriptor_sets_[0], 
			0, 
			nullptr
		);
		
		const int workgroup_size = 32;
		const int groupcount = ((blades.size()) / workgroup_size) + 1;

		uint32_t count = (blades.size() + workgroup_size - 1) / workgroup_size;
		
		GPU_.compute_command_buffer_.dispatch(count, 1, 1);

		GPU_.compute_command_buffer_.end();
	}

	void draw_frame() {
		GPU_.compute_queue_.waitIdle();

		GPU_.compute_command_buffer_.reset();
		
		record_compute_command_buffer();

		vk::SubmitInfo compute_submit_info{};
		compute_submit_info.commandBufferCount = 1;
		compute_submit_info.pCommandBuffers = &GPU_.compute_command_buffer_;

		GPU_.compute_queue_.submit(compute_submit_info);

		GPU_.logical_device_.waitForFences(GPU_.in_flight_fences[current_frame], true, UINT64_MAX);
		GPU_.logical_device_.resetFences(GPU_.in_flight_fences[current_frame]);

		auto acquire_image_result = GPU_.logical_device_.acquireNextImageKHR(GPU_.swapchain_, UINT64_MAX, GPU_.image_available_semaphores[current_frame]);
		auto image_index = acquire_image_result.value;

		GPU_.command_buffers[current_frame].reset();
		record_command_buffer(GPU_.command_buffers[current_frame], image_index);

		vk::Semaphore wait_semaphores[] = {
			GPU_.image_available_semaphores[current_frame]
		};
		vk::PipelineStageFlags wait_stages[] = {
			vk::PipelineStageFlagBits::eColorAttachmentOutput
		};

		vk::PipelineStageFlags compute_wait_stages[] = {
			vk::PipelineStageFlagBits::eComputeShader
		};

		vk::CommandBuffer buffers_to_submit[] = { GPU_.command_buffers[current_frame], GPU_.compute_command_buffer_ };

		vk::SubmitInfo submit_info{};
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &GPU_.command_buffers[current_frame];

		vk::Semaphore signal_semaphores[] = {
			GPU_.render_finished_semaphores[current_frame]
		};
		submit_info.pSignalSemaphores = signal_semaphores;
		submit_info.signalSemaphoreCount = 1;

		GPU_.graphics_queue_.submit(submit_info, GPU_.in_flight_fences[current_frame]);

		vk::PresentInfoKHR present_info{};
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;
		present_info.pSwapchains = &GPU_.swapchain_;
		present_info.swapchainCount = 1;
		present_info.pImageIndices = &image_index;

		auto present_result = GPU_.present_queue_.presentKHR(present_info); 
		/*if (present_result == vk::Result::eErrorOutOfDateKHR) {
			recreate_swapchain();
			return;
		}*/

		++current_frame %= MAX_FRAMES_IN_FLIGHT;
	}

private:
	uint32_t current_frame;

	std::vector<vertex> vertices = {
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
	};

	std::vector<uint32_t> indices = {
		0, 1, 2, 2, 3, 0
	};

	std::vector<blade> blades = grass::generate_terrain(4000);

	dimensional plane{ vertices, indices };

	device_context GPU_{ plane.vertices, plane.indices, blades };

	time_data_t time_;
};
