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
int screenWidth = 1600;
int screenHeight = 900;
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


//Global setting
float pointLightRadius = 7.5f;

const int MAX_POINT_LIGHTS = 64;
struct PointLight {
	glm::vec3 position = glm::vec3(0);
	float radius = pointLightRadius;
	glm::vec3 color = glm::vec3(1.0f);
};
PointLight pointLights[MAX_POINT_LIGHTS];
int numPointLights = MAX_POINT_LIGHTS;

bool postProcessEnabled;

ew::Transform monkeyTransform;
ew::Transform planeTransform;

//Assets
ew::Model monkeyModel;
ew::Mesh planeMesh;
ew::Mesh sphereMesh;
GLuint stoneColorTexture;
GLuint stoneNormalTexture;
GLuint goldColorTexture;
GLuint goldNormalTexture;

ew::Framebuffer gBuffer;
ew::Framebuffer framebuffer;
ew::Framebuffer lightVolumeBuffer;

//Shadows
ew::Framebuffer shadowFBO;
ew::Camera shadowCamera;
struct ShadowSettings {
	float camDistance = 10.0f;
	float minBias = 0.005f;
	float maxBias = 0.01f;
}shadowSettings;

void drawScene(ew::Camera& camera, ew::Shader& shader) {
	shader.use();
	shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

	glBindTextureUnit(0, stoneColorTexture);
	glBindTextureUnit(1, stoneNormalTexture);
	shader.setMat4("_Model", planeTransform.modelMatrix());
	planeMesh.draw();

	glBindTextureUnit(0, goldColorTexture);
	glBindTextureUnit(1, goldNormalTexture);
	shader.setMat4("_Model", monkeyTransform.modelMatrix());
	monkeyModel.draw();
}

int main() {
	GLFWwindow* window = initWindow("Assignment 0", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	stoneColorTexture = ew::loadTexture("assets/textures/stones_color.png",true);
	stoneNormalTexture = ew::loadTexture("assets/textures/stones_normal.png");
	goldColorTexture = ew::loadTexture("assets/textures/gold_color.png", true);
	goldNormalTexture = ew::loadTexture("assets/textures/gold_normal.png");

	ew::Shader litShader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader depthOnlyShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader postProcessShader = ew::Shader("assets/fsTriangle.vert", "assets/tonemapping.frag");
	ew::Shader gBufferShader = ew::Shader("assets/gBufferPass.vert", "assets/gBufferPass.frag");
	ew::Shader deferredShader = ew::Shader("assets/fsTriangle.vert", "assets/deferredShading.frag");
	ew::Shader emissiveShader = ew::Shader("assets/emissive.vert", "assets/emissive.frag");
	ew::Shader deferredLightVolume = ew::Shader("assets/deferredLightVolume.vert", "assets/deferredLightVolume.frag");

	//Load models
	monkeyModel = ew::Model("assets/Suzanne.obj");
	planeMesh = ew::Mesh(ew::createPlane(10, 10, 1));
	sphereMesh = ew::Mesh(ew::createSphere(1.0f, 16));
	planeTransform.position.y = -2;

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK); //Back face culling
	glEnable(GL_DEPTH_TEST); //Depth testing

	//Initialize cameras
	mainCamera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	mainCamera.target = glm::vec3(0.0f, 0.0f, 0.0f); //Look at the center of the scene
	mainCamera.aspectRatio = (float)screenWidth / screenHeight;
	mainCamera.fov = 60.0f; //Vertical field of view, in degrees

	shadowCamera.aspectRatio = 1;
	shadowCamera.farPlane = 50;
	shadowCamera.nearPlane = 1.0;
	shadowCamera.orthoHeight = 10.0f;
	shadowCamera.orthographic = true;

	//Initialize framebuffers
	framebuffer = ew::createFramebuffer(screenWidth, screenHeight, GL_RGBA16F);
	lightVolumeBuffer = ew::createFramebuffer(screenWidth, screenHeight, GL_RGB16F);
	shadowFBO = ew::createDepthOnlyFramebuffer(512, 512);
	gBuffer = ew::createGBuffers(screenWidth, screenHeight);

	//Used for supplying indices to vertex shaders
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);

	for (size_t i = 0; i < numPointLights; i++)
	{
		pointLights[i].color = glm::vec3((float)i / numPointLights, 1.0 - (float)i / numPointLights, 1.0);
	}

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		cameraController.move(window, &mainCamera, deltaTime);

		//Spin the monkey
		monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));

		//Spin point lights
		const float orbitRadius = 8.0f;
	    const float thetaStep = glm::two_pi<float>() / numPointLights;
		for (size_t i = 0; i < numPointLights; i++)
		{
			float theta = i * thetaStep + time;
			pointLights[i].position = glm::vec3(cosf(theta)* orbitRadius, 1, sin(theta) * orbitRadius);
		}

		glDisable(GL_BLEND);
		//RENDER SHADOW MAP
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO.fbo);
		glViewport(0, 0, shadowFBO.width, shadowFBO.height);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_DEPTH_BUFFER_BIT);
		shadowCamera.target = glm::vec3(0);
		shadowCamera.position = normalize(-mainLight.direction) * shadowSettings.camDistance;
		glCullFace(GL_FRONT);
		drawScene(shadowCamera,depthOnlyShader);

		//RENDER TO GBUFFER
		glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
		glViewport(0, 0, gBuffer.width, gBuffer.height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_BACK);
		drawScene(mainCamera, gBufferShader);

		//Render light volumes to light buffer
		{
			glBindFramebuffer(GL_FRAMEBUFFER, lightVolumeBuffer.fbo);
			glViewport(0, 0, lightVolumeBuffer.width, lightVolumeBuffer.height);
			glClearColor(0, 0, 0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			//Bind textures
			glBindTextureUnit(0, gBuffer.colorBuffers[0]);
			glBindTextureUnit(1, gBuffer.colorBuffers[1]);

			deferredLightVolume.use();
			deferredLightVolume.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());
			deferredLightVolume.setFloat("_Material.Ka", material.Ka);
			deferredLightVolume.setFloat("_Material.Kd", material.Kd);
			deferredLightVolume.setFloat("_Material.Ks", material.Ks);
			deferredLightVolume.setFloat("_Material.Shininess", material.Shininess);
			deferredLightVolume.setVec3("_EyePos", mainCamera.position);
			deferredLightVolume.setVec2("_ScreenSize", lightVolumeBuffer.width, lightVolumeBuffer.height);

			//Additive blending
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glCullFace(GL_FRONT);
			glDepthMask(GL_FALSE);

			for (size_t i = 0; i < numPointLights; i++)
			{
				deferredLightVolume.setVec3("_PointLight.position", pointLights[i].position);
				deferredLightVolume.setVec3("_PointLight.color", pointLights[i].color);
				deferredLightVolume.setFloat("_PointLight.radius", pointLights[i].radius);

				glm::mat4 m = glm::mat4(1.0f);
				m = glm::translate(m, pointLights[i].position);
				m = glm::scale(m, glm::vec3(pointLights[i].radius));
				deferredLightVolume.setMat4("_Model", m);
				sphereMesh.draw();
			}
		}

		glDisable(GL_BLEND);
		glCullFace(GL_BACK);
		glDepthMask(GL_TRUE);

		//Deferred shading
		//Lighting done in screenspace
		{
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
			glViewport(0, 0, framebuffer.width, framebuffer.height);
			glClearColor(0, 0, 0, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			//Bind textures
			glBindTextureUnit(0, gBuffer.colorBuffers[0]);
			glBindTextureUnit(1, gBuffer.colorBuffers[1]);
			glBindTextureUnit(2, gBuffer.colorBuffers[2]);
			glBindTextureUnit(3, shadowFBO.depthBuffer);
			glBindTextureUnit(4, lightVolumeBuffer.colorBuffers[0]);

			deferredShader.use();
			deferredShader.setFloat("_Material.Ka", material.Ka);
			deferredShader.setFloat("_Material.Kd", material.Kd);
			deferredShader.setFloat("_Material.Ks", material.Ks);
			deferredShader.setFloat("_Material.Shininess", material.Shininess);
			deferredShader.setVec3("_EyePos", mainCamera.position);
			deferredShader.setVec3("_MainLight.color", mainLight.color);
			deferredShader.setVec3("_MainLight.direction", mainLight.direction);
			deferredShader.setFloat("_MinBias", shadowSettings.minBias);
			deferredShader.setFloat("_MaxBias", shadowSettings.maxBias);
			deferredShader.setMat4("_LightTransform", shadowCamera.projectionMatrix() * shadowCamera.viewMatrix());

			for (size_t i = 0; i < numPointLights; i++)
			{
				std::string uniformPrefix = "_PointLights[" + std::to_string(i) + "].";
				deferredShader.setVec3(uniformPrefix + "position", pointLights[i].position);
				deferredShader.setVec3(uniformPrefix + "color", pointLights[i].color);
				deferredShader.setFloat(uniformPrefix + "radius", pointLights[i].radius);
			}
			glBindVertexArray(dummyVAO);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		//Blit gbuffer depth to HDR buffer for forward pass
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer.fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.fbo); // write to default framebuffer
		glBlitFramebuffer(
			0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST
		);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

		//Forward render light sources
		{
			emissiveShader.use();
			emissiveShader.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());
			for (size_t i = 0; i < numPointLights; i++)
			{
				glm::mat4 m = glm::mat4(1.0f);
				m = glm::translate(m, pointLights[i].position);
				m = glm::scale(m, glm::vec3(0.5f));

				emissiveShader.setMat4("_Model", m);
				emissiveShader.setVec3("_Color", pointLights[i].color);
				sphereMesh.draw();
			}
		}
		//Draw to screen w/ post processing
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, screenWidth, screenHeight);
			glClearColor(0, 0, 0, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glBindTextureUnit(0, framebuffer.colorBuffers[0]);
			//glBindTextureUnit(0, shadowFBO.depthBuffer);
			postProcessShader.use();
			glBindVertexArray(dummyVAO);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

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

			if (ImGui::SliderFloat("Point Light Radius", &pointLightRadius, 0.0f, 50.0f)) {
				for (size_t i = 0; i < numPointLights; i++)
				{
					pointLights[i].radius = pointLightRadius;
				}
			}
		}
		if (ImGui::CollapsingHeader("Shadows")) {
			ImGui::DragFloat("Shadow cam distance", &shadowSettings.camDistance);
			ImGui::DragFloat("Shadow cam size", &shadowCamera.orthoHeight);
			ImGui::DragFloat("Shadow min bias", &shadowSettings.minBias);
			ImGui::DragFloat("Shadow max bias", &shadowSettings.maxBias);
		}
	}
	ImGui::End();

	ImGui::Begin("GBuffers"); {
		for (size_t i = 0; i < 3; i++)
		{
			ImGui::Image((ImTextureID)gBuffer.colorBuffers[i], ImVec2(gBuffer.width/4, gBuffer.height/4), ImVec2(0, 1), ImVec2(1, 0));
		}
		ImGui::Image((ImTextureID)shadowFBO.depthBuffer, ImVec2(gBuffer.width / 4, gBuffer.height / 4), ImVec2(0, 1), ImVec2(1, 0));
		ImGui::Image((ImTextureID)lightVolumeBuffer.colorBuffers[0], ImVec2(gBuffer.width / 4, gBuffer.height / 4), ImVec2(0, 1), ImVec2(1, 0));
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

