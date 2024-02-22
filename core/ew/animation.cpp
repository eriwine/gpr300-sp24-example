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
		return glm::mat4(
			aiMat.a1, aiMat.a2, aiMat.a3, aiMat.a4,
			aiMat.b1, aiMat.b2, aiMat.b3, aiMat.b4,
			aiMat.c1, aiMat.c2, aiMat.c3, aiMat.c4,
			aiMat.d1, aiMat.d2, aiMat.d3, aiMat.d4
			);
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
		return boneAnim;
	}
	Skeleton loadSkeleton(const aiMesh* aiMesh) {
		Skeleton skeleton;
		if (!aiMesh->HasBones()) {
			printf("Mesh has no bones\n");
			return skeleton;
		}
		for (size_t i = 0; i < aiMesh->mNumBones; i++)
		{
			const aiBone* aiBone = aiMesh->mBones[i];
			Bone bone;
			bone.inverseBindPose = convertMat4(aiBone->mOffsetMatrix);
		}
		
		return skeleton;
	}
	//Loads a single animation from file
	AnimationClip loadAnimationFromFile(const char* filePath)
	{
		AnimationClip animClip;

		Assimp::Importer importer;
		const aiScene* aiScene = importer.ReadFile(filePath, aiProcess_PopulateArmatureData);
		if (aiScene == NULL) {
			printf("Failed to load file %s", filePath);
			return animClip;
		}
		if (!aiScene->HasAnimations()) {
			printf("File does not contain animations %s", filePath);
			return animClip;
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

		if (aiScene->HasMeshes()) {
			Skeleton skeleton = loadSkeleton(aiScene->mMeshes[0]);
		}
		
		return animClip;
	}
}