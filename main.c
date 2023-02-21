#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include "cglm/cglm.h"
#include "tracking.h"

static bool window_fullscreen = false;
static int window_width = 800;
static int window_height = 600;
static int saved_width, saved_height;
static int saved_x, saved_y;

static const float cube_vertices[] = {
	-0.5f, -0.5f, -0.5f,
	0.5f, -0.5f, -0.5f,
	0.5f,  0.5f, -0.5f,
	0.5f,  0.5f, -0.5f,
	-0.5f,  0.5f, -0.5f,
	-0.5f, -0.5f, -0.5f,

	-0.5f, -0.5f,  0.5f,
	0.5f, -0.5f,  0.5f,
	0.5f,  0.5f,  0.5f,
	0.5f,  0.5f,  0.5f,
	-0.5f,  0.5f,  0.5f,
	-0.5f, -0.5f,  0.5f,

	-0.5f,  0.5f,  0.5f,
	-0.5f,  0.5f, -0.5f,
	-0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f,  0.5f,
	-0.5f,  0.5f,  0.5f,

	0.5f,  0.5f,  0.5f,
	0.5f,  0.5f, -0.5f,
	0.5f, -0.5f, -0.5f,
	0.5f, -0.5f, -0.5f,
	0.5f, -0.5f,  0.5f,
	0.5f,  0.5f,  0.5f,

	-0.5f, -0.5f, -0.5f,
	0.5f, -0.5f, -0.5f,
	0.5f, -0.5f,  0.5f,
	0.5f, -0.5f,  0.5f,
	-0.5f, -0.5f,  0.5f,
	-0.5f, -0.5f, -0.5f,

	-0.5f,  0.5f, -0.5f,
	0.5f,  0.5f, -0.5f,
	0.5f,  0.5f,  0.5f,
	0.5f,  0.5f,  0.5f,
	-0.5f,  0.5f,  0.5f,
	-0.5f,  0.5f, -0.5f,
};

static const char* vertex_shader_source = "#version 330 core\n"
	"layout (location = 0) in vec3 position;\n"
	"uniform mat4 model;\n"
	"uniform mat4 view;\n"
	"uniform mat4 projection;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = projection * view * model * vec4(position, 1.0f);\n"
	"}\0";

static const char* fragment_shader_source = "#version 330 core\n"
	"out vec4 FragColor;\n"
	"uniform vec3 color;\n"
	"void main()\n"
	"{\n"
	"    FragColor = vec4(color, 1.0f);\n"
	"}\n\0";

static void
compile_shader(unsigned int shader, const char* src)
{
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	int success;
	char info[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shader, 512, NULL, info);
		printf("Could not compile shader:\n%s\n", info);
		exit(-1);
	}
}

static unsigned int
build_shader_program()
{
	unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	compile_shader(vertex_shader, vertex_shader_source);

	unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	compile_shader(fragment_shader, fragment_shader_source);

	unsigned int shader_program = glCreateProgram();
	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);

	int success;
	char info[512];
	glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shader_program, 512, NULL, info);
		printf("Could not link shader program:\n%s\n", info);
		exit(-1);
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return shader_program;
}

static void
framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	window_width = width;
	window_height = height;
}

static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS) {
		switch (key) {
		case GLFW_KEY_ESCAPE:
		case GLFW_KEY_Q:
			glfwSetWindowShouldClose(window, true);
			break;
		case GLFW_KEY_SPACE:
			tracking_set(GLM_QUAT_IDENTITY);
			break;
		case GLFW_KEY_TAB:
		case GLFW_KEY_F11: {
			window_fullscreen = !window_fullscreen;
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			if (!window_fullscreen) {
				glfwSetWindowMonitor(window, NULL, saved_x, saved_y, saved_width, saved_height, 0);
				framebuffer_size_callback(window, saved_width, saved_height);
			} else {
    		glfwGetWindowSize(window, &saved_width, &saved_height);
				glfwGetWindowPos(window, &saved_x, &saved_y);

				const GLFWvidmode* mode = glfwGetVideoMode(monitor);
				glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
				framebuffer_size_callback(window, mode->width, mode->height);
			}
		} break;
		default:
			break;
		}
	}
}

static float
randf()
{
	return (float)rand() / (float)RAND_MAX;
}

int
main(void)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(window_width, window_height, "demo", NULL, NULL);
	if (window == NULL) {
		printf("Failed to create GLFW window\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	glfwSetKeyCallback(window, key_callback);

	glewInit();

	unsigned int shader_program = build_shader_program();

	unsigned int vbo, vao;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	versor rotation;
	tracking_start();

	while (!glfwWindowShouldClose(window)) {
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(shader_program);

		mat4 projection;
		glm_perspective(glm_rad(46.0f), (float)window_width / (float)window_height, 0.1f, 100.0f, projection);
		glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, (float*)projection);

		tracking_get(rotation);
		mat4 view = GLM_MAT4_IDENTITY_INIT;
		glm_quat_look(GLM_VEC3_ZERO, rotation, view);
		glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, (float*)view);

		glBindVertexArray(vao);

		const int revs = 4;
		const float angle_step = 20.0f;
		const float elevation_step = (2.0f / (float)revs) / (360.0f / angle_step);

		srand(64);
		float elevation = 1.0f;
		for (float angle = 0.0f; angle < 360.0f * revs; angle += angle_step, elevation -= elevation_step) {
			mat4 model = GLM_MAT4_IDENTITY_INIT;

			glm_rotate(model, glm_rad(angle), GLM_YUP);
			glm_translate(model, (vec3){0.0f, elevation, -1.0f});

			glm_scale(model, (vec3){0.15f, 0.15f, 0.15f});
			glm_rotate(model, glm_rad(randf() * 360.0f), (vec3){randf(), randf(), randf()});

			glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, (float*)model);
			glUniform3fv(glGetUniformLocation(shader_program, "color"), 1, (float*)(vec3){randf(), randf(), randf()});

			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();    
	}

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteProgram(shader_program);

	glfwTerminate();
	return 0;
}
