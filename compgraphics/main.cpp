//
//  main.cpp
//  compgraphics
//

#include <iostream>
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include "Angel.h"

typedef vec2 point2;

GLuint vao;
GLuint buffer;
GLuint program;
GLuint modelLoc;
GLuint colorLoc;

float planet1Orbit = 0.0f;
float planet2Orbit = 0.0f;
float moonOrbit    = 0.0f;

void init()
{
    point2 points[3] = {
        point2(-0.15, -0.15),
        point2( 0.0,   0.15),
        point2( 0.15, -0.15)
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);

    program = Angel::InitShader("shaders/vshader.glsl", "shaders/fshader.glsl");
    glUseProgram(program);

    GLuint loc = glGetAttribLocation(program, "vPosition");
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

    modelLoc = glGetUniformLocation(program, "model");
    colorLoc = glGetUniformLocation(program, "uColor");

    glClearColor(0.04, 0.04, 0.10, 1.0);
}

void drawObject(const mat4& model, const vec4& color)
{
    glUniformMatrix4fv(modelLoc, 1, GL_TRUE, model);
    glUniform4fv(colorLoc, 1, color);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Sun
    mat4 sunModel = Scale(1.8, 1.8, 1.0);
    drawObject(sunModel, vec4(1.0, 0.6, 0.1, 1.0));

    // Planet 1
    mat4 planet1Model =
        RotateZ(planet1Orbit) *
        Translate(0.45, 0.0, 0.0) *
        Scale(0.6, 0.6, 1.0);

    drawObject(planet1Model, vec4(0.2, 0.6, 1.0, 1.0));

    // Moon of Planet 1
    mat4 moonModel =
        RotateZ(planet1Orbit) *
        Translate(0.45, 0.0, 0.0) *
        RotateZ(moonOrbit) *
        Translate(0.18, 0.0, 0.0) *
        Scale(0.25, 0.25, 1.0);

    drawObject(moonModel, vec4(0.8, 0.8, 0.8, 1.0));

    // Planet 2
    mat4 planet2Model =
        RotateZ(planet2Orbit) *
        Translate(0.75, 0.0, 0.0) *
        Scale(0.8, 0.8, 1.0);

    drawObject(planet2Model, vec4(0.3, 1.0, 0.4, 1.0));
}

int main()
{
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Solar System Progress", NULL, NULL);
    if (!window) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glViewport(0, 0, 800, 600);

    init();

    while (!glfwWindowShouldClose(window)) {
        planet1Orbit += 0.8f;
        planet2Orbit += 0.45f;
        moonOrbit    += 2.0f;

        if (planet1Orbit > 360.0f) planet1Orbit -= 360.0f;
        if (planet2Orbit > 360.0f) planet2Orbit -= 360.0f;
        if (moonOrbit > 360.0f) moonOrbit -= 360.0f;

        display();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
