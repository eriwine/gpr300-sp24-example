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
	glm::vec3 direction = glm::vec3(0, -1.0f, 0);
	glm::vec3 color = glm::vec3(0.5f);
}mainLight;


//Global setting
float pointLightRadius = 5.0f;
float orbitRadius = 8.0f;

const int MAX_POINT_LIGHTS = 1024;
struct PointLight {
	glm::vec3 position = glm::vec3(0);
	float radius = pointLightRadius;
	glm::vec4 color = glm::vec4(1.0f);
};
PointLight pointLights[MAX_POINT_LIGHTS];
int numPointLights = 64;

bool postProcessEnabled;

const int MONKEY_COUNT = 64;
ew::Transform monkeyTransforms[MONKEY_COUNT];
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
//ew::Framebuffer lightVolumeBuffer;

//Shadows
ew::Framebuffer shadowFBO;
ew::Camera shadowCamera;
struct ShadowSettings {
	float camDistance = 10.0f;
	float minBias = 0.005f;
	float maxBias = 0.01f;
}shadowSettings;
const int SHADOW_RESOLUTION = 1024;

//Instanced point light colors
struct InstancedLightData {
	glm::vec4 positionScale; //xyz = position, w = scale
	glm::vec3 color;
}instancedLightData[MAX_POINT_LIGHTS];

enum RenderPath {
	Forward,
	Deferred
};
const char* renderPaths[2] = { "Forward", "Deferred" };
int renderPathIndex = 0;

const float MAX_POINT_LIGHT_RADIUS = 10.0f;
const float PLANE_SIZE = 40.0f;
bool drawLightOrbs = true;

void drawScene(ew::Camera& camera, ew::Shader& shader) {
	shader.use();
	shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

	glBindTextureUnit(0, stoneColorTexture);
	glBindTextureUnit(1, stoneNormalTexture);
	shader.setMat4("_Model", planeTransform.modelMatrix());
	shader.setVec2("_Tiling", glm::vec2(8.0f));
	planeMesh.draw();

	glBindTextureUnit(0, goldColorTexture);
	glBindTextureUnit(1, goldNormalTexture);
	shader.setVec2("_Tiling", glm::vec2(1.0f));
	for (size_t i = 0; i < MONKEY_COUNT; i++)
	{
		shader.setMat4("_Model", monkeyTransforms[i].modelMatrix());
		monkeyModel.draw();
	}
	
}

float randomFloat() {
	return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
}
int main() {
	GLFWwindow* window = initWindow("Assignment 0", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	stoneColorTexture = ew::loadTexture("assets/textures/stones_color.png",true);
	stoneNormalTexture = ew::loadTexture("assets/textures/stones_normal.png");
	goldColorTexture = ew::loadTexture("assets/textures/gold_color.png", true);
	goldNormalTexture = ew::loadTexture("assets/textures/gold_normal.png");

	ew::Shader depthOnlyShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader postProcessShader = ew::Shader("assets/fsTriangle.vert", "assets/tonemapping.frag");
	ew::Shader gBufferShader = ew::Shader("assets/gBufferPass.vert", "assets/gBufferPass.frag");
	ew::Shader deferredShader = ew::Shader("assets/fsTriangle.vert", "assets/deferredShading.frag");
	ew::Shader emissiveShader = ew::Shader("assets/instancedLightOrb.vert", "assets/instancedLightOrb.frag");
	ew::Shader deferredLightVolume = ew::Shader("assets/deferredLightVolume.vert", "assets/deferredLightVolume.frag");
	ew::Shader litShader = ew::Shader("assets/lit.vert", "assets/lit.frag");

	//Load models
	monkeyModel = ew::Model("assets/Suzanne.obj");
	planeMesh = ew::Mesh(ew::createPlane(PLANE_SIZE, PLANE_SIZE, 1));
	sphereMesh = ew::Mesh(ew::createSphere(1.0f, 8));
	planeTransform.position.y = -1.25;

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
	shadowCamera.orthoHeight = 50.0f;
	shadowCamera.orthographic = true;
	shadowCamera.target = glm::vec3(20, 0, 20);

	//Initialize framebuffers
	framebuffer = ew::createFramebuffer(screenWidth, screenHeight, GL_RGBA16F);
	//lightVolumeBuffer = ew::createFramebufferColorOnly(screenWidth, screenHeight, GL_RGB16F);
	shadowFBO = ew::createDepthOnlyFramebuffer(SHADOW_RESOLUTION, SHADOW_RESOLUTION);
	gBuffer = ew::createGBuffers(screenWidth, screenHeight);

	//Used for supplying indices to vertex shaders
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);

	for (size_t i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		pointLights[i].color = glm::vec4(randomFloat(), randomFloat(), randomFloat(), 1.0f);
		instancedLightData[i].color = pointLights[i].color;
	}
	unsigned int lightInstanceVBO;
	//Setup per instance colors for lights
	{
		glBindVertexArray(sphereMesh.getVaoID());
		
		glGenBuffers(1, &lightInstanceVBO);
		glBindBuffer(GL_ARRAY_BUFFER, lightInstanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(instancedLightData), &instancedLightData[0], GL_DYNAMIC_DRAW);

		//PositionScale vec4
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstancedLightData), (void*)0);
		glEnableVertexAttribArray(4);

		//RGB color vec3
		glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(InstancedLightData), (void*)offsetof(InstancedLightData,color));
		glEnableVertexAttribArray(5);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//These attributes are per instance, not vertex
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glBindVertexArray(0);
	}

	//Monkey positions
	{
		int numColumns = sqrt(MONKEY_COUNT);
		for (size_t i = 0; i < MONKEY_COUNT; i++)
		{
			monkeyTransforms[i].position = glm::vec3((i % numColumns) * 6.0, 0, (i / numColumns) * 6.0);
		}
	}

	//Uniform Buffer Object to store light data
	unsigned int lightsUBO;
	glGenBuffers(1, &lightsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(pointLights), pointLights, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, lightsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		cameraController.move(window, &mainCamera, deltaTime);

		//Spin the monkey
		for (size_t i = 0; i < MONKEY_COUNT; i++)
		{
			monkeyTransforms[i].rotation = glm::rotate(monkeyTransforms[i].rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));
		}
		
		//Place point lights
		{
			int numColumns = sqrt(numPointLights);
			float spacing = PLANE_SIZE / numColumns;
			const float thetaStep = glm::two_pi<float>() / numPointLights;
			for (size_t i = 0; i < numPointLights; i++)
			{
				float theta = i * thetaStep + time;
				pointLights[i].position = glm::vec3((i % numColumns) * spacing - spacing/2.0f, 0, (i / numColumns) * spacing - spacing/2.0f);
				instancedLightData[i].positionScale = glm::vec4(pointLights[i].position, pointLights[i].radius);
			}

			//Update UBO
			//glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
			glNamedBufferSubData(lightsUBO, 0, sizeof(pointLights), pointLights);
		}

		//Update instanced light data
		glNamedBufferData(lightInstanceVBO, sizeof(instancedLightData), &instancedLightData[0], GL_DYNAMIC_DRAW);

		glDisable(GL_BLEND);
		//RENDER MAIN LIGHT SHADOW MAP
		{
			glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO.fbo);
			glViewport(0, 0, shadowFBO.width, shadowFBO.height);
			glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
			glClear(GL_DEPTH_BUFFER_BIT);

			shadowCamera.position = shadowCamera.target - (glm::normalize(mainLight.direction) * shadowSettings.camDistance);
			glCullFace(GL_FRONT);
			drawScene(shadowCamera, depthOnlyShader);
			glCullFace(GL_BACK);
		}

		if (renderPathIndex == RenderPath::Forward) {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
			glViewport(0, 0, framebuffer.width, framebuffer.height);
			glClearColor(0, 0, 0, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			litShader.use();
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

			litShader.setInt("_MainTex", 0);
			litShader.setInt("_NormalMap", 1);
			glBindTextureUnit(3, shadowFBO.depthBuffer);
			litShader.setInt("_ShadowMap", 3);

			litShader.setInt("_NumPointLights", numPointLights);

			drawScene(mainCamera, litShader);
			
			//Instanced render light sources
			if (drawLightOrbs)
			{
				emissiveShader.use();
				emissiveShader.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());
				sphereMesh.drawInstanced(ew::DrawMode::TRIANGLES, numPointLights);
			}
		}
		else if (renderPathIndex == RenderPath::Deferred) {
			//RENDER TO GBUFFER
			{
				glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
				glViewport(0, 0, gBuffer.width, gBuffer.height);
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				
				drawScene(mainCamera, gBufferShader);
			}

			//Render light volumes to light buffer
			//{
			//	glBindFramebuffer(GL_FRAMEBUFFER, lightVolumeBuffer.fbo);
			//	glViewport(0, 0, lightVolumeBuffer.width, lightVolumeBuffer.height);
			//	glClearColor(0, 0, 0, 1.0);
			//	glClear(GL_COLOR_BUFFER_BIT);

			//	deferredLightVolume.use();

			//	//Bind textures
			//	glBindTextureUnit(0, gBuffer.colorBuffers[0]);
			//	glBindTextureUnit(1, gBuffer.colorBuffers[1]);

			//	deferredLightVolume.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());
			//	deferredLightVolume.setFloat("_Material.Ka", material.Ka);
			//	deferredLightVolume.setFloat("_Material.Kd", material.Kd);
			//	deferredLightVolume.setFloat("_Material.Ks", material.Ks);
			//	deferredLightVolume.setFloat("_Material.Shininess", material.Shininess);
			//	deferredLightVolume.setVec3("_EyePos", mainCamera.position);
			//	deferredLightVolume.setVec2("_ScreenSize", lightVolumeBuffer.width, lightVolumeBuffer.height);

			//	//Additive blending
			//	glEnable(GL_BLEND);
			//	glBlendFunc(GL_ONE, GL_ONE);
			//	glCullFace(GL_FRONT);
			//	glDepthMask(GL_FALSE);

			//	sphereMesh.drawInstanced(ew::DrawMode::TRIANGLES, numPointLights);

			//	glDisable(GL_BLEND);
			//	glCullFace(GL_BACK);
			//	glDepthMask(GL_TRUE);
			//}


			//Deferred shading 
			//Lighting done in screenspace on fullscreen triangle
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
				//glBindTextureUnit(4, lightVolumeBuffer.colorBuffers[0]);

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

				glBindVertexArray(dummyVAO);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

			//Render light volumes to light buffer
			{
				//glBindFramebuffer(GL_FRAMEBUFFER, lightVolumeBuffer.fbo);
				//glViewport(0, 0, lightVolumeBuffer.width, lightVolumeBuffer.height);
				//glClearColor(0, 0, 0, 1.0);
				//glClear(GL_COLOR_BUFFER_BIT);

				deferredLightVolume.use();

				//Bind textures
				glBindTextureUnit(0, gBuffer.colorBuffers[0]);
				glBindTextureUnit(1, gBuffer.colorBuffers[1]);
				glBindTextureUnit(2, gBuffer.colorBuffers[2]);

				deferredLightVolume.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());
				deferredLightVolume.setFloat("_Material.Ka", material.Ka);
				deferredLightVolume.setFloat("_Material.Kd", material.Kd);
				deferredLightVolume.setFloat("_Material.Ks", material.Ks);
				deferredLightVolume.setFloat("_Material.Shininess", material.Shininess);
				deferredLightVolume.setVec3("_EyePos", mainCamera.position);
				deferredLightVolume.setVec2("_ScreenSize", framebuffer.width, framebuffer.height);

				//Additive blending
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glCullFace(GL_FRONT);
				glDepthMask(GL_FALSE);

				sphereMesh.drawInstanced(ew::DrawMode::TRIANGLES, numPointLights);

				glDisable(GL_BLEND);
				glCullFace(GL_BACK);
				glDepthMask(GL_TRUE);
			}

			////Blit gbuffer depth to HDR buffer for forward pass
			glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer.fbo); //read from G-buffer
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.fbo); // write to HDR buffer
			glBlitFramebuffer(
				0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST
			);
	
			//Forward render light sources
			if (drawLightOrbs)
			{
				emissiveShader.use();
				emissiveShader.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());
				sphereMesh.drawInstanced(ew::DrawMode::TRIANGLES, numPointLights);
			}
		}
		

		//Draw to screen w/ post processing
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, screenWidth, screenHeight);
			glClearColor(0, 0, 0, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glBindTextureUnit(0, framebuffer.colorBuffers[0]);
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

			if (ImGui::SliderFloat("Point Light Radius", &pointLightRadius, 0.0f, MAX_POINT_LIGHT_RADIUS)) {
				for (size_t i = 0; i < numPointLights; i++)
				{
					pointLights[i].radius = pointLightRadius;
				}
			}
			ImGui::SliderFloat("Point Light Orbit Radius", &orbitRadius, 0.0f, 50.0f);
			ImGui::SliderInt("Num lights", &numPointLights, 0, MAX_POINT_LIGHTS);
		}
		if (ImGui::CollapsingHeader("Shadows")) {
			ImGui::DragFloat("Shadow cam distance", &shadowSettings.camDistance);
			ImGui::DragFloat("Shadow cam size", &shadowCamera.orthoHeight);
			ImGui::DragFloat("Shadow min bias", &shadowSettings.minBias);
			ImGui::DragFloat("Shadow max bias", &shadowSettings.maxBias);
		}
		ImGui::Combo("Render Path", &renderPathIndex, renderPaths, IM_ARRAYSIZE(renderPaths));
		ImGui::Checkbox("Draw Light Orbs", &drawLightOrbs);
	}
	ImGui::End();

	ImGui::Begin("GBuffers"); {
		for (size_t i = 0; i < 3; i++)
		{
			ImGui::Image((ImTextureID)gBuffer.colorBuffers[i], ImVec2(gBuffer.width/4, gBuffer.height/4), ImVec2(0, 1), ImVec2(1, 0));
		}
		ImGui::Image((ImTextureID)shadowFBO.depthBuffer, ImVec2(gBuffer.width / 4, gBuffer.height / 4), ImVec2(0, 1), ImVec2(1, 0));
		//ImGui::Image((ImTextureID)lightVolumeBuffer.colorBuffers[0], ImVec2(gBuffer.width / 4, gBuffer.height / 4), ImVec2(0, 1), ImVec2(1, 0));
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

