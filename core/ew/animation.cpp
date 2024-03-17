#include "animation.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace ew {
	Vec3KeyFrame convertVec3Key(const aiVectorKey& aiKey) {
		Vec3KeyFrame key;
		key.value = glm::vec3(aiKey.mValue.x, aiKey.mValue.y, aiKey.mValue.z);
		key.time = aiKey.mTime;
		return key;
	}
	QuatKeyFrame convertQuatKey(const aiQuatKey& aiKey) {
		QuatKeyFrame key;
		key.value = glm::quat(aiKey.mValue.w,aiKey.mValue.x,aiKey.mValue.y,aiKey.mValue.z);
		key.time = aiKey.mTime;
		return key;
	}
	glm::mat4 convertMat4(const aiMatrix4x4& aiMat) {
		return glm::transpose(glm::mat4(
			aiMat.a1, aiMat.a2, aiMat.a3, aiMat.a4,
			aiMat.b1, aiMat.b2, aiMat.b3, aiMat.b4,
			aiMat.c1, aiMat.c2, aiMat.c3, aiMat.c4,
			aiMat.d1, aiMat.d2, aiMat.d3, aiMat.d4
		));
	}
	BoneAnimation loadBoneAnimation(const aiNodeAnim* aiNodeAnim) {
		BoneAnimation boneAnim;
		//Load position keyframes
		{
			const size_t numPositionKeys = aiNodeAnim->mNumPositionKeys;
			boneAnim.positionKeyFrames.reserve(numPositionKeys);
			for (size_t i = 0; i < numPositionKeys; i++)
			{
				boneAnim.positionKeyFrames.emplace_back(convertVec3Key(aiNodeAnim->mPositionKeys[i]));
			}
		}
		
		//Load rotation keyframes
		{
			const size_t numRotationKeys = aiNodeAnim->mNumRotationKeys;
			boneAnim.rotationKeyFrames.reserve(numRotationKeys);
			for (size_t i = 0; i < numRotationKeys; i++)
			{
				boneAnim.rotationKeyFrames.emplace_back(convertQuatKey(aiNodeAnim->mRotationKeys[i]));
			}
		}
		
		//Load scale keyframes
		{
			const size_t numScaleKeys = aiNodeAnim->mNumScalingKeys;
			boneAnim.scaleKeyFrames.reserve(numScaleKeys);
			for (size_t i = 0; i < numScaleKeys; i++)
			{
				boneAnim.scaleKeyFrames.emplace_back(convertVec3Key(aiNodeAnim->mScalingKeys[i]));
			}
		}
		boneAnim.name = std::string(aiNodeAnim->mNodeName.C_Str());
		return boneAnim;
	}

	/// <summary>
	/// Recursively populates boneList in order
	/// </summary>
	/// <param name="node"></param>
	/// <param name="boneList"></param>
	void loadAssimpSkeletonRecursive(aiNode* node, std::vector<aiNode*>* boneList) {
		//Skip nodes with meshes
		//We are only looking for the armature
		if (node->mNumMeshes == 0)
		{
			boneList->push_back(node);
		}
		for (size_t i = 0; i < node->mNumChildren; i++)
		{
			//if no meshes, assume this is just a bone
			loadAssimpSkeletonRecursive(node->mChildren[i], boneList);
		}
	}

	Skeleton loadSkeleton(aiNode* rootNode) {
		Skeleton skeleton;
		std::vector<aiNode*> nodeList;
		loadAssimpSkeletonRecursive(rootNode, &nodeList);
		//Keep sorted list of nodes to find parent indices
		size_t numBones = nodeList.size();
		for (size_t i = 0; i < numBones; i++)
		{
		//	const aiBone* aiBone = aiMesh->mBones[i];
			aiNode* node = nodeList[i];
			Bone bone;
			bone.name = node->mName.C_Str();
			//bone.inverseBindPose = convertMat4(aiBone->mOffsetMatrix);
			bone.localTransform = convertMat4(node->mTransformation);
			//parentIndex is index in sorted list
			bone.parentIndex = std::distance(nodeList.begin(), std::find(nodeList.begin(), nodeList.end(), node->mParent));
			if (bone.parentIndex == nodeList.size()) {
				bone.parentIndex = -1;
			}
			skeleton.bones.push_back(bone);
		}
		
		return skeleton;
	}
	//Loads a single animation from file
	AnimatedSkeletonPackage loadAnimationFromFile(const char* filePath)
	{
		AnimatedSkeletonPackage package;

		AnimationClip animClip;

		//Load scene from file.
		//Contains animations AND skeleton (armature)
		Assimp::Importer importer;
		const aiScene* aiScene = importer.ReadFile(filePath, aiProcess_PopulateArmatureData);

		if (aiScene == NULL) {
			printf("Failed to load file %s", filePath);
			return package;
		}
		if (!aiScene->HasAnimations()) {
			printf("File does not contain animations %s", filePath);
			return package;
		}
		//Get first animation clip
		aiAnimation* aiAnim = aiScene->mAnimations[0];
		animClip.bones.reserve(aiAnim->mNumChannels);
		//Load all bone animations
		for (size_t i = 0; i < aiAnim->mNumChannels; i++)
		{
			animClip.bones.emplace_back(loadBoneAnimation(aiAnim->mChannels[i]));
		}
		animClip.duration = aiAnim->mDuration;
		animClip.ticksPerSecond = aiAnim->mTicksPerSecond;

		package.skeleton = loadSkeleton(aiScene->mRootNode);
		package.animationClip = animClip;
		return package;
	}

	void solveFK(const ew::Skeleton& skeleton, std::vector<glm::mat4>& worldMatrices) {
		const size_t numBones = skeleton.bones.size();

		worldMatrices.reserve(numBones);
		worldMatrices.clear();
		
		for (size_t i = 0; i < numBones; i++)
		{
			const ew::Bone& thisBone = skeleton.bones[i];
			if (thisBone.parentIndex == -1) {
				worldMatrices.emplace_back(thisBone.localTransform);
				continue;
			}
			const ew::Bone& parentBone = skeleton.bones[thisBone.parentIndex];
			worldMatrices.emplace_back(worldMatrices[thisBone.parentIndex] * thisBone.localTransform);
		}
		return;
	}

	glm::vec3 lerp(const glm::vec3& x, const glm::vec3& y, float t) {
		return x * (1.f - t) + y * t;
	}

	glm::vec3 lerpVec3KeyFrames(const std::vector<ew::Vec3KeyFrame>& keyFrames, float time) {
		//Interpolate Positions
		ew::Vec3KeyFrame prevKeyFrame, nextKeyFrame;

		//Find 2 keyframes to interpolate between
		const size_t numKeyFrames = keyFrames.size();
		for (size_t i = 0; i < numKeyFrames; i++)
		{
			if (keyFrames[i].time < time) {
				prevKeyFrame = keyFrames[i];
				//Loop around if beyond last keyframe
				nextKeyFrame = (i < numKeyFrames) ? keyFrames[i + 1] : keyFrames[0];
				break;
			}
		}
		//Inverse lerp to get t = (0-1)
		float t = (time - prevKeyFrame.time) / (nextKeyFrame.time - prevKeyFrame.time);
		//Lerp to get value
		return ew::lerp(prevKeyFrame.value, nextKeyFrame.value, t);
	}

	void updateSkeleton(ew::Skeleton* skeleton, ew::AnimationClip* animClip, float time) {
		for (size_t i = 0; i < animClip->bones.size(); i++)
		{
			const BoneAnimation& boneAnim = animClip->bones[i];

			glm::vec3 interpolatedPos = lerpVec3KeyFrames(boneAnim.positionKeyFrames, time);

			glm::mat4 translation = glm::translate(glm::mat4(1), interpolatedPos);

			//Interpolate Rotation

			//Interpolate 

			skeleton->bones[i].localTransform = translation;
		}
	}
}