#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <vector>

namespace ew {
	struct Vec3KeyFrame {
		glm::vec3 value;
		float time;
	};
	struct QuatKeyFrame {
		glm::quat value;
		float time;
	};
	struct BoneAnimation {
		std::vector<Vec3KeyFrame> positionKeyFrames;
		std::vector<QuatKeyFrame> rotationKeyFrames;
		std::vector<Vec3KeyFrame> scaleKeyFrames;
	};
	struct AnimationClip {
		float duration;
		int ticksPerSecond;
		std::vector<BoneAnimation> bones;
	};
	struct Bone {
		glm::mat4 inverseBindPose; //Model space -> bone space in bind pose
		glm::mat4 localTransform; //Transform relative to parent
		int parentIndex;
	};
	struct Skeleton {
		std::vector<Bone> bones;
	};
	AnimationClip loadAnimationFromFile(const char* filePath);
}