#pragma once
#include "config.hpp"
#include <glm/glm.hpp>

struct vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 tex_coord;

	static vk::VertexInputBindingDescription get_binding_description() {
		vk::VertexInputBindingDescription binding_description{};
		binding_description.binding = 0;
		binding_description.stride = sizeof(vertex);
		binding_description.inputRate = vk::VertexInputRate::eVertex;

		return binding_description;
	}

	static std::vector<vk::VertexInputAttributeDescription> get_attribute_descriptions() {
		std::vector<vk::VertexInputAttributeDescription> attribute_descriptions;

		attribute_descriptions.emplace_back(
			0,								//location
			0,								//binding: from which binding the per-vertex data comes
			vk::Format::eR32G32B32Sfloat,	//format
			offsetof(vertex, pos)			//offset: the number of bytes since the start of the per-vertex data to read from
		);


		attribute_descriptions.emplace_back(
			1,								//location
			0,								//binding
			vk::Format::eR32G32B32Sfloat,	//format
			offsetof(vertex, color)			//offset
		);

		attribute_descriptions.emplace_back(
			2,									// location
			0,									// binding
			vk::Format::eR32G32Sfloat,			// format 
			offsetof(vertex, tex_coord)			// offset
		);

		return attribute_descriptions;
	}

	bool operator==(const vertex &other) const {
		return pos == other.pos && color == other.color && tex_coord == other.tex_coord;
	}
};

struct uniform_buffer_object {
	glm::vec2 align_test;
	alignas(16) glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};