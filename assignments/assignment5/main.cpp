#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>
#include<ew/shader.h>
#include<ew/model.h>
#include<ew/camera.h>
#include<ew/transform.h>
#include<ew/cameraController.h>
#include<ew/texture.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ew/framebuffer.h>
#include <ew/procGen.h>

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();

ew::CameraController cameraController;
ew::Camera mainCamera;


//Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;
struct Material {
	float Ka = 1.0;
	float Kd = 0.5;
	float Ks = 0.5;
	float Shininess = 128;
}material;

struct Light {
	glm::vec3 direction = glm::vec3(-0.77f, -0.77f, 0);
	glm::vec3 color = glm::vec3(1.0f);
}mainLight;

bool postProcessEnabled;

ew::Transform monkeyTransform;
ew::Transform planeTransform;

//Assets
ew::Model monkeyModel;
ew::Mesh planeMesh;
GLuint stoneColorTexture;
GLuint stoneNormalTexture;
GLuint goldColorTexture;
GLuint goldNormalTexture;

ew::Framebuffer framebuffer;

//Shadows
ew::Framebuffer shadowFBO;
ew::Camera shadowCamera;
struct ShadowSettings {
	float camDistance = 10.0f;
	float minBias = 0.005f;
	float maxBias = 0.01f;
}shadowSettings;

const int SHADOW_RESOLUTION = 512;

namespace ew {
	struct Joint {
		std::string m_name;
		int m_parentID;
	};
	struct Skeleton {
		std::vector<Joint> m_joints;
	};
	struct Pose {
		glm::vec3 position = glm::vec3(0);
		glm::quat rotation = glm::quat(1, 0, 0, 0);
		glm::vec3 scale = glm::vec3(1);
	};
	struct KeyFrame {
		float time;
		Pose pose;
	};
	struct AnimClip {
		float m_duration;
		std::vector<KeyFrame> m_keyFrames;
	};
	struct Animator {
		AnimClip* m_animClip;
		Transform* m_transform;
		bool m_playing = true;
		float m_playbackSpeed = 1.0f;
		bool m_loop = true;
		float m_playbackTime = 0.0f;
	};
	Animator* initAnimator(AnimClip* animClip) {
		Animator* animator = new Animator();
		animator->m_animClip = animClip;
		return animator;
	}
	float inverseLerp(float a, float b, float v) {
		return (v - a) / (b - a);
	}
	void interpolatePose(Transform* transform, const KeyFrame& prevKey, const KeyFrame& nextKey, float time) {
		float normT = inverseLerp(prevKey.time, nextKey.time, time);
		glm::vec3 position = glm::mix(prevKey.pose.position, nextKey.pose.position, normT);
		glm::quat rotation = glm::slerp(prevKey.pose.rotation, nextKey.pose.rotation, normT);
		glm::vec3 scale = glm::mix(prevKey.pose.scale, nextKey.pose.scale, normT);
		transform->position = position;
		transform->rotation = rotation;
		transform->scale = scale;
	}
	void updateAnimator(Animator* animator, float deltaTime) {

		if (animator->m_playing) {
			animator->m_playbackTime += deltaTime * animator->m_playbackSpeed;
			//Loop or stop at end time
			if (animator->m_playbackTime > animator->m_animClip->m_duration) {
				if (animator->m_loop) {
					animator->m_playbackTime = 0;
				}
				else {
					animator->m_playbackTime = animator->m_animClip->m_duration;
				}
			}
		}

		//Find 2 keyframes on either side of playback time
		const auto& keyFrames = animator->m_animClip->m_keyFrames;
		KeyFrame prevKeyFrame = keyFrames[0];
		KeyFrame nextKeyFrame = keyFrames[1];
		size_t numKeyFrames = keyFrames.size();
		for (size_t i = 0; i < numKeyFrames; i++)
		{
			if (keyFrames[i].time >= animator->m_playbackTime) {
				nextKeyFrame = keyFrames[i];
				//Loop if this is first keyframe
				prevKeyFrame = i > 0 ? keyFrames[i - 1] : keyFrames[numKeyFrames - 1];
				break;
			}
		}
		interpolatePose(animator->m_transform,prevKeyFrame, nextKeyFrame, animator->m_playbackTime);
	}
}

void drawScene(ew::Camera& camera, ew::Shader& shader) {
	shader.use();
	shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

	glBindTextureUnit(0, goldColorTexture);
	glBindTextureUnit(1, goldNormalTexture);
	shader.setMat4("_Model", monkeyTransform.modelMatrix());
	monkeyModel.draw();

	glBindTextureUnit(0, stoneColorTexture);
	glBindTextureUnit(1, stoneNormalTexture);
	shader.setMat4("_Model", planeTransform.modelMatrix());
	planeMesh.draw();
}

ew::Animator* animator;
ew::AnimClip* animClip;

int main() {
	GLFWwindow* window = initWindow("Assignment 2", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	stoneColorTexture = ew::loadTexture("assets/textures/stones_color.png",true);
	stoneNormalTexture = ew::loadTexture("assets/textures/stones_normal.png");
	goldColorTexture = ew::loadTexture("assets/textures/gold_color.png", true);
	goldNormalTexture = ew::loadTexture("assets/textures/gold_normal.png");

	ew::Shader litShader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader depthOnlyShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader postProcessShader = ew::Shader("assets/fsTriangle.vert", "assets/tonemapping.frag");

	//Load models
	monkeyModel = ew::Model("assets/Suzanne.obj");
	planeMesh = ew::Mesh(ew::createPlane(10, 10, 1));

	planeTransform.position = glm::vec3(-5, -2, -5);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK); //Back face culling
	glEnable(GL_DEPTH_TEST); //Depth testing

	//Initialize cameras
	mainCamera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	mainCamera.target = glm::vec3(0.0f, 0.0f, 0.0f); //Look at the center of the scene
	mainCamera.aspectRatio = (float)screenWidth / screenHeight;
	mainCamera.fov = 60.0f; //Vertical field of view, in degrees

	//Orthographic shadow camera for directional light
	shadowCamera.aspectRatio = 1;
	shadowCamera.farPlane = 50;
	shadowCamera.nearPlane = 1.0;
	shadowCamera.orthoHeight = 10.0f;
	shadowCamera.orthographic = true;

	//Initialize framebuffers
	framebuffer = ew::createFramebuffer(screenWidth, screenHeight, GL_RGBA16F);
	shadowFBO = ew::createDepthOnlyFramebuffer(SHADOW_RESOLUTION, SHADOW_RESOLUTION);

	//Used for supplying indices to vertex shaders
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);


	//Initial animation clip
	//TODO: Read from file or whatever
	animClip = new ew::AnimClip();
	animClip->m_duration = 2;

	ew::KeyFrame keyFrame0;
	keyFrame0.time = 0;
	keyFrame0.pose.rotation = glm::quat(glm::vec3(0, 0, 0));

	ew::KeyFrame keyFrame1;
	keyFrame1.time = 2;
	keyFrame1.pose.position = glm::vec3(0, 2, 0);
	keyFrame1.pose.rotation = glm::quat(glm::vec3(0, 90, 0));

	animClip->m_keyFrames.push_back(keyFrame0);
	animClip->m_keyFrames.push_back(keyFrame1);

	animator = new ew::Animator();
	animator->m_animClip = animClip;
	animator->m_transform = &monkeyTransform;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		cameraController.move(window, &mainCamera, deltaTime);

		ew::updateAnimator(animator, deltaTime);
		//Spin the monkey
		//monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));

		//RENDER SHADOW MAP
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO.fbo);
		glViewport(0, 0, shadowFBO.width, shadowFBO.height);
		glClear(GL_DEPTH_BUFFER_BIT);
		shadowCamera.target = glm::vec3(0);
		shadowCamera.position = normalize(-mainLight.direction) * shadowSettings.camDistance;
		glCullFace(GL_FRONT);
		drawScene(shadowCamera,depthOnlyShader);

		//RENDER SCENE TO HDR BUFFER
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
		glViewport(0, 0, framebuffer.width, framebuffer.height);
		glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_BACK);
		//Bind textures
		glBindTextureUnit(2, shadowFBO.depthBuffer);

		litShader.use();
		litShader.setInt("_MainTex", 0);
		litShader.setInt("_NormalMap", 1);
		litShader.setInt("_ShadowMap", 2);
		litShader.setFloat("_Material.Ka", material.Ka);
		litShader.setFloat("_Material.Kd", material.Kd);
		litShader.setFloat("_Material.Ks", material.Ks);
		litShader.setFloat("_Material.Shininess", material.Shininess);
		litShader.setVec3("_EyePos", mainCamera.position);
		litShader.setVec3("_MainLight.color", mainLight.color);
		litShader.setVec3("_MainLight.direction", mainLight.direction);
		litShader.setFloat("_MinBias", shadowSettings.minBias);
		litShader.setFloat("_MaxBias", shadowSettings.maxBias);
		litShader.setMat4("_LightTransform", shadowCamera.projectionMatrix() * shadowCamera.viewMatrix());

		drawScene(mainCamera,litShader);

		//Draw to screen
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screenWidth, screenHeight);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindTextureUnit(0, framebuffer.colorBuffers[0]);
		postProcessShader.use();
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		drawUI();

		glfwSwapBuffers(window);
	}
	printf("Shutting down...");
}
void resetCamera(ew::Camera* mainCamera, ew::CameraController* controller) {
	mainCamera->position = glm::vec3(0, 0, 5.0f);
	mainCamera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}

void drawUI() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings"); {
		if (ImGui::Button("Reset Camera")) {
			resetCamera(&mainCamera, &cameraController);
		}
		if (ImGui::CollapsingHeader("Material")) {
			ImGui::SliderFloat("AmbientK", &material.Ka, 0.0f, 1.0f);
			ImGui::SliderFloat("DiffuseK", &material.Kd, 0.0f, 1.0f);
			ImGui::SliderFloat("SpecularK", &material.Ks, 0.0f, 1.0f);
			ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
		}
		if (ImGui::CollapsingHeader("Light")) {
			ImGui::ColorEdit3("Color", &mainLight.color.r);
			ImGui::SliderFloat3("Direction", &mainLight.direction.x, -1.0f, 1.0f);
		}
		if (ImGui::CollapsingHeader("Shadows")) {
			ImGui::DragFloat("Shadow cam distance", &shadowSettings.camDistance);
			ImGui::DragFloat("Shadow cam size", &shadowCamera.orthoHeight);
			ImGui::DragFloat("Shadow min bias", &shadowSettings.minBias, 0.01f);
			ImGui::DragFloat("Shadow max bias", &shadowSettings.maxBias, 0.01f);
		}
	}
	ImGui::End();

	ImGui::Begin("Shadow Map"); {
		//Using a Child allow to fill all the space of the window.
		ImGui::BeginChild("ShadowMap");
		//Stretch image to be window size
		ImVec2 windowSize = ImGui::GetWindowSize();
		//Invert 0-1 V to flip vertically. ImGui::Image expects V = 0 to be on top.
		ImGui::Image((ImTextureID)shadowFBO.depthBuffer, windowSize, ImVec2(0, 1), ImVec2(1, 0));
		ImGui::EndChild();

	}
	ImGui::End();

	ImGui::Begin("Animation"); {
		if (ImGui::CollapsingHeader("Transform")) {
			ImGui::DragFloat3("Position", &animator->m_transform->position.x, 0.1f);
			ImGui::DragFloat4("Rotation", &animator->m_transform->rotation.x, 0.1f);
			ImGui::DragFloat3("Scale", &animator->m_transform->scale.x, 0.1f);
		}
		
		ImGui::SliderFloat("Playback Time", &animator->m_playbackTime, 0, animator->m_animClip->m_duration);
		if (ImGui::DragFloat("Animation Duration", &animator->m_animClip->m_duration)) {
			animator->m_animClip->m_duration = glm::max(animator->m_animClip->m_duration, 0.0f);
		}
		ImGui::Checkbox("Playing", &animator->m_playing);
		ImGui::DragFloat("Anim Speed", &animator->m_playbackSpeed, 0.1f);
	

		for (size_t i = 0; i < animator->m_animClip->m_keyFrames.size(); i++)
		{
			ImGui::PushID(i);
			if (ImGui::CollapsingHeader(("KeyFrame"+std::to_string(i)).c_str())) {
				ImGui::DragFloat("Time", &animator->m_animClip->m_keyFrames[i].time, 0.05f);
				ImGui::DragFloat3("Position", &animator->m_animClip->m_keyFrames[i].pose.position.x, 0.1f);
				ImGui::DragFloat4("Rotation", &animator->m_animClip->m_keyFrames[i].pose.rotation.x, 0.1f);
				ImGui::DragFloat3("Scale", &animator->m_animClip->m_keyFrames[i].pose.scale.x, 0.1f);
			}
			ImGui::PopID();
		}
		if (ImGui::Button("Add Keyframe")) {
			animator->m_animClip->m_keyFrames.emplace_back(ew::KeyFrame());
		}
	}
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	screenWidth = width;
	screenHeight = height;
}

/// <summary>
/// Initializes GLFW, GLAD, and IMGUI
/// </summary>
/// <param name="title">Window title</param>
/// <param name="width">Window width</param>
/// <param name="height">Window height</param>
/// <returns>Returns window handle on success or null on fail</returns>
/// 
GLFWwindow* initWindow(const char* title, int width, int height) {
	printf("Initializing...");
	if (!glfwInit()) {
		printf("GLFW failed to init!");
		return nullptr;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (window == NULL) {
		printf("GLFW failed to create window");
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGL(glfwGetProcAddress)) {
		printf("GLAD Failed to load GL headers");
		return nullptr;
	}

	//Initialize ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	return window;
}

