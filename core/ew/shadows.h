#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace ew {
	glm::vec3 frustumCenter(const std::vector<glm::vec4>& frustumCorners);
	std::vector<glm::vec4> frustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
	glm::mat4 shadowViewMatrix(const glm::vec3& center, const glm::vec3& lightDir);
	glm::mat4 shadowOrthoProjection(const std::vector<glm::vec4>& frustumCorners, const glm::mat4 view, const float zMult);
}