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
int screenWidth = 1680;
int screenHeight = 1050;
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

unsigned const int MONKEY_COUNT = 4;
ew::Transform monkeyTransforms[MONKEY_COUNT];
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
	int pcfSize = 1;
	int shadowMapResolution = 1024;
	bool drawFrustum = false;
}shadowSettings;

void drawScene(ew::Camera& camera, ew::Shader& shader) {
	shader.use();
	shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

	glBindTextureUnit(0, goldColorTexture);
	glBindTextureUnit(1, goldNormalTexture);

	for (int i = 0; i < MONKEY_COUNT; i++) {
		shader.setMat4("_Model", monkeyTransforms[i].modelMatrix());
		monkeyModel.draw();
	}

	//glBindTextureUnit(0, goldColorTexture);
	//glBindTextureUnit(1, goldNormalTexture);
	//shader.setMat4("_Model", monkeyTransform.modelMatrix());
	//monkeyModel.draw();

	glBindTextureUnit(0, stoneColorTexture);
	glBindTextureUnit(1, stoneNormalTexture);
	shader.setMat4("_Model", planeTransform.modelMatrix());
	planeMesh.draw();
}

/// <summary>
/// A cube mesh designed to be drawn with GL_LINES
/// </summary>
/// <returns></returns>
unsigned int createCubeLinesVAO(){
	glm::vec3 v[8] = {
		glm::vec3(-1,-1,1),
		glm::vec3(1,-1,1),
		glm::vec3(1,1,1),
		glm::vec3(-1,1,1),
		glm::vec3(-1,-1,-1),
		glm::vec3(1,-1,-1),
		glm::vec3(1,1,-1),
		glm::vec3(-1,1,-1)
	};
	GLushort i[24] = {
		0,1,1,2,2,3,3,0,
		4,5,5,6,6,7,7,4,
		4,0,1,5,6,2,3,7
	}; 
	unsigned int m_vao = 0;
	unsigned int m_vbo = 0;
	unsigned int m_ebo = 0;
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	glGenBuffers(1, &m_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(v), &v[0], GL_STATIC_DRAW);
	glGenBuffers(1, &m_ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(i),&i[0], GL_STATIC_DRAW);

	//Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (const void*)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return m_vao;
}

void resizeShadowFBO(ew::Framebuffer* framebuffer, int width, int height) {

	framebuffer->width = width;
	framebuffer->height = height;
	glBindTexture(GL_TEXTURE_2D, framebuffer->depthBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	//glTextureStorage2D(framebuffer->depthBuffer, 1, GL_DEPTH_COMPONENT24, width, height);
	glBindTexture(GL_TEXTURE_2D, 0);
}

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
	ew::Shader debugShader = ew::Shader("assets/debugFrustum.vert", "assets/debugFrustum.frag");

	//Load models
	monkeyModel = ew::Model("assets/Suzanne.obj");
	planeMesh = ew::Mesh(ew::createPlane(10, 10, 1));

	for (size_t i = 0; i < MONKEY_COUNT; i++)
	{
		monkeyTransforms[i].position = glm::vec3(-1.5f + (i % 2) * 3.0f, 0.0, -1.5 + (i / 2) * 3.0f);
	}
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
	shadowCamera.farPlane = 50.0;
	shadowCamera.nearPlane = 1.0;
	shadowCamera.orthoHeight = 10.0f;
	shadowCamera.orthographic = true;
	//Initialize framebuffers
	framebuffer = ew::createFramebuffer(screenWidth, screenHeight, GL_RGBA16F);
	shadowFBO = ew::createDepthOnlyFramebuffer(shadowSettings.shadowMapResolution, shadowSettings.shadowMapResolution);

	//Used for supplying indices to vertex shaders
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);

	unsigned int wireCubeVAO = createCubeLinesVAO();
	glLineWidth(3);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		cameraController.move(window, &mainCamera, deltaTime);

		//Spin the monkey
		for (int i = 0; i < 4; i++) {
			monkeyTransforms[i].rotation = glm::rotate(monkeyTransforms[i].rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));
		}

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
		litShader.setInt("_PCFSize", shadowSettings.pcfSize);
		litShader.setMat4("_LightTransform", shadowCamera.projectionMatrix() * shadowCamera.viewMatrix());

		drawScene(mainCamera,litShader);

		if (shadowSettings.drawFrustum) {
			debugShader.use();
			debugShader.setMat4("_FrustumInvProj", glm::inverse(shadowCamera.projectionMatrix() * shadowCamera.viewMatrix()));
			debugShader.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());
			glBindVertexArray(wireCubeVAO);
			glDrawElements(GL_LINES, 24, GL_UNSIGNED_SHORT, 0);
		}

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
			ImGui::DragFloat("Min bias", &shadowSettings.minBias, 0.005f);
			ImGui::DragFloat("Max bias", &shadowSettings.maxBias, 0.005f);
			ImGui::SliderInt("PCF Size", &shadowSettings.pcfSize, 0, 5);
			if (ImGui::SliderInt("Shadow Map Resolution", &shadowSettings.shadowMapResolution, 8, 2048)) {
				resizeShadowFBO(&shadowFBO, shadowSettings.shadowMapResolution, shadowSettings.shadowMapResolution);
			}
			ImGui::Indent(16.0f);
			if (ImGui::CollapsingHeader("Camera")) {
				ImGui::Checkbox("Debug draw frustum", &shadowSettings.drawFrustum);
				ImGui::Checkbox("Orthographic", &shadowCamera.orthographic);
				if (shadowCamera.orthographic) {
					ImGui::DragFloat("Size", &shadowCamera.orthoHeight);
				}
				else {
					ImGui::DragFloat("FOV", &shadowCamera.fov, 1.0f);
				}
				ImGui::DragFloat("Distance to target", &shadowSettings.camDistance);
				ImGui::DragFloat("Far plane", &shadowCamera.farPlane);
			}
			
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

