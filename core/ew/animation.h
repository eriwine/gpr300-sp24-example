#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <vector>
#include <string>

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
		std::string name;
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
		std::string name;
		int parentIndex;
		glm::mat4 inverseBindPose; //Model space -> bone space in bind pose
		glm::mat4 localTransform; //Transform relative to parent in bind pose
	};
	struct Skeleton {
		std::vector<Bone> bones;
	};
	struct AnimatedSkeletonPackage {
		Skeleton skeleton;
		AnimationClip animationClip;
	};
	AnimatedSkeletonPackage loadAnimationFromFile(const char* filePath);

	void solveFK(const ew::Skeleton& skeleton, std::vector<glm::mat4>& worldMatrices);
	void updateSkeleton(ew::Skeleton* skeleton, ew::AnimationClip* animClip, float time);
}