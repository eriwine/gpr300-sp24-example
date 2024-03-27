/*
*	Author: Eric Winebrenner
*/

#pragma once
#include "mesh.h"
#include "shader.h"
#include <vector>
#include <map>

namespace ew {
	struct BoneInfo {
		glm::mat4 invBindPose;
	};
	class Model {
	public:
		Model();
		Model(const std::string& filePath);
		void draw();
	private:
		std::vector<ew::Mesh> m_meshes;
		std::map<std::string, ew::BoneInfo> m_boneInfoMap;
	};
}