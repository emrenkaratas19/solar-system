//
//  main.cpp
//  compgraphics — Solar System (final)
//

// Force-enable optional features unconditionally (no Xcode build flag needed).
// Requires stb_image.h and stb_easy_font.h to sit next to this file.
#ifndef HAVE_STB_IMAGE
#define HAVE_STB_IMAGE
#endif
#ifndef HAVE_STB_EASY_FONT
#define HAVE_STB_EASY_FONT
#endif

#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unistd.h>
#include <sys/stat.h>
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>

#ifdef HAVE_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#ifdef HAVE_STB_EASY_FONT
#include "stb_easy_font.h"
#endif

#include "Angel.h"

// ── Forward declarations ─────────────────────────────────────────────────────
static void updateWindowTitle(GLFWwindow* window);
static void buildOrbitCircle(std::vector<float>& verts, int segments = 200);
static void drawTextOverlay(GLFWwindow* window, const mat4& view, const mat4& proj);
static void displayGravityScene(GLFWwindow* window, float realDt);

// ── Sphere geometry ──────────────────────────────────────────────────────────

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

            verts.push_back(x); verts.push_back(y); verts.push_back(z);
            verts.push_back(x); verts.push_back(y); verts.push_back(z);
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

// ── Orbit circle geometry ────────────────────────────────────────────────────

static void buildOrbitCircle(std::vector<float>& verts, int segments)
{
    verts.clear();
    for (int i = 0; i <= segments; ++i) {
        float t = 2.0f * (float)M_PI * i / segments;
        float x = cosf(t);
        float z = sinf(t);

        verts.push_back(x);    verts.push_back(0.0f); verts.push_back(z);
        verts.push_back(0.0f); verts.push_back(1.0f); verts.push_back(0.0f);
        verts.push_back(0.0f); verts.push_back(0.0f);
    }
}

// ── Texture loading ──────────────────────────────────────────────────────────

// Search for a texture file in several places — the runtime working directory
// can vary depending on how the app is launched (Xcode vs Terminal vs bundle).
static std::string findTexture(const char* relPath) {
    // 1. as given (cwd-relative)
    struct stat st;
    if (stat(relPath, &st) == 0) return relPath;
    // 2. parent of cwd
    {
        std::string p = std::string("../") + relPath;
        if (stat(p.c_str(), &st) == 0) return p;
    }
    // 3. project folder (hardcoded fallback — last resort)
    {
        std::string p = std::string("/Users/daryacetinkaya/Desktop/solar-system-main 3/") + relPath;
        if (stat(p.c_str(), &st) == 0) return p;
    }
    return "";
}

static GLuint loadTex(const char* path, const unsigned char fallbackRGBA[4])
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

#ifdef HAVE_STB_IMAGE
    stbi_set_flip_vertically_on_load(true);
    std::string resolved = findTexture(path);
    int w = 0, h = 0, ch = 0;
    unsigned char* data = nullptr;
    if (!resolved.empty()) {
        data = stbi_load(resolved.c_str(), &w, &h, &ch, 4);
    }
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        stbi_image_free(data);
        std::cout << "[tex OK ] " << resolved << "  (" << w << "x" << h << ")\n";
    } else {
        if (resolved.empty()) {
            std::cerr << "[tex MISS] " << path << "  (no file found via any path)\n";
        } else {
            std::cerr << "[tex FAIL] " << resolved << "  (stb_image: " << stbi_failure_reason() << ")\n";
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallbackRGBA);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
#else
    std::cerr << "[tex SKIP] " << path << "  (HAVE_STB_IMAGE not defined at compile time)\n";
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallbackRGBA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// ── Planet data ──────────────────────────────────────────────────────────────

struct Planet {
    const char*   name;
    float         orbitRadius;
    float         orbitSpeed;     // degrees / second (sim time)
    float         spinSpeed;      // degrees / second (sim time)
    float         scale;          // visual radius
    float         axialTilt;      // degrees
    float         orbitAngle;
    float         spinAngle;
    int           parentIndex;    // -1 = root (Sun)
    GLuint        texID;
    bool          isEmissive;
    const char*   texPath;
    unsigned char color[4];

    // info-display fields
    float         diameterKm;
    float         dayHours;
    float         yearDays;
    float         gravityMS2;
    const char*   description;

    // computed each frame (used by mouse picking & camera focus)
    vec3          worldPos;
};

// Sun + 8 planets + key moons. parentIndex refers to position in this array.
static const int NUM_PLANETS = 16;
static Planet planets[NUM_PLANETS] = {
    // name        orb_R  orb_S spin_S scale  tilt  oA  sA  par  tex  emis  texPath                 fallback color           diam_km    day_h     year_d   g     description
    { "Sun",       0.0f,   0.0f,  5.0f, 1.50f,  7.25f, 0, 0, -1, 0, true,  "textures/sun.jpg",      {255, 200,  50, 255}, 1392700.0f,  609.6f,      0.0f, 274.0f, "G2V yellow dwarf star at the center of our system." },
    { "Mercury",   2.5f,  60.0f,  8.0f, 0.10f,  0.03f, 0, 0,  0, 0, false, "textures/mercury.jpg",  {180, 160, 140, 255},    4879.0f, 1407.6f,     88.0f,   3.7f, "Smallest planet; closest to the Sun." },
    { "Venus",     3.5f,  30.0f, -3.0f, 0.18f,177.40f, 0, 0,  0, 0, false, "textures/venus.jpg",    {220, 180, 100, 255},   12104.0f,-5832.5f,    224.7f,   8.87f,"Hottest planet; spins backward (retrograde)." },
    { "Earth",     4.7f,  20.0f, 30.0f, 0.20f, 23.44f, 0, 0,  0, 0, false, "textures/earth.jpg",    { 50, 100, 200, 255},   12742.0f,   24.0f,    365.25f,  9.81f,"Our home; the only known world with surface liquid water." },
    { "Moon",      0.55f, 90.0f,  5.0f, 0.06f,  6.68f, 0, 0,  3, 0, false, "textures/moon.jpg",     {200, 200, 200, 255},    3474.0f,  708.0f,     27.3f,   1.62f,"Earth's only natural satellite; tidally locked." },
    { "Mars",      6.5f,  14.0f, 28.0f, 0.15f, 25.19f, 0, 0,  0, 0, false, "textures/mars.jpg",     {200,  80,  40, 255},    6779.0f,   24.6f,    687.0f,   3.71f,"The Red Planet; iron-oxide dust gives the surface its color." },
    { "Phobos",    0.30f,200.0f, 50.0f, 0.025f, 1.10f, 0, 0,  5, 0, false, "textures/phobos.jpg",   {130, 110,  90, 255},      22.4f,    7.7f,      0.32f,  0.006f,"Mars' larger moon; slowly spiraling inward." },
    { "Deimos",    0.45f, 80.0f, 40.0f, 0.020f, 1.79f, 0, 0,  5, 0, false, "textures/deimos.jpg",   {140, 130, 110, 255},      12.4f,   30.3f,      1.26f,  0.003f,"Mars' smaller, more distant moon." },
    { "Jupiter",  10.0f,   8.0f, 80.0f, 0.55f,  3.13f, 0, 0,  0, 0, false, "textures/jupiter.jpg",  {210, 170, 120, 255},  139820.0f,    9.9f,   4333.0f,  24.79f,"Largest planet; gas giant with a Great Red Spot storm." },
    { "Io",        0.85f,100.0f, 50.0f, 0.07f,  0.05f, 0, 0,  8, 0, false, "textures/io.jpg",       {220, 200, 130, 255},    3643.0f,   42.5f,      1.77f,  1.80f,"Most volcanically active body in the Solar System." },
    { "Europa",    1.10f, 70.0f, 30.0f, 0.06f,  0.10f, 0, 0,  8, 0, false, "textures/europa.jpg",   {210, 200, 180, 255},    3122.0f,   85.2f,      3.55f,  1.31f,"Icy moon of Jupiter; suspected subsurface ocean." },
    { "Ganymede",  1.40f, 50.0f, 25.0f, 0.09f,  0.33f, 0, 0,  8, 0, false, "textures/ganymede.jpg", {180, 170, 150, 255},    5268.0f,  171.7f,      7.15f,  1.43f,"Largest moon in the Solar System (bigger than Mercury)." },
    { "Saturn",   14.0f,   5.0f, 70.0f, 0.50f, 26.73f, 0, 0,  0, 0, false, "textures/saturn.jpg",   {220, 200, 160, 255},  116460.0f,   10.7f,  10759.0f,  10.44f,"Famous for its bright ring system of ice and rock." },
    { "Titan",     1.10f, 35.0f, 20.0f, 0.10f,  0.30f, 0, 0, 12, 0, false, "textures/titan.jpg",    {200, 170, 100, 255},    5150.0f,  382.7f,     15.9f,   1.35f,"Saturn's largest moon; thick atmosphere and methane lakes." },
    { "Uranus",   18.0f,   3.0f, 50.0f, 0.30f, 97.77f, 0, 0,  0, 0, false, "textures/uranus.jpg",   {150, 200, 220, 255},   50724.0f,  -17.2f,  30687.0f,   8.69f,"Ice giant tipped on its side; rotates almost lying down." },
    { "Neptune",  22.0f,   2.0f, 60.0f, 0.28f, 28.32f, 0, 0,  0, 0, false, "textures/neptune.jpg",  {100, 130, 220, 255},   49244.0f,   16.1f,  60190.0f,  11.15f,"Windiest planet; deep blue methane atmosphere." },
};

// ── GL objects ───────────────────────────────────────────────────────────────

static GLuint vao, vbo, ebo;
static GLuint orbitVAO, orbitVBO;
static GLuint program;
static unsigned int sphereIndexCount;
static unsigned int orbitVertexCount;

static GLint uModel, uView, uProj;
static GLint uLightPos, uViewPos, uLightColor;
static GLint uIsEmissive, uTex;
static GLint uUseSolidColor, uSolidColor;
static GLint uHighlight;

// text overlay GL objects
static GLuint textProgram = 0;
static GLuint textVAO = 0, textVBO = 0, textEBO = 0;
static GLint  uTextProj = -1, uTextColor = -1;

// selection marker (2D ring drawn at the projected position of the selected body)
static GLuint markerVAO = 0, markerVBO = 0;
static const int MARKER_SEGMENTS = 48;

// readable text size (vertices scaled around the anchor); 2.0 ~ doubles the size
static const float TEXT_SCALE = 2.0f;

// ── Camera ───────────────────────────────────────────────────────────────────

static vec3  defaultCamPos(0.0f, 4.0f, 22.0f);
static float defaultYaw = -90.0f;
static float defaultPitch = -10.0f;

static vec3  camPos    = defaultCamPos;
static vec3  camFront(0.0f, 0.0f, -1.0f);
static vec3  camUp(0.0f, 1.0f, 0.0f);
static float camYaw    = defaultYaw;
static float camPitch  = defaultPitch;
static float camSpeed  = 8.0f;

static int   winWidth = 1280, winHeight = 800;
static float lastMouseX = 640.0f, lastMouseY = 400.0f;
static bool  firstMouse = true;
static bool  mouseCaptured = false;

// ── Selection / focus ────────────────────────────────────────────────────────

static int   selectedPlanet = -1;   // -1 = none
static int   focusedPlanet  = -1;   // -1 = free fly
static float focusFollowDistance = 0.0f;

// ── Scene mode (solar system vs gravity simulator) ───────────────────────────

enum SceneMode { SCENE_SOLAR = 0, SCENE_GRAVITY = 1 };
static SceneMode sceneMode = SCENE_SOLAR;

// Gravity-sim state. When entering SCENE_GRAVITY we snapshot the
// selected planet's gravity and name so changing the selection
// in the background doesn't perturb an in-progress simulation.
static float       gravG          = 9.81f;   // m/s²
static const char* gravPlanetName = "Earth";
static float       gravStartY     = 50.0f;   // metres above ground
static float       gravObjY       = 50.0f;   // current height (m)
static float       gravObjVy      = 0.0f;    // current downward velocity (m/s)
static bool        gravFalling    = false;
static float       gravElapsed    = 0.0f;    // seconds since drop started
static float       gravPeakV      = 0.0f;    // max velocity attained
static bool        gravLanded     = false;

// ── Timing / simulation ──────────────────────────────────────────────────────

static float lastTime  = 0.0f;
static float timeScale = 1.0f;
static bool  paused    = false;

// ── Helpers ──────────────────────────────────────────────────────────────────

static inline mat4 Ortho(float l, float r, float b, float t, float n, float f) {
    mat4 M(0.0f);
    M[0][0] =  2.0f / (r - l);
    M[1][1] =  2.0f / (t - b);
    M[2][2] = -2.0f / (f - n);
    M[0][3] = -(r + l) / (r - l);
    M[1][3] = -(t + b) / (t - b);
    M[2][3] = -(f + n) / (f - n);
    M[3][3] =  1.0f;
    return M;
}

static void updateCamFrontFromAngles() {
    float yr = camYaw   * (float)M_PI / 180.0f;
    float pr = camPitch * (float)M_PI / 180.0f;
    camFront = normalize(vec3(cosf(yr) * cosf(pr),
                              sinf(pr),
                              sinf(yr) * cosf(pr)));
}

static void updateAnglesFromCamFront() {
    camPitch = asinf(camFront.y) * 180.0f / (float)M_PI;
    camYaw   = atan2f(camFront.z, camFront.x) * 180.0f / (float)M_PI;
}

static void updateWindowTitle(GLFWwindow* window) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    if (sceneMode == SCENE_GRAVITY) {
        oss << "Gravity Demo  |  " << gravPlanetName
            << "  g = " << gravG << " m/s^2"
            << "  drop = " << gravStartY << " m"
            << "  |  Space:drop  R:reset  Up/Down:height  G:exit  ESC:quit";
    } else {
        oss << "Solar System  |  M:Mouse  WASD:Move  Click:Select  F:Focus  G:Gravity  Space:Pause  Up/Down:Speed  R:Reset  ESC:Quit  |  ";
        if (paused) oss << "Paused";
        else        oss << "Speed " << timeScale << "x";
        if (selectedPlanet >= 0) oss << "  |  Selected: " << planets[selectedPlanet].name;
        if (focusedPlanet  >= 0) oss << "  |  Focused: "  << planets[focusedPlanet].name;
    }
    glfwSetWindowTitle(window, oss.str().c_str());
}

static void resetToDefaults(GLFWwindow* window) {
    timeScale = 1.0f;
    paused = false;
    camPos = defaultCamPos;
    camYaw = defaultYaw;
    camPitch = defaultPitch;
    updateCamFrontFromAngles();
    selectedPlanet = -1;
    focusedPlanet  = -1;
    mouseCaptured = false;
    firstMouse = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    updateWindowTitle(window);
    std::cout << "Reset to defaults\n";
}

// ── Mouse picking ────────────────────────────────────────────────────────────

static void pickPlanet(GLFWwindow* window, double mouseX, double mouseY) {
    // CRITICAL: glfwGetCursorPos returns WINDOW coords (e.g. 0..1280) while winWidth
    // holds FRAMEBUFFER pixels (e.g. 0..2560 on Retina). Normalize against window size.
    int wsW, wsH;
    glfwGetWindowSize(window, &wsW, &wsH);
    if (wsW <= 0 || wsH <= 0) return;

    float tanHalfFovy = tanf((45.0f * (float)M_PI / 180.0f) * 0.5f);
    float aspect = (float)wsW / (float)wsH;

    float xNdc = (2.0f * (float)mouseX / (float)wsW) - 1.0f;
    float yNdc = 1.0f - (2.0f * (float)mouseY / (float)wsH);

    vec3 right  = normalize(cross(camFront, camUp));
    vec3 trueUp = cross(right, camFront);

    vec3 rayDir = normalize(
        camFront +
        right  * (xNdc * tanHalfFovy * aspect) +
        trueUp * (yNdc * tanHalfFovy)
    );
    vec3 rayOrigin = camPos;

    // Generous picking: inflate each body's pick radius so small moons are easy to hit.
    int   bestIdx = -1;
    float bestT   = 1e30f;
    for (int i = 0; i < NUM_PLANETS; ++i) {
        vec3 oc = rayOrigin - planets[i].worldPos;
        float r = std::max(planets[i].scale * 1.6f, 0.08f);
        float b = dot(oc, rayDir);
        float c = dot(oc, oc) - r * r;
        float disc = b * b - c;
        if (disc < 0.0f) continue;
        float sq = sqrtf(disc);
        float t  = -b - sq;
        if (t < 0.0f) t = -b + sq;
        if (t > 0.0f && t < bestT) { bestT = t; bestIdx = i; }
    }

    selectedPlanet = bestIdx;
    std::cout << "[click " << mouseX << "," << mouseY
              << " window " << wsW << "x" << wsH << "] ";
    if (bestIdx >= 0) std::cout << "Selected: " << planets[bestIdx].name << "\n";
    else              std::cout << "Selection cleared (no body hit)\n";
}

// Select a body by index (with bounds check + console log).
static void selectByIndex(int idx) {
    if (idx < 0 || idx >= NUM_PLANETS) return;
    selectedPlanet = idx;
    std::cout << "Selected: " << planets[idx].name << " (#" << idx << ")\n";
}

// ── Callbacks ────────────────────────────────────────────────────────────────

// Helper used by the G key.
static void resetGravityObject() {
    gravObjY    = gravStartY;
    gravObjVy   = 0.0f;
    gravElapsed = 0.0f;
    gravPeakV   = 0.0f;
    gravFalling = false;
    gravLanded  = false;
}

static void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    // G toggles gravity sim mode (universal — works from either scene)
    if (key == GLFW_KEY_G && action == GLFW_PRESS) {
        if (sceneMode == SCENE_GRAVITY) {
            sceneMode = SCENE_SOLAR;
            std::cout << "Exited gravity mode\n";
        } else {
            if (selectedPlanet < 0) {
                std::cout << "(Gravity) Press 1-9 to select a planet first.\n";
            } else {
                sceneMode      = SCENE_GRAVITY;
                gravG          = planets[selectedPlanet].gravityMS2;
                gravPlanetName = planets[selectedPlanet].name;
                resetGravityObject();
                std::cout << "Entered gravity mode: " << gravPlanetName
                          << "  g = " << gravG << " m/s^2\n";
            }
        }
        updateWindowTitle(window);
        return;
    }

    // In gravity mode, Space / R / Up / Down control the simulation, not the solar scene.
    if (sceneMode == SCENE_GRAVITY) {
        if (action != GLFW_PRESS) return;
        if (key == GLFW_KEY_SPACE) {
            if (gravLanded) resetGravityObject();
            gravFalling = !gravFalling;
            std::cout << (gravFalling ? "Drop!\n" : "Paused\n");
        } else if (key == GLFW_KEY_R) {
            resetGravityObject();
            std::cout << "Gravity sim reset\n";
        } else if (key == GLFW_KEY_UP) {
            gravStartY += 10.0f;
            if (gravStartY > 500.0f) gravStartY = 500.0f;
            resetGravityObject();
            std::cout << "Drop height: " << gravStartY << " m\n";
        } else if (key == GLFW_KEY_DOWN) {
            gravStartY -= 10.0f;
            if (gravStartY < 10.0f) gravStartY = 10.0f;
            resetGravityObject();
            std::cout << "Drop height: " << gravStartY << " m\n";
        }
        return;
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        mouseCaptured = !mouseCaptured;
        if (mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
            // entering free-look breaks any active focus
            focusedPlanet = -1;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        updateWindowTitle(window);
    }

    if (key == GLFW_KEY_R && action == GLFW_PRESS) resetToDefaults(window);

    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        if (focusedPlanet >= 0) {
            focusedPlanet = -1;
            std::cout << "Focus released\n";
        } else if (selectedPlanet >= 0) {
            focusedPlanet = selectedPlanet;
            // Distance proportional to body radius, with a sensible minimum.
            float r = planets[focusedPlanet].scale;
            focusFollowDistance = std::max(r * 5.0f, 0.7f);
            std::cout << "Focused on " << planets[focusedPlanet].name << "\n";
        } else {
            std::cout << "(Focus) Click on a planet first to select it.\n";
        }
        updateWindowTitle(window);
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_SPACE) {
            paused = !paused;
            updateWindowTitle(window);
        } else if (key == GLFW_KEY_UP) {
            timeScale *= 1.5f;
            if (timeScale > 50.0f) timeScale = 50.0f;
            updateWindowTitle(window);
        } else if (key == GLFW_KEY_DOWN) {
            timeScale /= 1.5f;
            if (timeScale < 0.05f) timeScale = 0.05f;
            updateWindowTitle(window);
        }
        // Quick-select major bodies by number key:
        //   1=Sun  2=Mercury  3=Venus  4=Earth  5=Mars  6=Jupiter  7=Saturn  8=Uranus  9=Neptune
        //   0 clears selection
        else if (key == GLFW_KEY_1) { selectByIndex(0);  updateWindowTitle(window); }
        else if (key == GLFW_KEY_2) { selectByIndex(1);  updateWindowTitle(window); }
        else if (key == GLFW_KEY_3) { selectByIndex(2);  updateWindowTitle(window); }
        else if (key == GLFW_KEY_4) { selectByIndex(3);  updateWindowTitle(window); }
        else if (key == GLFW_KEY_5) { selectByIndex(5);  updateWindowTitle(window); }
        else if (key == GLFW_KEY_6) { selectByIndex(8);  updateWindowTitle(window); }
        else if (key == GLFW_KEY_7) { selectByIndex(12); updateWindowTitle(window); }
        else if (key == GLFW_KEY_8) { selectByIndex(14); updateWindowTitle(window); }
        else if (key == GLFW_KEY_9) { selectByIndex(15); updateWindowTitle(window); }
        else if (key == GLFW_KEY_0) { selectedPlanet = -1; std::cout << "Selection cleared\n"; updateWindowTitle(window); }
        // Cycle through ALL 16 bodies (including moons).
        // Multiple bindings so it works on any keyboard layout:
        //   Left arrow / Right arrow   — universal (physical scancodes)
        //   ,            / .           — physical comma/period
        //   [            / ]           — works on US/UK only (Turkish ğ/ü use AltGr)
        else if (key == GLFW_KEY_LEFT  || key == GLFW_KEY_COMMA  || key == GLFW_KEY_LEFT_BRACKET) {
            int n = (selectedPlanet < 0) ? NUM_PLANETS - 1
                                         : (selectedPlanet - 1 + NUM_PLANETS) % NUM_PLANETS;
            selectByIndex(n); updateWindowTitle(window);
        }
        else if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_PERIOD || key == GLFW_KEY_RIGHT_BRACKET) {
            int n = (selectedPlanet < 0) ? 0
                                         : (selectedPlanet + 1) % NUM_PLANETS;
            selectByIndex(n); updateWindowTitle(window);
        }
    }
}

static void cursorCallback(GLFWwindow*, double xpos, double ypos) {
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
    updateCamFrontFromAngles();
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    if (mouseCaptured) return;
    if (sceneMode != SCENE_SOLAR) return;   // no picking inside gravity sim
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    pickPlanet(window, mx, my);
    updateWindowTitle(window);
}

static void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    if (height == 0) height = 1;
    winWidth  = width;
    winHeight = height;
    glViewport(0, 0, winWidth, winHeight);
}

// ── Initialisation ───────────────────────────────────────────────────────────

static void initTextRendering() {
    // Vertex shader: takes 2D pixel-space position, projects with ortho.
    const char* vs =
        "#version 150\n"
        "in vec2 inPos;\n"
        "uniform mat4 uProj;\n"
        "void main() { gl_Position = uProj * vec4(inPos, 0.0, 1.0); }\n";
    const char* fs =
        "#version 150\n"
        "uniform vec4 uColor;\n"
        "out vec4 fragColor;\n"
        "void main() { fragColor = uColor; }\n";

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, nullptr); glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, nullptr); glCompileShader(f);

    textProgram = glCreateProgram();
    glAttachShader(textProgram, v);
    glAttachShader(textProgram, f);
    glBindAttribLocation(textProgram, 0, "inPos");
    glLinkProgram(textProgram);
    glDeleteShader(v); glDeleteShader(f);

    uTextProj  = glGetUniformLocation(textProgram, "uProj");
    uTextColor = glGetUniformLocation(textProgram, "uColor");

    glGenVertexArrays(1, &textVAO);
    glBindVertexArray(textVAO);

    glGenBuffers(1, &textVBO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0); // stb_easy_font: 16 bytes/vertex (xyz + color), we only use xy

    // EBO is required for glDrawElements in macOS Core Profile (no client-side indices allowed).
    glGenBuffers(1, &textEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textEBO);

    glBindVertexArray(0);

    // ── Selection-marker ring (unit circle, line loop) ──────────────────────
    std::vector<float> ringV;
    ringV.reserve(MARKER_SEGMENTS * 2);
    for (int i = 0; i < MARKER_SEGMENTS; ++i) {
        float t = i * 2.0f * (float)M_PI / MARKER_SEGMENTS;
        ringV.push_back(cosf(t));
        ringV.push_back(sinf(t));
    }
    glGenVertexArrays(1, &markerVAO);
    glBindVertexArray(markerVAO);
    glGenBuffers(1, &markerVBO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    glBufferData(GL_ARRAY_BUFFER, ringV.size() * sizeof(float), ringV.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

static void init() {
    std::vector<float>        verts;
    std::vector<unsigned int> idx;
    buildSphere(36, 72, verts, idx);
    sphereIndexCount = (unsigned int)idx.size();

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

    program = Angel::InitShader("shaders/vshader.glsl", "shaders/fshader.glsl");

    GLuint posLoc = glGetAttribLocation(program, "vPosition");
    GLuint norLoc = glGetAttribLocation(program, "vNormal");
    GLuint texLoc = glGetAttribLocation(program, "vTexCoord");

    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(norLoc);
    glVertexAttribPointer(norLoc, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(texLoc);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    // Orbit circle
    std::vector<float> orbitVerts;
    buildOrbitCircle(orbitVerts);
    orbitVertexCount = (unsigned int)(orbitVerts.size() / 8);

    glGenVertexArrays(1, &orbitVAO);
    glBindVertexArray(orbitVAO);
    glGenBuffers(1, &orbitVBO);
    glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
    glBufferData(GL_ARRAY_BUFFER, orbitVerts.size() * sizeof(float), orbitVerts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(norLoc);
    glVertexAttribPointer(norLoc, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(texLoc);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

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
    uHighlight     = glGetUniformLocation(program, "highlight");

    for (int i = 0; i < NUM_PLANETS; ++i)
        planets[i].texID = loadTex(planets[i].texPath, planets[i].color);

    initTextRendering();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.06f, 1.0f);

    updateCamFrontFromAngles();
}

// ── Display ──────────────────────────────────────────────────────────────────

static void display(GLFWwindow* window, float simDt, float realDt) {
    // Dispatch to gravity-sim render path when the user has toggled into it.
    if (sceneMode == SCENE_GRAVITY) {
        displayGravityScene(window, realDt);
        return;
    }

    glClearColor(0.02f, 0.02f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. Advance simulation (uses sim time — frozen when paused)
    for (int i = 0; i < NUM_PLANETS; ++i) {
        planets[i].orbitAngle += planets[i].orbitSpeed * simDt;
        planets[i].spinAngle  += planets[i].spinSpeed  * simDt;
        if (planets[i].orbitAngle >  360.0f) planets[i].orbitAngle -= 360.0f;
        if (planets[i].orbitAngle < -360.0f) planets[i].orbitAngle += 360.0f;
        if (planets[i].spinAngle  >  360.0f) planets[i].spinAngle  -= 360.0f;
        if (planets[i].spinAngle  < -360.0f) planets[i].spinAngle  += 360.0f;
    }

    // 2. Compute hierarchical orbit transforms (no spin/scale here).
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
        // Cache world-space center for picking & focus.
        planets[i].worldPos = vec3(orbitTF[i][0][3], orbitTF[i][1][3], orbitTF[i][2][3]);
    }

    // 3. Camera focus follow (uses REAL time so the snap works even when paused).
    if (focusedPlanet >= 0) {
        const Planet& tp = planets[focusedPlanet];
        vec3 desired = tp.worldPos + vec3(focusFollowDistance, focusFollowDistance * 0.4f, focusFollowDistance);
        float k = 1.0f - expf(-5.0f * std::max(realDt, 1.0f / 240.0f));
        camPos = camPos + (desired - camPos) * k;
        camFront = normalize(tp.worldPos - camPos);
        updateAnglesFromCamFront();
    }

    // 4. Build view/projection
    mat4 view = LookAt(camPos, camPos + camFront, camUp);
    mat4 proj = Perspective(45.0f, (float)winWidth / (float)winHeight, 0.1f, 500.0f);

    glUseProgram(program);
    glUniformMatrix4fv(uView, 1, GL_TRUE, view);
    glUniformMatrix4fv(uProj, 1, GL_TRUE, proj);
    glUniform3f(uLightPos,   0.0f, 0.0f, 0.0f);
    glUniform3f(uViewPos,    camPos.x, camPos.y, camPos.z);
    glUniform3f(uLightColor, 1.0f, 1.0f, 1.0f);
    glUniform1i(uTex, 0);

    // 5. Draw orbit paths (primary planets around Sun + moons around their parent)
    glUniform1i(uUseSolidColor, 1);
    glUniform3f(uSolidColor, 0.30f, 0.30f, 0.35f);
    glUniform1i(uIsEmissive, 0);
    glUniform1f(uHighlight, 0.0f);

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

    // 6. Draw planets
    glUniform1i(uUseSolidColor, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);

    for (int i = 0; i < NUM_PLANETS; ++i) {
        Planet& p = planets[i];
        // model: orbitTF * tilt * spin * scale  (so planet spins about its tilted axis)
        mat4 model = orbitTF[i]
                   * RotateZ(p.axialTilt)
                   * RotateY(p.spinAngle)
                   * Scale(p.scale, p.scale, p.scale);

        glUniformMatrix4fv(uModel, 1, GL_TRUE, model);
        glUniform1i(uIsEmissive, p.isEmissive ? 1 : 0);
        glUniform1f(uHighlight, (i == selectedPlanet) ? 1.0f : 0.0f);
        glBindTexture(GL_TEXTURE_2D, p.texID);

        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);
    }

    // 7. Text overlay (HUD + selected-planet info + 2D selection marker)
    drawTextOverlay(window, view, proj);
}

// ── Text overlay ─────────────────────────────────────────────────────────────

#ifdef HAVE_STB_EASY_FONT
static void drawTextString(float x, float y, const char* text) {
    static char buffer[99999];
    int numQuads = stb_easy_font_print(x, y, (char*)text, nullptr, buffer, sizeof(buffer));
    if (numQuads <= 0) return;

    // Scale all vertices around the anchor (x, y) so the text reads cleanly on Retina.
    // stb_easy_font emits 16 bytes / vertex (3 floats xyz + 4 bytes packed color).
    float* vp = (float*)buffer;
    int numVerts = numQuads * 4;
    for (int i = 0; i < numVerts; ++i) {
        vp[i*4 + 0] = x + (vp[i*4 + 0] - x) * TEXT_SCALE;
        vp[i*4 + 1] = y + (vp[i*4 + 1] - y) * TEXT_SCALE;
    }

    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, numQuads * 4 * 16, buffer, GL_DYNAMIC_DRAW);

    // Core Profile requires indices to live in a GPU buffer (no client arrays).
    static std::vector<unsigned short> idx;
    static int idxCapacityQuads = 0;
    if (numQuads > idxCapacityQuads) {
        idx.resize(numQuads * 6);
        for (int q = 0; q < numQuads; ++q) {
            unsigned short b = (unsigned short)(q * 4);
            idx[q*6+0] = b + 0; idx[q*6+1] = b + 1; idx[q*6+2] = b + 2;
            idx[q*6+3] = b + 0; idx[q*6+4] = b + 2; idx[q*6+5] = b + 3;
        }
        idxCapacityQuads = numQuads;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     idx.size() * sizeof(unsigned short),
                     idx.data(), GL_STATIC_DRAW);
    }

    glDrawElements(GL_TRIANGLES, numQuads * 6, GL_UNSIGNED_SHORT, (void*)0);
}

// Multiply a row-major mat4 by a vec4 (Angel-style).
static inline vec4 mulMV(const mat4& M, const vec4& v) {
    return vec4(
        M[0][0]*v.x + M[0][1]*v.y + M[0][2]*v.z + M[0][3]*v.w,
        M[1][0]*v.x + M[1][1]*v.y + M[1][2]*v.z + M[1][3]*v.w,
        M[2][0]*v.x + M[2][1]*v.y + M[2][2]*v.z + M[2][3]*v.w,
        M[3][0]*v.x + M[3][1]*v.y + M[3][2]*v.z + M[3][3]*v.w
    );
}

// Draw a bright cyan ring at the projected screen position of the selected planet.
// Window-space sized so it's always clearly visible, no matter how far the body is.
static void drawSelectionMarker(GLFWwindow* window, const mat4& view, const mat4& proj) {
    if (selectedPlanet < 0) return;
    const Planet& p = planets[selectedPlanet];

    // Center: project worldPos
    vec4 wp = vec4(p.worldPos.x, p.worldPos.y, p.worldPos.z, 1.0f);
    vec4 clip = mulMV(proj, mulMV(view, wp));
    if (clip.w <= 0.001f) return;  // behind camera
    float nx = clip.x / clip.w;
    float ny = clip.y / clip.w;

    // Project an offset point so we get screen-space radius matching the body.
    vec3 right = normalize(cross(camFront, camUp));
    vec4 wpEdge = vec4(p.worldPos.x + right.x * p.scale,
                       p.worldPos.y + right.y * p.scale,
                       p.worldPos.z + right.z * p.scale,
                       1.0f);
    vec4 clipE = mulMV(proj, mulMV(view, wpEdge));
    float ex = clipE.x / clipE.w;
    float screenRadiusNdc = fabsf(ex - nx);

    int wsW, wsH;
    glfwGetWindowSize(window, &wsW, &wsH);
    float cx = (nx * 0.5f + 0.5f) * wsW;
    float cy = (1.0f - (ny * 0.5f + 0.5f)) * wsH;     // flip Y to window coords
    float rPx = screenRadiusNdc * 0.5f * wsW;
    rPx = std::max(rPx * 1.6f, 22.0f);                 // bracket the body with a generous minimum

    // Build per-frame ring quad transform via a small ortho + per-vertex scale in shader.
    // Simpler: send an ortho whose units are pixels, then submit (cx + r*cos, cy + r*sin) directly.
    // We already use such an ortho for text. Reuse it.
    // Build the actual 32-vertex ring on CPU, upload, draw.
    static std::vector<float> ringPx;
    ringPx.resize(MARKER_SEGMENTS * 2);
    for (int i = 0; i < MARKER_SEGMENTS; ++i) {
        float t = i * 2.0f * (float)M_PI / MARKER_SEGMENTS;
        ringPx[i*2+0] = cx + rPx * cosf(t);
        ringPx[i*2+1] = cy + rPx * sinf(t);
    }

    glBindVertexArray(markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    glBufferData(GL_ARRAY_BUFFER, ringPx.size() * sizeof(float), ringPx.data(), GL_DYNAMIC_DRAW);

    glUniform4f(uTextColor, 0.4f, 0.95f, 1.0f, 1.0f);
    glDrawArrays(GL_LINE_LOOP, 0, MARKER_SEGMENTS);

    // Slightly larger second ring for emphasis
    for (int i = 0; i < MARKER_SEGMENTS; ++i) {
        float t = i * 2.0f * (float)M_PI / MARKER_SEGMENTS;
        ringPx[i*2+0] = cx + (rPx + 4.0f) * cosf(t);
        ringPx[i*2+1] = cy + (rPx + 4.0f) * sinf(t);
    }
    glBufferData(GL_ARRAY_BUFFER, ringPx.size() * sizeof(float), ringPx.data(), GL_DYNAMIC_DRAW);
    glUniform4f(uTextColor, 0.3f, 0.7f, 0.95f, 0.6f);
    glDrawArrays(GL_LINE_LOOP, 0, MARKER_SEGMENTS);
}
#endif

static void drawTextOverlay(GLFWwindow* window, const mat4& view, const mat4& proj) {
#ifdef HAVE_STB_EASY_FONT
    // Use window-coordinate ortho (NOT framebuffer-coordinate) so text/marker
    // pixel sizes match cursor coordinates on Retina displays.
    int wsW, wsH;
    glfwGetWindowSize(window, &wsW, &wsH);
    if (wsW <= 0 || wsH <= 0) return;
    mat4 ortho = Ortho(0.0f, (float)wsW, (float)wsH, 0.0f, -1.0f, 1.0f);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(textProgram);
    glUniformMatrix4fv(uTextProj, 1, GL_TRUE, ortho);

    // Draw the 2D selection ring first (uses the same text program & ortho).
    drawSelectionMarker(window, view, proj);

    glBindVertexArray(textVAO);

    // -- HUD line (top-left)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << (paused ? "PAUSED" : "RUNNING")
            << "   speed " << timeScale << "x"
            << "   bodies " << NUM_PLANETS;
        glUniform4f(uTextColor, 0.85f, 0.95f, 1.0f, 1.0f);
        drawTextString(12.0f, 16.0f, oss.str().c_str());
    }

    // Layout uses 2x-scaled text, so account for that when spacing/anchoring.
    const float L = 16.0f;      // line height in source units (becomes ~32 px on screen)
    // PANEL_W is how many SCREEN pixels we reserve on the right for the info panel.
    // Text is rendered at TEXT_SCALE (2x), so this needs to cover the widest line × 2.
    const float PANEL_W = 760.0f;

    // -- Controls hint (bottom-left, two lines)
    {
        glUniform4f(uTextColor, 0.65f, 0.7f, 0.8f, 1.0f);
        drawTextString(12.0f, (float)wsH - 2.0f * L - 16.0f,
            "1-9 select major bodies   0 clear   Left/Right prev/next body   F focus   G gravity   M mouse-look");
        drawTextString(12.0f, (float)wsH - L - 12.0f,
            "WASD fly (mouse-look only)   Space pause   Up/Down speed   R reset   ESC quit");
    }

    // -- Selected planet panel (top-right) — clamp to keep on narrow windows
    if (selectedPlanet >= 0) {
        const Planet& p = planets[selectedPlanet];
        float xR = std::max((float)wsW - PANEL_W, 12.0f);
        float y  = 16.0f;

        glUniform4f(uTextColor, 1.0f, 0.95f, 0.7f, 1.0f);
        drawTextString(xR, y, p.name);

        glUniform4f(uTextColor, 0.88f, 0.88f, 0.9f, 1.0f);
        std::ostringstream l1; l1 << std::fixed << std::setprecision(1);
        l1 << "Diameter:  " << p.diameterKm << " km";
        drawTextString(xR, y + 1.0f * L, l1.str().c_str());

        std::ostringstream l2; l2 << std::fixed << std::setprecision(2);
        l2 << "Day length: " << p.dayHours << " h";
        drawTextString(xR, y + 2.0f * L, l2.str().c_str());

        std::ostringstream l3; l3 << std::fixed << std::setprecision(2);
        l3 << "Year:       " << p.yearDays << " Earth-days";
        drawTextString(xR, y + 3.0f * L, l3.str().c_str());

        std::ostringstream l4; l4 << std::fixed << std::setprecision(2);
        l4 << "Gravity:    " << p.gravityMS2 << " m/s^2";
        drawTextString(xR, y + 4.0f * L, l4.str().c_str());

        std::ostringstream l5; l5 << std::fixed << std::setprecision(1);
        l5 << "Axial tilt: " << p.axialTilt << " deg";
        drawTextString(xR, y + 5.0f * L, l5.str().c_str());

        glUniform4f(uTextColor, 0.7f, 0.9f, 1.0f, 1.0f);
        drawTextString(xR, y + 6.5f * L, p.description);

        if (focusedPlanet == selectedPlanet) {
            glUniform4f(uTextColor, 0.5f, 1.0f, 0.6f, 1.0f);
            drawTextString(xR, y + 8.0f * L, "[FOCUSED]  press F to release");
        } else {
            glUniform4f(uTextColor, 0.7f, 0.8f, 0.7f, 1.0f);
            drawTextString(xR, y + 8.0f * L, "press F to focus camera");
        }
    } else {
        glUniform4f(uTextColor, 0.6f, 0.7f, 0.8f, 1.0f);
        float xR = std::max((float)wsW - PANEL_W, 12.0f);
        drawTextString(xR, 16.0f, "Press 1-9 to select a planet");
        drawTextString(xR, 32.0f, "Then press F to focus camera");
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
#endif
}

// ── Gravity simulator scene ──────────────────────────────────────────────────

static void displayGravityScene(GLFWwindow* window, float realDt) {
    // 1. Update physics (semi-implicit Euler — energy-stable enough for this).
    if (gravFalling && !gravLanded) {
        gravObjVy   += gravG * realDt;
        gravObjY    -= gravObjVy * realDt;
        gravElapsed += realDt;
        if (gravObjVy > gravPeakV) gravPeakV = gravObjVy;
        if (gravObjY <= 0.0f) {
            gravObjY    = 0.0f;
            gravFalling = false;
            gravLanded  = true;
        }
    }

    // 2. Set up 2D rendering.
    glClearColor(0.04f, 0.05f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int wsW, wsH;
    glfwGetWindowSize(window, &wsW, &wsH);
    if (wsW <= 0 || wsH <= 0) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(textProgram);
    mat4 ortho = Ortho(0.0f, (float)wsW, (float)wsH, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uTextProj, 1, GL_TRUE, ortho);

    // 3. Layout in window pixels.
    float topMargin  = 70.0f;
    float botMargin  = 90.0f;
    float groundY    = (float)wsH - botMargin;
    float usableH    = groundY - topMargin;
    // metres-per-pixel scale: drop height fills most of the usable area
    float pxPerMetre = usableH / std::max(gravStartY * 1.05f, 1.0f);
    float cx         = (float)wsW * 0.5f;
    float ballR      = 22.0f;
    float ballY      = groundY - gravObjY * pxPerMetre;

    glBindVertexArray(markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);

    // 3a. Ground line.
    {
        float verts[] = { 40.0f, groundY, (float)wsW - 40.0f, groundY };
        glUniform4f(uTextColor, 0.55f, 0.7f, 0.45f, 1.0f);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 2);
    }

    // 3b. Height-scale ticks on the left side (every 10 m).
    {
        std::vector<float> ticks;
        for (float h = 0.0f; h <= gravStartY + 0.5f; h += 10.0f) {
            float ty = groundY - h * pxPerMetre;
            ticks.push_back(58.0f); ticks.push_back(ty);
            ticks.push_back(78.0f); ticks.push_back(ty);
        }
        glUniform4f(uTextColor, 0.35f, 0.4f, 0.55f, 1.0f);
        glBufferData(GL_ARRAY_BUFFER, ticks.size() * sizeof(float), ticks.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, (int)(ticks.size() / 2));
    }

    // 3c. Drop-height marker (dashed-ish solid line + tick on right).
    {
        float dy = groundY - gravStartY * pxPerMetre;
        float verts[] = {
            (float)wsW - 80.0f, dy,
            (float)wsW - 58.0f, dy,
        };
        glUniform4f(uTextColor, 0.7f, 0.7f, 0.4f, 1.0f);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 2);
    }

    // 3d. Trail of past positions (faint dots).
    static std::vector<float> trail;       // pairs of (t, y_metres) for last ~3 s of fall
    static float lastSampleT = -1.0f;
    if (gravFalling && !gravLanded) {
        if (gravElapsed - lastSampleT > 0.05f) {
            trail.push_back(gravElapsed);
            trail.push_back(gravObjY);
            lastSampleT = gravElapsed;
            while (trail.size() > 4 * 80) {  // keep last ~80 samples
                trail.erase(trail.begin(), trail.begin() + 2);
            }
        }
    }
    if (!trail.empty()) {
        std::vector<float> verts;
        verts.reserve(trail.size());
        for (size_t i = 0; i < trail.size(); i += 2) {
            float ty = trail[i + 1];
            float yScreen = groundY - ty * pxPerMetre;
            verts.push_back(cx - ballR - 8.0f); verts.push_back(yScreen);
            verts.push_back(cx - ballR - 4.0f); verts.push_back(yScreen);
        }
        glUniform4f(uTextColor, 0.5f, 0.6f, 0.85f, 0.7f);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, (int)(verts.size() / 2));
    }
    if (!gravFalling && !gravLanded && gravElapsed == 0.0f) {
        trail.clear();
        lastSampleT = -1.0f;
    }

    // 3e. Falling object (filled disc + outline).
    const int CIRC_SEGS = 36;
    {
        std::vector<float> disc;
        disc.reserve((CIRC_SEGS + 2) * 2);
        disc.push_back(cx); disc.push_back(ballY);
        for (int i = 0; i <= CIRC_SEGS; ++i) {
            float t = i * 2.0f * (float)M_PI / CIRC_SEGS;
            disc.push_back(cx + ballR * cosf(t));
            disc.push_back(ballY + ballR * sinf(t));
        }
        glUniform4f(uTextColor, 0.85f, 0.55f, 0.30f, 1.0f);
        glBufferData(GL_ARRAY_BUFFER, disc.size() * sizeof(float), disc.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_FAN, 0, CIRC_SEGS + 2);
    }
    {
        std::vector<float> ring;
        ring.reserve(CIRC_SEGS * 2);
        for (int i = 0; i < CIRC_SEGS; ++i) {
            float t = i * 2.0f * (float)M_PI / CIRC_SEGS;
            ring.push_back(cx + ballR * cosf(t));
            ring.push_back(ballY + ballR * sinf(t));
        }
        glUniform4f(uTextColor, 1.0f, 0.85f, 0.55f, 1.0f);
        glBufferData(GL_ARRAY_BUFFER, ring.size() * sizeof(float), ring.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINE_LOOP, 0, CIRC_SEGS);
    }

#ifdef HAVE_STB_EASY_FONT
    // 4. Text overlays.
    glBindVertexArray(textVAO);

    // Title (top-left)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "GRAVITY DEMO  ::  %s   g = %.2f m/s^2",
                 gravPlanetName, gravG);
        glUniform4f(uTextColor, 1.0f, 0.95f, 0.7f, 1.0f);
        drawTextString(40.0f, 24.0f, buf);
    }

    // Real-world comparison hint (top-right)
    {
        char buf[128];
        // Theoretical time-to-fall from full drop height (for context)
        float tTotal = sqrtf(2.0f * gravStartY / std::max(gravG, 0.0001f));
        float vEnd   = sqrtf(2.0f * gravG * gravStartY);
        snprintf(buf, sizeof(buf), "From %.0fm:  total fall %.2fs   impact %.1f m/s",
                 gravStartY, tTotal, vEnd);
        glUniform4f(uTextColor, 0.6f, 0.75f, 0.95f, 1.0f);
        float xR = std::max((float)wsW - 720.0f, 12.0f);
        drawTextString(xR, 24.0f, buf);
    }

    // Live stats (above the ground)
    {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Height %5.1f m   Velocity %5.1f m/s   Time %5.2f s   Peak v %5.1f m/s",
                 gravObjY, gravObjVy, gravElapsed, gravPeakV);
        glUniform4f(uTextColor, 0.9f, 0.9f, 0.95f, 1.0f);
        drawTextString(40.0f, groundY - 24.0f, buf);
    }

    // Status pill (just under title)
    {
        const char* state = gravLanded ? "[LANDED]" :
                            (gravFalling ? "[FALLING]" : "[READY]");
        glUniform4f(uTextColor,
                    gravLanded ? 0.6f : (gravFalling ? 0.5f : 0.7f),
                    gravLanded ? 1.0f : (gravFalling ? 1.0f : 0.85f),
                    gravLanded ? 0.6f : (gravFalling ? 0.7f : 0.6f), 1.0f);
        drawTextString(40.0f, 44.0f, state);
    }

    // Controls hint (bottom)
    {
        glUniform4f(uTextColor, 0.6f, 0.65f, 0.75f, 1.0f);
        drawTextString(40.0f, (float)wsH - 50.0f,
            "Space  drop / pause     R  reset     Up / Down  change drop height");
        drawTextString(40.0f, (float)wsH - 28.0f,
            "G  exit gravity mode     ESC  quit");
    }
#endif

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    int windowWidth  = 1280;
    int windowHeight = 800;
    lastMouseX = windowWidth  * 0.5f;
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
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    winWidth  = fbWidth;
    winHeight = fbHeight;
    glViewport(0, 0, winWidth, winHeight);

    updateWindowTitle(window);
    std::cout << "Controls:\n"
              << "  M       toggle mouse capture (free-look)\n"
              << "  WASD    fly camera\n"
              << "  Click   select planet (when mouse not captured)\n"
              << "  F       toggle focus camera on selected planet\n"
              << "  Space   pause / resume\n"
              << "  Up/Down speed up / slow down\n"
              << "  R       reset\n"
              << "  ESC     quit\n";

    // Diagnostic: print working directory + HAVE_STB_IMAGE state, so
    // we can tell whether texture loading should be able to succeed.
    {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) std::cout << "[startup] cwd = " << cwd << "\n";
#ifdef HAVE_STB_IMAGE
        std::cout << "[startup] HAVE_STB_IMAGE = 1  (texture loading ENABLED)\n";
#else
        std::cout << "[startup] HAVE_STB_IMAGE = 0  (texture loading DISABLED at compile time)\n";
#endif
    }

    init();
    lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dtRaw = now - lastTime;
        lastTime = now;

        float dt = paused ? 0.0f : dtRaw * timeScale;

        // Movement only in free-look mode AND when not focused AND in solar scene.
        if (mouseCaptured && focusedPlanet < 0 && sceneMode == SCENE_SOLAR) {
            vec3 right = normalize(cross(camFront, camUp));
            float mv = camSpeed * dtRaw; // camera moves on real time, not sim time
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos = camPos + camFront * mv;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos = camPos - camFront * mv;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos = camPos - right    * mv;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos = camPos + right    * mv;
        }

        display(window, dt, dtRaw);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
