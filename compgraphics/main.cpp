//
//  main.cpp
//  compgraphics — Solar System
//

#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>

// stb_image (optional — drop stb_image.h next to this file and define HAVE_STB_IMAGE
// in Other C++ Flags to enable real texture loading; planets render with solid
// colors otherwise).
#ifdef HAVE_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include "Angel.h"

static void updateWindowTitle(GLFWwindow* window);
static void buildOrbitCircle(std::vector<float>& verts, int segments = 200);

static void buildSphere(int stacks, int sectors,
                        std::vector<float>& verts,
                        std::vector<unsigned int>& idx)
{
    verts.clear();
    idx.clear();

    for (int i = 0; i <= stacks; ++i) {
        float phi = (float)M_PI * 0.5f - i * (float)M_PI / stacks;
        float y   = sinf(phi);
        float xzr = cosf(phi);

        for (int j = 0; j <= sectors; ++j) {
            float theta = j * 2.0f * (float)M_PI / sectors;
            float x = xzr * cosf(theta);
            float z = xzr * sinf(theta);

            // position
            verts.push_back(x); verts.push_back(y); verts.push_back(z);
            // normal
            verts.push_back(x); verts.push_back(y); verts.push_back(z);
            // texcoord
            verts.push_back((float)j / sectors);
            verts.push_back((float)i / stacks);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            unsigned int k1 = i * (sectors + 1) + j;
            unsigned int k2 = k1 + sectors + 1;
            if (i != 0)          { idx.push_back(k1);   idx.push_back(k2);   idx.push_back(k1 + 1); }
            if (i != stacks - 1) { idx.push_back(k1+1); idx.push_back(k2);   idx.push_back(k2 + 1); }
        }
    }
}

// orbit circle geometry

static void buildOrbitCircle(std::vector<float>& verts, int segments)
{
    verts.clear();

    for (int i = 0; i <= segments; ++i) {
        float t = 2.0f * (float)M_PI * i / segments;
        float x = cosf(t);
        float z = sinf(t);

        // position
        verts.push_back(x);
        verts.push_back(0.0f);
        verts.push_back(z);

        // dummy normal
        verts.push_back(0.0f);
        verts.push_back(1.0f);
        verts.push_back(0.0f);

        // dummy texcoord
        verts.push_back(0.0f);
        verts.push_back(0.0f);
    }
}

// ── Texture loading ───────────────────────────────────────────────────────────

static GLuint loadTex(const char* path, const unsigned char fallbackRGBA[4])
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

#ifdef HAVE_STB_IMAGE
    stbi_set_flip_vertically_on_load(true);
    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        stbi_image_free(data);
    } else {
        std::cerr << "Texture not found: " << path << " — using solid color fallback\n";
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallbackRGBA);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
#else
    (void)path;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallbackRGBA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// ── Planet data ───────────────────────────────────────────────────────────────

struct Planet {
    float orbitRadius;
    float orbitSpeed;
    float spinSpeed;
    float scale;
    float orbitAngle;
    float spinAngle;
    int   parentIndex;
    GLuint texID;
    bool  isEmissive;
    const char* texPath;
    unsigned char color[4];
};

static const int NUM_PLANETS = 6;
static Planet planets[NUM_PLANETS] = {
    { 0.0f,  0.0f,  15.0f, 1.2f,  0, 0, -1, 0, true,  "textures/sun.jpg",     {255, 200,  50, 255} },
    { 2.0f, 47.0f,  10.0f, 0.15f, 0, 0,  0, 0, false, "textures/mercury.jpg", {180, 160, 140, 255} },
    { 3.0f, 35.0f,   5.0f, 0.25f, 0, 0,  0, 0, false, "textures/venus.jpg",   {220, 180, 100, 255} },
    { 4.5f, 29.0f,  25.0f, 0.27f, 0, 0,  0, 0, false, "textures/earth.jpg",   { 50, 100, 200, 255} },
    { 0.6f,130.0f,  15.0f, 0.10f, 0, 0,  3, 0, false, "textures/moon.jpg",    {200, 200, 200, 255} },
    { 6.5f, 24.0f,  24.0f, 0.18f, 0, 0,  0, 0, false, "textures/mars.jpg",    {200,  80,  40, 255} },
};

// ── GL objects ────────────────────────────────────────────────────────────────

static GLuint vao, vbo, ebo;
static GLuint orbitVAO, orbitVBO;
static GLuint program;
static unsigned int sphereIndexCount;
static unsigned int orbitVertexCount;

static GLint uModel, uView, uProj;
static GLint uLightPos, uViewPos, uLightColor;
static GLint uIsEmissive, uTex;
static GLint uUseSolidColor, uSolidColor;

// ── Camera ────────────────────────────────────────────────────────────────────

static vec3  defaultCamPos(0.0f, 2.0f, 14.0f);
static float defaultYaw = -90.0f;
static float defaultPitch = 0.0f;

static vec3  camPos(0.0f, 2.0f, 14.0f);
static vec3  camFront(0.0f, 0.0f, -1.0f);
static vec3  camUp(0.0f, 1.0f, 0.0f);
static float camYaw   = -90.0f;
static float camPitch =   0.0f;
static float camSpeed =   5.0f;

static int   winWidth = 800, winHeight = 600;
static float lastMouseX = 400.0f, lastMouseY = 300.0f;
static bool  firstMouse = true;
static bool  mouseCaptured = false;

// ── Timing / simulation ───────────────────────────────────────────────────────

static float lastTime = 0.0f;
static float timeScale = 1.0f;
static bool  paused = false;

// ── UI helpers ────────────────────────────────────────────────────────────────

static void updateWindowTitle(GLFWwindow* window)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    oss << "Solar System | "
        << "M: Toggle Mouse | "
        << "WASD: Move | "
        << "Space: Pause/Resume | "
        << "↑/↓: Speed | "
        << "R: Reset | "
        << "ESC: Quit | ";

    if (paused) oss << "Status: Paused";
    else oss << "Speed: " << timeScale << "x";

    glfwSetWindowTitle(window, oss.str().c_str());
}

static void resetToDefaults(GLFWwindow* window)
{
    timeScale = 1.0f;
    paused = false;

    camPos = defaultCamPos;
    camYaw = defaultYaw;
    camPitch = defaultPitch;

    float yr = camYaw * (float)M_PI / 180.0f;
    float pr = camPitch * (float)M_PI / 180.0f;
    camFront = normalize(vec3(
        cosf(yr) * cosf(pr),
        sinf(pr),
        sinf(yr) * cosf(pr)
    ));

    mouseCaptured = false;
    firstMouse = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    updateWindowTitle(window);
    std::cout << "Reset to default\n";
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

static void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        mouseCaptured = !mouseCaptured;

        if (mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        updateWindowTitle(window);
    }

    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        resetToDefaults(window);
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_SPACE) {
            paused = !paused;
            updateWindowTitle(window);
            std::cout << (paused ? "Paused\n" : "Resumed\n");
        }
        else if (key == GLFW_KEY_UP) {
            timeScale *= 1.5f;
            if (timeScale > 50.0f) timeScale = 50.0f;
            updateWindowTitle(window);
            std::cout << "Time scale: " << timeScale << "x\n";
        }
        else if (key == GLFW_KEY_DOWN) {
            timeScale /= 1.5f;
            if (timeScale < 0.1f) timeScale = 0.1f;
            updateWindowTitle(window);
            std::cout << "Time scale: " << timeScale << "x\n";
        }
    }
}

static void cursorCallback(GLFWwindow*, double xpos, double ypos)
{
    if (!mouseCaptured) return;

    if (firstMouse) {
        lastMouseX = (float)xpos;
        lastMouseY = (float)ypos;
        firstMouse = false;
        return;
    }

    float dx = (float)(xpos - lastMouseX) * 0.1f;
    float dy = (float)(lastMouseY - ypos) * 0.1f;
    lastMouseX = (float)xpos;
    lastMouseY = (float)ypos;

    camYaw   += dx;
    camPitch += dy;
    if (camPitch >  89.0f) camPitch =  89.0f;
    if (camPitch < -89.0f) camPitch = -89.0f;

    float yr = camYaw   * (float)M_PI / 180.0f;
    float pr = camPitch * (float)M_PI / 180.0f;
    camFront = normalize(vec3(cosf(yr) * cosf(pr),
                              sinf(pr),
                              sinf(yr) * cosf(pr)));
}

static void framebufferSizeCallback(GLFWwindow*, int width, int height)
{
    if (height == 0) height = 1;
    winWidth = width;
    winHeight = height;
    glViewport(0, 0, winWidth, winHeight);
}

// ── Initialisation ────────────────────────────────────────────────────────────

static void init()
{
    std::vector<float>        verts;
    std::vector<unsigned int> idx;
    buildSphere(36, 72, verts, idx);
    sphereIndexCount = (unsigned int)idx.size();

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float),
                 verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int),
                 idx.data(), GL_STATIC_DRAW);

    program = Angel::InitShader("shaders/vshader.glsl", "shaders/fshader.glsl");

    GLuint posLoc = glGetAttribLocation(program, "vPosition");
    GLuint norLoc = glGetAttribLocation(program, "vNormal");
    GLuint texLoc = glGetAttribLocation(program, "vTexCoord");

    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(norLoc);
    glVertexAttribPointer(norLoc, 3, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(texLoc);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void*)(6 * sizeof(float)));

    // Orbit circle setup
    std::vector<float> orbitVerts;
    buildOrbitCircle(orbitVerts);
    orbitVertexCount = (unsigned int)(orbitVerts.size() / 8);

    glGenVertexArrays(1, &orbitVAO);
    glBindVertexArray(orbitVAO);

    glGenBuffers(1, &orbitVBO);
    glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
    glBufferData(GL_ARRAY_BUFFER, orbitVerts.size() * sizeof(float),
                 orbitVerts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(norLoc);
    glVertexAttribPointer(norLoc, 3, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(texLoc);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void*)(6 * sizeof(float)));

    uModel         = glGetUniformLocation(program, "model");
    uView          = glGetUniformLocation(program, "view");
    uProj          = glGetUniformLocation(program, "proj");
    uLightPos      = glGetUniformLocation(program, "lightPos");
    uViewPos       = glGetUniformLocation(program, "viewPos");
    uLightColor    = glGetUniformLocation(program, "lightColor");
    uIsEmissive    = glGetUniformLocation(program, "isEmissive");
    uTex           = glGetUniformLocation(program, "uTex");
    uUseSolidColor = glGetUniformLocation(program, "useSolidColor");
    uSolidColor    = glGetUniformLocation(program, "solidColor");

    for (int i = 0; i < NUM_PLANETS; ++i)
        planets[i].texID = loadTex(planets[i].texPath, planets[i].color);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.06f, 1.0f);
}

// ── Display ───────────────────────────────────────────────────────────────────

static void display(float dt)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (int i = 0; i < NUM_PLANETS; ++i) {
        planets[i].orbitAngle += planets[i].orbitSpeed * dt;
        planets[i].spinAngle  += planets[i].spinSpeed  * dt;
        if (planets[i].orbitAngle > 360.0f) planets[i].orbitAngle -= 360.0f;
        if (planets[i].spinAngle  > 360.0f) planets[i].spinAngle  -= 360.0f;
    }

    mat4 orbitTF[NUM_PLANETS];
    for (int i = 0; i < NUM_PLANETS; ++i) {
        int p = planets[i].parentIndex;
        if (p == -1) {
            orbitTF[i] = mat4();
        } else {
            orbitTF[i] = orbitTF[p]
                       * RotateY(planets[i].orbitAngle)
                       * Translate(planets[i].orbitRadius, 0.0f, 0.0f);
        }
    }

    mat4 view = LookAt(camPos, camPos + camFront, camUp);
    mat4 proj = Perspective(45.0f, (float)winWidth / (float)winHeight, 0.1f, 200.0f);

    glUniformMatrix4fv(uView, 1, GL_TRUE, view);
    glUniformMatrix4fv(uProj, 1, GL_TRUE, proj);
    glUniform3f(uLightPos,   0.0f, 0.0f, 0.0f);
    glUniform3f(uViewPos,    camPos.x, camPos.y, camPos.z);
    glUniform3f(uLightColor, 1.0f, 1.0f, 1.0f);
    glUniform1i(uTex, 0);

    // Draw orbit paths
    glUniform1i(uUseSolidColor, 1);
    glUniform3f(uSolidColor, 0.35f, 0.35f, 0.35f);
    glUniform1i(uIsEmissive, 0);

    glBindVertexArray(orbitVAO);

    for (int i = 0; i < NUM_PLANETS; ++i) {
        if (planets[i].parentIndex == -1) continue;

        mat4 orbitModel;
        if (planets[i].parentIndex == 0) {
            orbitModel = Scale(planets[i].orbitRadius, 1.0f, planets[i].orbitRadius);
        } else {
            orbitModel = orbitTF[planets[i].parentIndex]
                       * Scale(planets[i].orbitRadius, 1.0f, planets[i].orbitRadius);
        }

        glUniformMatrix4fv(uModel, 1, GL_TRUE, orbitModel);
        glDrawArrays(GL_LINE_STRIP, 0, orbitVertexCount);
    }

    // Draw planets
    glUniform1i(uUseSolidColor, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);

    for (int i = 0; i < NUM_PLANETS; ++i) {
        Planet& p = planets[i];

        mat4 model = orbitTF[i]
                   * RotateY(p.spinAngle)
                   * Scale(p.scale, p.scale, p.scale);

        glUniformMatrix4fv(uModel, 1, GL_TRUE, model);
        glUniform1i(uIsEmissive, p.isEmissive ? 1 : 0);
        glBindTexture(GL_TEXTURE_2D, p.texID);

        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────

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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    int windowWidth = 1280;
    int windowHeight = 800;
    lastMouseX = windowWidth * 0.5f;
    lastMouseY = windowHeight * 0.5f;

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Solar System", nullptr, nullptr);
    if (!window) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    winWidth = fbWidth;
    winHeight = fbHeight;
    glViewport(0, 0, winWidth, winHeight);

    updateWindowTitle(window);

    std::cout << "Controls:\n"
              << "M   -> toggle mouse capture\n"
              << "WASD-> move camera\n"
              << "Space -> pause/resume\n"
              << "Up/Down -> speed up / slow down\n"
              << "R   -> reset defaults\n"
              << "ESC -> quit\n";

    init();

    lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        if (paused) dt = 0.0f;
        else dt *= timeScale;

        if (mouseCaptured) {
            vec3 right = normalize(cross(camFront, camUp));
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos = camPos + camFront * (camSpeed * dt);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos = camPos - camFront * (camSpeed * dt);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos = camPos - right    * (camSpeed * dt);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos = camPos + right    * (camSpeed * dt);
        }

        display(dt);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
