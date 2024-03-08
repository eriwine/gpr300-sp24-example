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

namespace ew {
	struct LineRenderer {
		//Public
		float thickness = 16.0f; //Thickness in pixels 
		glm::vec4 color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		bool wireFrame = true;
		//Private
		unsigned int m_vao; //Dummy VAO used for supplying vertex IDs
		unsigned int m_ssbo; //Used to supply positions to GPU
		ew::Shader* m_shader;
		std::vector<glm::vec4> m_positions;
		bool m_dirty = true; //Set to true when positions change
	};
	LineRenderer gizmoInit(ew::Shader* shader) {
		LineRenderer renderer;
		glCreateVertexArrays(1, &renderer.m_vao);
		glBindVertexArray(renderer.m_vao);
	
		renderer.m_ssbo;
		glGenBuffers(1, &renderer.m_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer.m_ssbo);

		renderer.m_shader = shader;

		glBindVertexArray(0);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		return renderer;
	}
	void gizmoBegin(LineRenderer* renderer, const ew::Camera* camera, glm::vec2 screenSize) {
		renderer->m_shader->use();
		renderer->m_shader->setMat4("_ViewProjection", camera->projectionMatrix() * camera->viewMatrix());
		renderer->m_shader->setVec2("_ScreenResolution", screenSize);
		renderer->m_shader->setFloat("_Thickness", renderer->thickness);
		renderer->m_shader->setVec4("_Color", renderer->color);
	}

	void gizmoSetPositions(LineRenderer* renderer, const std::vector<glm::vec3>& positions) {
		size_t numPositions = positions.size();
		if (numPositions < 2) {
			printf("Not enough positions for line render");
			return;
		}
		renderer->m_positions.clear();
		//Tangent of first position
		renderer->m_positions.emplace_back(glm::vec4(positions[1], 1.0f));
		for (size_t i = 0; i < numPositions; i++)
		{
			renderer->m_positions.emplace_back(glm::vec4(positions[i],1.0f));
		}
		//Tangent of last position
		renderer->m_positions.emplace_back(glm::vec4(positions[0], 1.0f));
		renderer->m_dirty = true;
	}
	void gizmoSetPositionsManual(LineRenderer* renderer, const std::vector<glm::vec4>& positions) {
		if (positions.size() < 2) {
			printf("Not enough positions for line render");
			return;
		}
		renderer->m_positions = positions;
		renderer->m_dirty = true;
	}
	void gizmoEnd(LineRenderer* renderer) {
		glBindVertexArray(renderer->m_vao);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderer->m_ssbo);
		glPolygonMode(GL_FRONT_AND_BACK, renderer->wireFrame ? GL_LINE : GL_FILL);
		GLsizei numPositions = renderer->m_positions.size();
		if (renderer->m_dirty) {
			glBufferData(GL_SHADER_STORAGE_BUFFER, numPositions * sizeof(*renderer->m_positions.data()), renderer->m_positions.data(), GL_DYNAMIC_DRAW);
			renderer->m_dirty = false;
		}
		GLsizei N1 = numPositions - 2;
		glDrawArrays(GL_TRIANGLES, 0, 6 * (N1 - 1));


		//Clean up
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	}
}
ew::LineRenderer lineRenderer;

namespace ew {
	class LineMesh {
	public:
		void Init() {
			if (m_initialized) {
				return;
			}
			m_initialized = true;
			glGenVertexArrays(1, &m_vao);
			glBindVertexArray(m_vao);
			glGenBuffers(1, &m_vbo);
			glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
			glEnableVertexAttribArray(0);

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		void SetPositions(const std::vector <glm::vec3>& positions) {
			glNamedBufferData(m_vbo, sizeof(glm::vec3) * positions.size(), positions.data(), GL_DYNAMIC_DRAW);
			m_numPositions = positions.size();
		}
		void Draw(const Camera& camera, const ew::Shader& shader) {
			shader.use();
			shader.setMat4("_MVP", camera.projectionMatrix() * camera.viewMatrix());
			shader.setVec4("_Color", m_color);
			glLineWidth(m_width);
			glBindVertexArray(m_vao);
			glDrawArrays(GL_LINE_STRIP, 0, m_numPositions);
		}
		void SetColor(const glm::vec4& color) {
			m_color = color;
		}
		void SetWidth(float width) {
			m_width = width;
		}
	private:
		bool m_initialized;
		unsigned int m_vao;
		unsigned int m_vbo;
		unsigned int m_numPositions;
		float m_width = 2;
		glm::vec4 m_color = glm::vec4(0, 0, 0, 1);
	};
}

ew::LineMesh lineMesh;

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

	ew::Shader lineRenderShader = ew::Shader("assets/line.vert", "assets/line.frag");
	lineRenderer = ew::gizmoInit(&lineRenderShader);

	ew::Shader gizmoShader = ew::Shader("assets/gizmo.vert", "assets/gizmo.frag");
	lineMesh.Init();
	const std::vector<glm::vec3> linePositions = { glm::vec3(0), glm::vec3(1), glm::vec3(-1,2,0) };
	lineMesh.SetPositions(linePositions);

	//Anti-aliasing
	glfwWindowHint(GLFW_SAMPLES, 4);
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_LINE_SMOOTH);

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

		//drawScene(mainCamera,litShader);


		//Draw line renderer
		//ew::gizmoBegin(&gizmoRenderer, &mainCamera, glm::vec2(screenWidth,screenHeight));
		//std::vector<glm::vec3> positions;
		//positions.push_back(glm::vec3(-1.0f, 0.0f, 0.0f));
		//positions.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
		////positions.push_back(glm::vec3(1.0f, 1.0f, 0.0f));
		////positions.push_back(glm::vec3(-1.0f, 1.0f, 0.0f));
		//ew::gizmoSetPositions(&gizmoRenderer, positions);

		//glm::vec4 p0(-1.0f, -1.0f, 0.0f, 1.0f);
		//glm::vec4 p1(1.0f, -1.0f, 0.0f, 1.0f);
		//glm::vec4 p2(1.0f, 1.0f, 0.0f, 1.0f);
		//glm::vec4 p3(-1.0f, 1.0f, 0.0f, 1.0f);
		//std::vector<glm::vec4> varray1{ p3, p0, p1, p2, p3, p0, p1 };
	
		//ew::gizmoSetPositionsManual(&gizmoRenderer, varray1);
		//ew::gizmoEnd(&gizmoRenderer);

		lineMesh.Draw(mainCamera, gizmoShader);

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

	/*ImGui::Begin("Line renderer");
	ImGui::Checkbox("Wireframe", &gizmoRenderer.wireFrame);
	ImGui::DragFloat("Thickness", &gizmoRenderer.thickness, 1.0f);
	ImGui::ColorEdit4("Color", &gizmoRenderer.color.r);
	ImGui::End();*/

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

