#include "config.hpp"

struct blade_push_constant_data {
	// vertex shader
	alignas(16) glm::mat4 model_matrix;

	// tessellation evaluation shader
	alignas(16) glm::mat4 view_matrix;
	alignas(16) glm::mat4 projection_matrix;
};

struct blade_compute_push_data {
	alignas(16) glm::mat4 view_matrix;
	alignas(16) glm::mat4 projection_matrix;

	float		delta_time;
	float		total_time;
};

struct blade {
	glm::vec4 v0; // v0.w is direction_angle
	glm::vec4 v1; // v1.w is height
	glm::vec4 v2; // v2.w is width
	glm::vec4 up; // up.w is stiffness

public:
	static constexpr auto binding_description() {
		vk::VertexInputBindingDescription binding_description{};
		binding_description.binding = 0;
		binding_description.inputRate = vk::VertexInputRate::eVertex;
		binding_description.stride = sizeof(blade);

		return binding_description;
	}

	static constexpr auto attribute_descriptions() {
		std::vector<vk::VertexInputAttributeDescription> attribute_description(4);
		attribute_description[0].binding = 0;
		attribute_description[0].location = 0;
		attribute_description[0].format = vk::Format::eR32G32B32A32Sfloat;
		attribute_description[0].offset = offsetof(blade, v0);

		attribute_description[1].binding = 0;
		attribute_description[1].location = 1;
		attribute_description[1].format = vk::Format::eR32G32B32A32Sfloat;
		attribute_description[1].offset = offsetof(blade, v1);

		attribute_description[2].binding = 0;
		attribute_description[2].location = 2;
		attribute_description[2].format = vk::Format::eR32G32B32A32Sfloat;
		attribute_description[2].offset = offsetof(blade, v2);

		attribute_description[3].binding = 0;
		attribute_description[3].location = 3;
		attribute_description[3].format = vk::Format::eR32G32B32A32Sfloat;
		attribute_description[3].offset = offsetof(blade, up);

		return attribute_description;
	}
};

struct blade_draw_indirect {
	uint32_t vertex_count;
	uint32_t instance_count;
	uint32_t first_vertex;
	uint32_t first_instance;
};

struct grass {
	static constexpr auto generate_terrain() -> std::vector<blade> {
		// just a single blade
		std::vector<blade> blades;

		const float width = 2.f;
		const float height = 5.f;
		const float stiffness = 2.5f;
		constexpr float direction_angle = glm::pi<float>() / 2.f;

		glm::vec3 up{ 0.f, 1.f, 0.f };
		glm::vec3 initial_position{ 0.5, 0.f, 0.0 };

		blade b{
			{initial_position, direction_angle},		// v0
			{initial_position + up * height, height},	// v1
			{initial_position + up * height, width},	// v2
			{up, stiffness}								// up
		};

		blades.emplace_back(b);

		return blades;
	}

	static auto generate_terrain_tobin_heart(const unsigned int num_blades, const float plane_dim = 30.f) -> std::vector<blade> {
		const float min_height = 2.5f; // 3
		const float max_height = 4.f; // 5

		const float min_width = 0.14f; // 1.5
		const float max_width = 0.44f; // 3.5

		const float min_bend = 5.0f; // 1
		const float max_bend = 10.0f; // 2.5

		std::vector<blade> blades(num_blades);

		glm::vec3 up{ 0.f, 1.f, 0.f };

		for (int i = 0; i < num_blades; ++i) {
			// Parametric equations for 3D Tobin heart
			const float t = random_float() * 2.f * glm::pi<float>();
			const float s = random_float() * 2.f * glm::pi<float>();
			const float x = 16.f * sin(t) * sin(t) * sin(t);
			const float y = 13.f * cos(t) - 5.f * cos(2 * t) - 2.f * cos(3 * t) - cos(4 * t);
			const float z = 16.f * sin(t) * sin(t) * sin(t) * cos(s);

			up = glm::normalize(glm::vec3(x, y, z) - glm::vec3(0, 0, 0));

			const float direction_angle = random_float() * 2.f * glm::pi<float>();

			glm::vec3 initial_position{ x, y, z };

			const float width = random_float() * (max_width - min_width) + min_width;
			const float height = random_float() * (max_height - min_height) + min_height;
			const float stiffness = random_float() * (max_bend - min_bend) + min_bend;


			blade b{
				{initial_position, direction_angle},        // v0
				{initial_position + up * height, height},   // v1
				{initial_position + up * height, width},    // v2
				{up, stiffness}                             // up
			};

			blades[i] = std::move(b);
		}

		return blades;
	}

	static auto generate_terrain(const unsigned int num_blades, const float plane_dim = 30.f) -> std::vector<blade> {
		const float min_height = 2.5f; // 3
		const float max_height = 4.f; // 5
		
		const float min_width = 0.14f; // 1.5
		const float max_width = 0.44f; // 3.5

		const float min_bend = 5.0f; // 1 
		const float max_bend = 10.0f; // 2.5

		std::vector<blade> blades(num_blades);

		glm::vec3 up{ 0.f, 1.f, 0.f };

		for(int i = 0; i < num_blades; ++i) {
			// sphere:
			//const float theta = random_float() * 2.f * glm::pi<float>();

			//float u = random_float() * 2 - 1;
			//float x = sqrt(1 - u * u) * cos(theta);
			//float y = sqrt(1 - u * u) * sin(theta) + 4;
			//float z = u;
			//up = glm::normalize(glm::vec3(x, y, z) - glm::vec3(0, 4, 0));

			const float x = (random_float() - 0.5f) * plane_dim;
			const float y = 0.0f;
			const float z = (random_float() - 0.5f) * plane_dim;

			const float direction_angle = random_float() * 2.f * glm::pi<float>();
			
			//constexpr float direction_angle = glm::pi<float>() / 2.f; // for a culling demo

			glm::vec3 initial_position{ x, y, z };

			const float width = random_float() * (max_width - min_width) + min_width;
			const float height = random_float() * (max_height - min_height) + min_height;
			const float stiffness = random_float() * (max_bend - min_bend) + min_bend;;

			blade b{
				{initial_position, direction_angle},		// v0
				{initial_position + up * height, height},	// v1
				{initial_position + up * height, width},	// v2
				{up, stiffness}								// up
			};

			blades[i] = std::move(b); 
		}

		return blades;
	}

private:
	static float random_float() {
		return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}
};