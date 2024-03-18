#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <limits>
#include <cassert>

class camera {
public:
	void set_view_direction(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& up = glm::vec3{ .0f, -1.f, .0f }) {
		view_matrix_ = glm::lookAt(position, direction, up);
		return;
	}

	void update(float delta_x, float delta_y, float delta_z) {
		theta += delta_x;
		phi += delta_y;
		r = glm::clamp(r - delta_z, 1.0f, 50.0f);

		float radTheta = glm::radians(theta);
		float radPhi = glm::radians(phi);

		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radTheta, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), radPhi, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 final_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)) * rotation * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, r));

		view_matrix_ = glm::inverse(final_transform);
	}

	static auto get_projection(float aspect_ratio) {
		return glm::perspective(glm::radians(90.0f), aspect_ratio, 0.1f, 100.0f);
	}

public:
	const auto get_projection() const {
		return projection_matrix_;
	}

	const auto get_view() const {
		return view_matrix_;
	}
private:
	glm::mat4 projection_matrix_{ 1.f };
	glm::mat4 view_matrix_{ 1.f };

	float r; 
	float theta; 
	float phi;
};