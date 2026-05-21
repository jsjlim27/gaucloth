#include <glad/glad.h>    // MUST come first -- defines the GL function types
#include <GLFW/glfw3.h>   // windowing, comes after GLAD

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt
#include <glm/gtc/type_ptr.hpp>         // value_ptr (matrix -> float*)

#include "cloth.h" // our physics state (no OpenGL inside it)

#include <iostream>  // for printing errors
#include <vector>
#include <cmath>

// ----------------------------------------------------------------------------
// Shader sources (embedded as strings for now; will move to files later)
// ----------------------------------------------------------------------------
const char *VERTEX_SRC = R"(
#version 430 core
layout (location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
	gl_Position = uProjection * uView * uModel *vec4(aPos, 1.0);
	gl_PointSize = 4.0; // make points visible as small squares
}
)";

const char *FRAGMENT_SRC = R"(
#version 430 core
out vec4 FragColor;
void main() {
	FragColor = vec4(0.85, 0.87, 0.90, 1.0); // light gray points
}
)";

// ----------------------------------------------------------------------------
// Orbit camera state (spherical coords around a target)
// ----------------------------------------------------------------------------
struct Camera {
	float yaw = 0.7f;    // horizontal angle (radians)
	float pitch = 0.5f;  // vertical angle (radians)
	float radius = 4.0f; // distance from target
	glm::vec3 target = glm::vec3(0.0f);

	glm::vec3 eye() const 
	{
		// spherical -> cartesian (r, two angles -> xyz)
		float x = radius * std::cos(pitch) * std::sin(yaw);
		float y = radius * std::sin(pitch);
		float z = radius * std::cos(pitch) * std::cos(yaw);
		return target + glm::vec3(x, y, z);
	}

	glm::mat4 view() const 
	{
		return glm::lookAt(eye(), target, glm::vec3(0.0f, 1.0f, 0.0f));
	}
};

Camera g_camera;

// mouse drag state
bool g_dragging = false;
double g_lastX = 0.0;
double g_lastY = 0.0;

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			g_dragging = true;
			glfwGetCursorPos(window, &g_lastX, &g_lastY);
		}
		else if (action == GLFW_RELEASE)
		{
			g_dragging = false;
		}
	}
}

void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
	if (!g_dragging)
	{
		return;
	}

	float dx = static_cast<float>(xpos - g_lastX);
	float dy = static_cast<float>(ypos - g_lastY);
	g_lastX = xpos;
	g_lastY = ypos;

	const float sensitivity = 0.005f;
	g_camera.yaw -= dx * sensitivity;
	g_camera.pitch += dy * sensitivity;

	// clamp pitch so we don't flip over the poles
	const float limit = 1.55f; // just under pi/2
	if (g_camera.pitch > limit)
	{
		g_camera.pitch = limit;
	}

	if (g_camera.pitch < -limit)
	{
		g_camera.pitch = -limit;
	}

}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
	g_camera.radius -= static_cast<float>(yoffset) * 0.3f;
	if (g_camera.radius < 0.5f)
	{
		g_camera.radius = 0.5f;
	}

	if (g_camera.radius > 20.0f)
	{
		g_camera.radius = 20.0f;
	}
}

// Keep the GL viewport matched to the window size on resize.
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void process_input(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// ----------------------------------------------------------------------------
// Shader compilation helpers
// ----------------------------------------------------------------------------
GLuint compile_shader(GLenum type, const char *src)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);
	GLint ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if(!ok)
	{
		char log[1024];
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		std::cerr << "Shader compile error:\n" << log << "\n";
	}
	return shader;
}

GLuint make_program(const char *vsrc, const char *fsrc)
{
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[1024];
		glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
		std::cerr << "Program link error:\n" << log << "\n";
	}
	glDeleteShader(vs);    // linked into program now; can delete the parts
	glDeleteShader(fs);
	return prog;
}

int main()
{
	// --- initialize GLFW and request and OpenGL 4.3 Core context ---
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW\n";
		return -1;
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// --- create the window ---
	GLFWwindow *window = glfwCreateWindow(1280, 720, "gaucloth", nullptr, nullptr);
	if (!window)
	{
		std::cerr << "Failed to create GLFW window\n";
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// --- load OpenGL function pointers via GLAD (must be after context is current) ---
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cerr << "Failed to initialize GLAD\n";
		glfwTerminate();
		return -1;
	}

	std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

	glEnable(GL_DEPTH_TEST);         // nearer points occlude farther ones
	glEnable(GL_PROGRAM_POINT_SIZE); // let the vertex shader set point size

	// --- physics: build the cloth, pin the top corners ---
	const int GRID_N = 20;
	const float GRID_SPACE = 0.1f;

	Cloth cloth;
	cloth.init_grid(GRID_N, GRID_SPACE);
	
	// top row is i = N-1 in our grid; pin its two corners
	cloth.pin(cloth.index(GRID_N - 1, 0));
	cloth.pin(cloth.index(GRID_N - 1, GRID_N - 1));

	std::cout << "constraints: " << cloth. constraints.size() << "\n";
	int c0 = cloth.index(GRID_N - 1, 0);
	int c1 = cloth.index(GRID_N - 1, GRID_N - 1);
	std::cout << "corner indices: " << c0 << ", " << c1 << "\n";
	std::cout << "inv_mass at corners: " << cloth.inv_mass[c0]
		  << ", " << cloth.inv_mass[c1] << "\n";

	const int point_count = GRID_N * GRID_N;

	// --- rendering buffers (positions now change every frame -> DYNAMIC) ---
	GLuint vao, vbo;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);             // start recording layout into VAO
	glBindBuffer(GL_ARRAY_BUFFER, vbo); // the VBO we're describing

	// allocate space sized to the position array; fill it each frame below
	glBufferData(GL_ARRAY_BUFFER,
		     cloth.positions.size() * sizeof(glm::vec3),
		     cloth.positions.data(),
		     GL_DYNAMIC_DRAW);

	// layout: location 0, 3 floats per vertex, tightly packed
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0); // done recording

	// --- one-time setup: shaders ---
	GLuint program = make_program(VERTEX_SRC, FRAGMENT_SRC);
	GLint locModel = glGetUniformLocation(program, "uModel");
	GLint locView = glGetUniformLocation(program, "uView");
	GLint locProj = glGetUniformLocation(program, "uProjection");
	glm::mat4 model = glm::mat4(1.0f); // grid sits at origin, no transform

	const float dt = 1.0f / 60.0f; // fixed timestep
	
	// --- render loop ---
	while (!glfwWindowShouldClose(window))
	{
		process_input(window);

		// --- PHYSICS: advance the simulation ---
		cloth.step(dt);

		// --- HANDOFF: copy updated positions into the VBO ---
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0,
				cloth.positions.size() * sizeof(glm::vec3),
				cloth.positions.data());

		// --- RENDER ---
		int fbw, fbh;
		glfwGetFramebufferSize(window, &fbw, &fbh);
		float aspect = (fbh == 0) ? 1.0f : (float)fbw / (float)fbh;

		glm::mat4 view = g_camera.view();
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

		glClearColor(0.1f, 0.12f, 0.15f, 1.0f); // dark slate
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(program);
		glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(proj));

		glBindVertexArray(vao);
		glDrawArrays(GL_POINTS, 0, point_count);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// --- cleanup ---
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteProgram(program);
	glfwTerminate();
	return 0;
}
