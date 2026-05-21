#include <glad/glad.h>    // MUST come first -- defines the GL function types
#include <GLFW/glfw3.h>   // windowing, comes after GLAD
#include <iostream>	  // for printing errors

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
	if (window == nullptr)
	{
		std::cerr << "Failed to create GLFW window\n";
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// --- load OpenGL function pointers via GLAD (must be after context is current) ---
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cerr << "Failed to initialize GLAD\n";
		glfwTerminate();
		return -1;
	}

	std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

	// --- render loop ---
	while (!glfwWindowShouldClose(window))
	{
		process_input(window);

		glClearColor(0.1f, 0.12f, 0.15f, 1.0f); // dark slate
		glClear(GL_COLOR_BUFFER_BIT);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// --- cleanup ---
	glfwTerminate();
	return 0;
}
