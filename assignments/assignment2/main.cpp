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

ew::Model monkeyModel;
ew::Mesh planeMesh;
ew::Transform monkeyTransform;
ew::Transform planeTransform;
GLuint stoneColorTexture;
GLuint stoneNormalTexture;

void drawScene(ew::Camera& camera, ew::Shader& shader) {
	shader.use();
	
	shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
	shader.setMat4("_Model", monkeyTransform.modelMatrix());
	monkeyModel.draw();

	shader.setMat4("_Model", planeTransform.modelMatrix());
	planeMesh.draw();
}

int main() {
	GLFWwindow* window = initWindow("Assignment 0", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	stoneColorTexture = ew::loadTexture("assets/stones_color.png",true);
	stoneNormalTexture = ew::loadTexture("assets/stones_normal.png");

	ew::Shader litShader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader depthOnlyShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader postProcessShader = ew::Shader("assets/fsTriangle.vert", "assets/tonemapping.frag");

	//Load models
	monkeyModel = ew::Model("assets/Suzanne.obj");
	planeMesh = ew::Mesh(ew::createPlane(10, 10, 1));

	planeTransform.position.y = -2;

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK); //Back face culling
	glEnable(GL_DEPTH_TEST); //Depth testing


	mainCamera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	mainCamera.target = glm::vec3(0.0f, 0.0f, 0.0f); //Look at the center of the scene
	mainCamera.aspectRatio = (float)screenWidth / screenHeight;
	mainCamera.fov = 60.0f; //Vertical field of view, in degrees

	ew::Framebuffer framebuffer = ew::createFramebuffer(screenWidth, screenHeight);
	ew::Framebuffer shadowFBO = ew::createDepthOnlyFramebuffer(512, 512);

	ew::Camera shadowCamera;
	shadowCamera.aspectRatio = 1;
	shadowCamera.farPlane = 50;
	shadowCamera.nearPlane = 1.0;
	shadowCamera.orthoHeight = 10.0;
	shadowCamera.orthographic = true;

	//Used for supplying indices to vertex shaders
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		cameraController.move(window, &mainCamera, deltaTime);

		//Spin the monkey
		monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));

		//RENDER SHADOW MAP
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO.fbo);
		glViewport(0, 0, shadowFBO.width, shadowFBO.height);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_DEPTH_BUFFER_BIT);
		shadowCamera.target = glm::vec3(0);
		shadowCamera.position = normalize(-mainLight.direction) * 3.0f;
		glCullFace(GL_FRONT);
		drawScene(shadowCamera,depthOnlyShader);

		//RENDER SCENE TO HDR BUFFER
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
		glViewport(0, 0, framebuffer.width, framebuffer.height);
		glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_BACK);
		//Bind textures
		glBindTextureUnit(0, stoneColorTexture);
		glBindTextureUnit(1, stoneNormalTexture);
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

		litShader.setMat4("_LightTransform", shadowCamera.projectionMatrix() * shadowCamera.viewMatrix());

		drawScene(mainCamera,litShader);

		//Draw to screen
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screenWidth, screenHeight);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindTextureUnit(0, framebuffer.colorBuffer);
		//glBindTextureUnit(0, shadowFBO.depthBuffer);
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

	ImGui::Begin("Settings");
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

