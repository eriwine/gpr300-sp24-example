#include "shadows.h"
#include <glm/gtc/matrix_transform.hpp>

namespace ew {
	/// <summary>
	/// Calculates 8 frustum corners
	/// </summary>
	/// <param name="proj"></param>
	/// <param name="view"></param>
	/// <returns></returns>
	std::vector<glm::vec4> frustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
		glm::mat4 inv = glm::inverse(proj * view);
		std::vector<glm::vec4> frustumCorners;
		for (size_t x = 0; x < 2; x++)
		{
			for (size_t y = 0; y < 2; y++)
			{
				for (size_t z = 0; z < 2; z++)
				{
					const glm::vec4 pt = inv * glm::vec4(
						2.0f * x - 1.0f,
						2.0f * y - 1.0f,
						2.0f * z - 1.0f,
						1.0f);
					frustumCorners.push_back(pt / pt.w);
				}
			}
		}
		return frustumCorners;
	}

	/// <summary>
	/// Calculates center of a frustum
	/// </summary>
	/// <param name="frustumCorners"></param>
	/// <returns></returns>
	glm::vec3 frustumCenter(const std::vector<glm::vec4>& frustumCorners) {
		glm::vec3 center = glm::vec3(0);
		for (const glm::vec3& v : frustumCorners) {
			center += glm::vec3(v);
		}
		center /= frustumCorners.size();
		return center;
	}

	/// <summary>
	/// Calculates a view matrix that looks at the center of a frustum
	/// </summary>
	/// <param name="frustumCorners"></param>
	/// <param name="lightDir"></param>
	/// <returns></returns>
	glm::mat4 shadowViewMatrix(const glm::vec3& center, const glm::vec3& lightDir) {
		//Look at center of frustum
		return glm::lookAt(
			center + lightDir,
			center,
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
	}

	/// <summary>
	/// Calculates an orthographic projection matrix that tightly fits a camera frustum
	/// </summary>
	/// <param name="frustumCorners"></param>
	/// <param name="view"></param>
	/// <param name="zMult"></param>
	/// <returns></returns>
	glm::mat4 shadowOrthoProjection(const std::vector<glm::vec4>& frustumCorners,
		const glm::mat4 view, const float zMult) {

		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		float minZ = std::numeric_limits<float>::max();
		float maxZ = std::numeric_limits<float>::lowest();
		for (const auto& v : frustumCorners)
		{
			const auto trf = view * v;
			minX = std::min(minX, trf.x);
			maxX = std::max(maxX, trf.x);
			minY = std::min(minY, trf.y);
			maxY = std::max(maxY, trf.y);
			minZ = std::min(minZ, trf.z);
			maxZ = std::max(maxZ, trf.z);
		}
		if (minZ < 0)
		{
			minZ *= zMult;
		}
		else
		{
			minZ /= zMult;
		}
		if (maxZ < 0)
		{
			maxZ /= zMult;
		}
		else
		{
			maxZ *= zMult;
		}

		const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
		return lightProjection * view;
	}
}
