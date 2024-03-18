#pragma once
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

// the std::hash functions for the GLM types
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <memory>
#include <functional>
#include <iostream>
#include <chrono> //does precies timekeeping

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>