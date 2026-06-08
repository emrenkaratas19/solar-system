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
static void drawSkybox(const mat4& view, const mat4& proj);
static void drawFlightSprite(GLFWwindow* window, const mat4& ortho);
static void drawSunGlow(const mat4& view, const mat4& proj);
static void drawWorldGlow(const vec3& center, float size, const vec3& color, float alpha,
                          const mat4& view, const mat4& proj);
static void drawEarthAtmosphere(const mat4& earthModel, const mat4& view, const mat4& proj,
                                const vec3& color = vec3(0.30f, 0.70f, 1.0f),
                                float strength = 0.36f,
                                float fill = 0.0f);
static void drawTurkeyMarker(const mat4& earthOrbitTF, float earthSpinAngle);
static void enterEarthMode(GLFWwindow* window);
static void displayEarthScene(GLFWwindow* window, float realDt);
static void displayGravityScene(GLFWwindow* window, float realDt);
static const char* massComparison(float m);

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
            verts.push_back(1.0f - (float)i / stacks);
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

static vec3 cubemapFaceDirection(int face, float s, float t) {
    switch (face) {
        case 0: return normalize(vec3( 1.0f, -t,    -s));    // +X
        case 1: return normalize(vec3(-1.0f, -t,     s));    // -X
        case 2: return normalize(vec3( s,     1.0f,  t));    // +Y
        case 3: return normalize(vec3( s,    -1.0f, -t));    // -Y
        case 4: return normalize(vec3( s,    -t,     1.0f)); // +Z
        default:return normalize(vec3(-s,    -t,    -1.0f)); // -Z
    }
}

static void sampleEquirectangular(const unsigned char* src, int w, int h,
                                  const vec3& dir, unsigned char out[4]) {
    float lon = atan2f(dir.z, dir.x);
    float lat = asinf(std::max(-1.0f, std::min(1.0f, dir.y)));

    float u = lon / (2.0f * (float)M_PI) + 0.5f;
    float v = 0.5f - lat / (float)M_PI;

    u = u - floorf(u);
    v = std::max(0.0f, std::min(1.0f, v));

    float x = u * (float)(w - 1);
    float y = v * (float)(h - 1);
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int x1 = (x0 + 1) % w;
    int y1 = std::min(y0 + 1, h - 1);
    float tx = x - (float)x0;
    float ty = y - (float)y0;

    const unsigned char* p00 = src + (y0 * w + x0) * 4;
    const unsigned char* p10 = src + (y0 * w + x1) * 4;
    const unsigned char* p01 = src + (y1 * w + x0) * 4;
    const unsigned char* p11 = src + (y1 * w + x1) * 4;

    for (int c = 0; c < 4; ++c) {
        float a = p00[c] * (1.0f - tx) + p10[c] * tx;
        float b = p01[c] * (1.0f - tx) + p11[c] * tx;
        out[c] = (unsigned char)(a * (1.0f - ty) + b * ty + 0.5f);
    }
}

static GLuint loadSkyboxCubemap(const char* panoramaPath) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

#ifdef HAVE_STB_IMAGE
    stbi_set_flip_vertically_on_load(false);
    std::string resolved = findTexture(panoramaPath);
    int w = 0, h = 0, ch = 0;
    unsigned char* panorama = nullptr;
    if (!resolved.empty()) {
        panorama = stbi_load(resolved.c_str(), &w, &h, &ch, 4);
    }

    if (panorama) {
        int faceSize = std::max(256, std::min(1024, w / 4));
        std::vector<unsigned char> facePixels(faceSize * faceSize * 4);

        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < faceSize; ++y) {
                for (int x = 0; x < faceSize; ++x) {
                    float s = (2.0f * ((float)x + 0.5f) / (float)faceSize) - 1.0f;
                    float t = (2.0f * ((float)y + 0.5f) / (float)faceSize) - 1.0f;
                    vec3 dir = cubemapFaceDirection(face, s, t);
                    sampleEquirectangular(panorama, w, h, dir,
                                          &facePixels[(y * faceSize + x) * 4]);
                }
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA,
                         faceSize, faceSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         facePixels.data());
        }

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        stbi_image_free(panorama);
        std::cout << "[skybox OK] " << resolved << " -> 6 faces ("
                  << faceSize << "x" << faceSize << ")\n";
    } else {
        if (resolved.empty()) {
            std::cerr << "[skybox MISS] " << panoramaPath << "  (no file found via any path)\n";
        } else {
            std::cerr << "[skybox FAIL] " << resolved << "  (stb_image: "
                      << stbi_failure_reason() << ")\n";
        }
        const unsigned char fallback[] = { 2, 3, 10, 255 };
        for (int face = 0; face < 6; ++face) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA,
                         1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
        }
    }
#else
    const unsigned char fallback[] = { 2, 3, 10, 255 };
    for (int face = 0; face < 6; ++face) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA,
                     1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
    }
#endif

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
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

struct ExplosionParticle {
    vec3 pos;
    vec3 vel;
    vec3 color;
    float age;
    float life;
    float size;
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

static GLuint skyboxVAO = 0, skyboxVBO = 0;
static GLuint skyboxProgram = 0;
static GLuint skyboxCubemap = 0;
static GLint  uSkyboxView = -1, uSkyboxProj = -1, uSkyboxTex = -1;

static GLint uModel, uView, uProj;
static GLint uLightPos, uViewPos, uLightColor;
static GLint uIsEmissive, uTex;
static GLint uUseSolidColor, uSolidColor;
static GLint uHighlight;
static GLint uAmbientStrength, uSpecularStrength, uShininess, uFlipTexX;

// additive visual effects
static GLuint glowProgram = 0;
static GLuint glowVAO = 0, glowVBO = 0;
static GLint  uGlowView = -1, uGlowProj = -1, uGlowColor = -1, uGlowAlpha = -1;

static GLuint atmosphereProgram = 0;
static GLuint atmosphereVAO = 0;
static GLint  uAtmModel = -1, uAtmView = -1, uAtmProj = -1, uAtmViewPos = -1;
static GLint  uAtmColor = -1, uAtmStrength = -1, uAtmFill = -1;

// text overlay GL objects
static GLuint textProgram = 0;
static GLuint textVAO = 0, textVBO = 0, textEBO = 0;
static GLint  uTextProj = -1, uTextColor = -1;

// free-flight overlay sprite
static GLuint flightProgram = 0;
static GLuint flightVAO = 0, flightVBO = 0;
static GLuint flightTex = 0;
static GLint  uFlightProj = -1, uFlightTex = -1, uFlightAlpha = -1;

static GLuint earthNightTex = 0;
static bool earthNightMode = false;

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
static float camSpeed  = 3.0f;
static vec3  flyVelocity(0.0f, 0.0f, 0.0f);
static bool  isFlying = false;
static float currentFlySpeed = 0.0f;
static float earthApproachAltitude = 999.0f;
static vec3  turkeyWorldPos(0.0f, 0.0f, 0.0f);
static bool  turkeyMarkerVisible = false;

static int   winWidth = 1280, winHeight = 800;
static float lastMouseX = 640.0f, lastMouseY = 400.0f;
static bool  firstMouse = true;
static bool  mouseCaptured = false;

// ── Selection / focus ────────────────────────────────────────────────────────

static int   selectedPlanet = -1;   // -1 = none
static int   focusedPlanet  = -1;   // -1 = free fly
static float focusFollowDistance = 0.0f;

// Solar-view destruction effect.
static bool planetExploded[NUM_PLANETS] = { false };
static float planetExplosionAge[NUM_PLANETS] = { 999.0f };
static std::vector<ExplosionParticle> explosionParticles;

// ── Scene mode (solar system vs gravity simulator) ───────────────────────────

enum SceneMode { SCENE_SOLAR = 0, SCENE_GRAVITY = 1, SCENE_EARTH = 2 };
static SceneMode sceneMode = SCENE_SOLAR;

static const float EARTH_MODE_RADIUS = 5.0f;
static const float EARTH_MODE_MIN_ALTITUDE = 0.18f;
static const float EARTH_MODE_MAX_ALTITUDE = 8.0f;

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
static float       gravMass       = 1.0f;    // object mass in kg (visualized as ball size)
static int         gravPlanetIdx  = 3;       // snapshotted on G-press for background tinting
static float       gravTimeScale  = 2.0f;    // sim time multiplier (1x = exact real physics)

// Background gradient for the gravity scene (one shader + a fullscreen quad).
static GLuint bgProgram = 0;
static GLuint bgVAO = 0, bgVBO = 0;
static GLint  uBgTop = -1, uBgBottom = -1;

// Solid-color 1x1 texture so we can shade the gravity ball with the existing
// Phong shader without needing a real bitmap.
static GLuint gravBallTex = 0;

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

static float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

static float smooth01(float v) {
    v = clamp01(v);
    return v * v * (3.0f - 2.0f * v);
}

static float descendFade(float altitude, float fullAt, float hiddenAt) {
    return smooth01((hiddenAt - altitude) / std::max(hiddenAt - fullAt, 0.0001f));
}

static void setPlanetMaterial(int planetIndex) {
    float ambient = 0.035f;
    float specular = 0.12f;
    float shine = 24.0f;

    if (planetIndex == 3) {              // Earth: water/ocean glints
        specular = 0.95f;
        shine = 72.0f;
    } else if (planetIndex == 2 || planetIndex == 8 ||
               planetIndex == 12 || planetIndex == 14 || planetIndex == 15) {
        specular = 0.22f;                // cloudy/gas/ice planets
        shine = 36.0f;
    } else if (planetIndex == 0) {
        ambient = 1.0f;
        specular = 0.0f;
        shine = 1.0f;
    }

    glUniform1f(uAmbientStrength, ambient);
    glUniform1f(uSpecularStrength, specular);
    glUniform1f(uShininess, shine);
}

static vec3 earthSurfaceDirection(float latitudeDeg, float longitudeDeg) {
    float lat = latitudeDeg * (float)M_PI / 180.0f;
    float lon = (longitudeDeg + 180.0f) * (float)M_PI / 180.0f;
    float cosLat = cosf(lat);
    return normalize(vec3(cosLat * cosf(lon), sinf(lat), cosLat * sinf(lon)));
}

static vec3 earthVisualSurfaceDirection(float latitudeDeg, float longitudeDeg) {
    return earthSurfaceDirection(latitudeDeg, -longitudeDeg);
}

static vec3 transformPoint(const mat4& m, const vec3& p) {
    return vec3(
        m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
        m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
        m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]
    );
}

static float earthAwareFlySpeed() {
    const Planet& earth = planets[3];
    vec3 toEarth = camPos - earth.worldPos;
    float distanceToEarth = length(toEarth);
    earthApproachAltitude = std::max(0.0f, distanceToEarth - earth.scale);

    float t = (earthApproachAltitude - 0.05f) / 3.0f;
    t = std::max(0.0f, std::min(1.0f, t));
    float smooth = t * t * (3.0f - 2.0f * t);

    float closeSpeed = 0.16f;
    return closeSpeed + (camSpeed - closeSpeed) * smooth;
}

static void keepCameraAboveEarthSurface() {
    const Planet& earth = planets[3];
    vec3 fromEarth = camPos - earth.worldPos;
    float distanceToEarth = length(fromEarth);
    float minimumDistance = earth.scale * 1.04f;
    if (distanceToEarth > 0.0001f && distanceToEarth < minimumDistance) {
        vec3 outward = fromEarth * (1.0f / distanceToEarth);
        camPos = earth.worldPos + outward * minimumDistance;
        float outwardSpeed = dot(flyVelocity, outward);
        if (outwardSpeed < 0.0f) {
            flyVelocity = flyVelocity - outward * outwardSpeed;
        }
    }
}

static float earthModeAltitude() {
    return std::max(0.0f, length(camPos) - EARTH_MODE_RADIUS);
}

static vec3 turkeySurfaceDirection() {
    // Earth texture is sampled flipped on X, so visual markers use mirrored longitude.
    return earthVisualSurfaceDirection(39.0f, 35.0f);
}

static float earthModeFlySpeed(float altitude) {
    float t = smooth01(altitude / EARTH_MODE_MAX_ALTITUDE);
    return 0.08f + 1.45f * t;
}

static const char* earthModeLayerName(float altitude) {
    if (altitude < 0.75f) return "landing";
    if (altitude < 2.8f)  return "surface";
    if (altitude < 5.6f)  return "cloud";
    return "orbit";
}

static bool isPlanetHiddenByExplosion(int planetIndex) {
    int idx = planetIndex;
    while (idx >= 0 && idx < NUM_PLANETS) {
        if (planetExploded[idx]) return true;
        idx = planets[idx].parentIndex;
    }
    return false;
}

static vec3 explosionDirection(int i) {
    float a = i * 2.3999632f;
    float y = 1.0f - 2.0f * ((i + 0.5f) / 72.0f);
    float r = sqrtf(std::max(0.0f, 1.0f - y * y));
    return normalize(vec3(cosf(a) * r, y, sinf(a) * r));
}

static void triggerPlanetExplosion(int planetIndex) {
    if (planetIndex <= 0 || planetIndex >= NUM_PLANETS) {
        std::cout << "Select a non-Sun planet or moon first.\n";
        return;
    }
    if (planetExploded[planetIndex]) {
        std::cout << planets[planetIndex].name << " is already exploded.\n";
        return;
    }

    Planet& p = planets[planetIndex];
    planetExploded[planetIndex] = true;
    planetExplosionAge[planetIndex] = 0.0f;
    if (focusedPlanet == planetIndex || isPlanetHiddenByExplosion(focusedPlanet)) focusedPlanet = -1;

    const int count = 72;
    float baseSize = std::max(p.scale, 0.05f);
    for (int i = 0; i < count; ++i) {
        vec3 dir = explosionDirection(i);
        float wave = sinf((float)i * 12.9898f) * 43758.5453f;
        float rnd = wave - floorf(wave);
        float speed = (0.45f + rnd * 1.25f) * (1.2f + baseSize * 2.1f);
        ExplosionParticle e;
        e.pos = p.worldPos + dir * baseSize * (0.65f + rnd * 0.55f);
        e.vel = dir * speed + vec3(0.0f, 0.05f + rnd * 0.10f, 0.0f);
        e.age = -rnd * 0.12f;
        e.life = 1.8f + rnd * 1.4f;
        e.size = baseSize * (0.10f + rnd * 0.18f);
        if (i % 3 == 0) {
            e.color = vec3(1.0f, 0.32f, 0.08f);
        } else if (i % 3 == 1) {
            e.color = vec3(1.0f, 0.74f, 0.20f);
        } else {
            e.color = vec3(0.55f, 0.46f, 0.38f);
        }
        explosionParticles.push_back(e);
    }

    selectedPlanet = -1;
    std::cout << "Exploded " << p.name << "\n";
}

static void resetExplosions() {
    for (int i = 0; i < NUM_PLANETS; ++i) {
        planetExploded[i] = false;
        planetExplosionAge[i] = 999.0f;
    }
    explosionParticles.clear();
}

static void updateExplosions(float realDt) {
    float dt = std::max(0.0f, std::min(realDt, 0.05f));
    for (int i = 0; i < NUM_PLANETS; ++i) {
        if (planetExploded[i]) planetExplosionAge[i] += dt;
    }
    if (explosionParticles.empty()) return;
    for (ExplosionParticle& e : explosionParticles) {
        e.age += dt;
        if (e.age <= 0.0f) continue;
        e.pos = e.pos + e.vel * dt;
        e.vel = e.vel * expf(-0.28f * dt);
    }
    explosionParticles.erase(
        std::remove_if(explosionParticles.begin(), explosionParticles.end(),
                       [](const ExplosionParticle& e) { return e.age > e.life; }),
        explosionParticles.end());
}

static void keepEarthModeCameraInFlightBand() {
    float distanceFromCenter = length(camPos);
    if (distanceFromCenter <= 0.0001f) {
        camPos = turkeySurfaceDirection() * (EARTH_MODE_RADIUS + 2.0f);
        return;
    }

    vec3 outward = camPos * (1.0f / distanceFromCenter);
    float minDistance = EARTH_MODE_RADIUS + EARTH_MODE_MIN_ALTITUDE;
    float maxDistance = EARTH_MODE_RADIUS + EARTH_MODE_MAX_ALTITUDE;
    if (distanceFromCenter < minDistance) {
        camPos = outward * minDistance;
        float inwardSpeed = dot(flyVelocity, outward);
        if (inwardSpeed < 0.0f) flyVelocity = flyVelocity - outward * inwardSpeed;
    } else if (distanceFromCenter > maxDistance) {
        camPos = outward * maxDistance;
        float outwardSpeed = dot(flyVelocity, outward);
        if (outwardSpeed > 0.0f) flyVelocity = flyVelocity - outward * outwardSpeed;
    }
}

static void enterEarthMode(GLFWwindow* window) {
    sceneMode = SCENE_EARTH;
    focusedPlanet = -1;
    selectedPlanet = 3;
    paused = false;
    flyVelocity = vec3(0.0f, 0.0f, 0.0f);
    isFlying = false;

    vec3 turkeyDir = turkeySurfaceDirection();
    camPos = turkeyDir * (EARTH_MODE_RADIUS + 4.2f);
    camFront = normalize(-turkeyDir);
    updateAnglesFromCamFront();

    mouseCaptured = true;
    firstMouse = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    updateWindowTitle(window);
    std::cout << "Entered Earth travel mode. Fly toward the Turkey marker.\n";
}

static void updateWindowTitle(GLFWwindow* window) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    if (sceneMode == SCENE_GRAVITY) {
        oss << "Gravity Demo  |  " << gravPlanetName
            << "  g = " << gravG << " m/s^2"
            << "  drop = " << gravStartY << " m"
            << "  |  Space:drop  R:reset  Up/Down:height  G:exit  ESC:quit";
    } else if (sceneMode == SCENE_EARTH) {
        oss << "Earth Travel Mode  |  N:" << (earthNightMode ? "day" : "night")
            << "  WASD:fly  Mouse:look  R:reset above Turkey  E:exit  ESC:quit";
    } else {
        oss << "Solar System  |  E:Earth Mode  X:Explode  M:Mouse  WASD:Move  Click:Select  F:Focus  G:Gravity  Space:Pause  Up/Down:Speed  R:Reset  ESC:Quit  |  ";
        if (paused) oss << "Paused";
        else        oss << "Speed " << timeScale << "x";
        if (selectedPlanet >= 0) oss << "  |  Selected: " << planets[selectedPlanet].name;
        if (focusedPlanet  >= 0) oss << "  |  Focused: "  << planets[focusedPlanet].name;
    }
    glfwSetWindowTitle(window, oss.str().c_str());
}

static void resetToDefaults(GLFWwindow* window) {
    sceneMode = SCENE_SOLAR;
    timeScale = 1.0f;
    paused = false;
    camPos = defaultCamPos;
    camYaw = defaultYaw;
    camPitch = defaultPitch;
    updateCamFrontFromAngles();
    selectedPlanet = -1;
    focusedPlanet  = -1;
    resetExplosions();
    earthNightMode = false;
    flyVelocity = vec3(0.0f, 0.0f, 0.0f);
    isFlying = false;
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
        if (isPlanetHiddenByExplosion(i)) continue;
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
    if (isPlanetHiddenByExplosion(idx)) {
        std::cout << planets[idx].name << " is gone. Press R to reset.\n";
        return;
    }
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

    // E toggles Earth-travel mode — but only outside gravity sim (gravity uses E for "heavier mass")
    if (key == GLFW_KEY_E && action == GLFW_PRESS && sceneMode != SCENE_GRAVITY) {
        if (sceneMode == SCENE_EARTH) {
            sceneMode = SCENE_SOLAR;
            camPos = defaultCamPos;
            camYaw = defaultYaw;
            camPitch = defaultPitch;
            updateCamFrontFromAngles();
            flyVelocity = vec3(0.0f, 0.0f, 0.0f);
            isFlying = false;
            mouseCaptured = false;
            firstMouse = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            updateWindowTitle(window);
            std::cout << "Exited Earth travel mode\n";
        } else if (sceneMode == SCENE_SOLAR) {
            enterEarthMode(window);
        }
        return;
    }

    if (sceneMode == SCENE_EARTH) {
        if (action != GLFW_PRESS) return;
        if (key == GLFW_KEY_R) {
            enterEarthMode(window);
        } else if (key == GLFW_KEY_N) {
            earthNightMode = !earthNightMode;
            updateWindowTitle(window);
            std::cout << "Earth mode: " << (earthNightMode ? "night map\n" : "day map\n");
        } else if (key == GLFW_KEY_M) {
            mouseCaptured = !mouseCaptured;
            glfwSetInputMode(window, GLFW_CURSOR,
                             mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            firstMouse = true;
            flyVelocity = vec3(0.0f, 0.0f, 0.0f);
            isFlying = false;
            updateWindowTitle(window);
        }
        return;
    }

    if (sceneMode == SCENE_SOLAR && key == GLFW_KEY_X && action == GLFW_PRESS) {
        if (selectedPlanet < 0) {
            std::cout << "Select a planet first, then press X to explode it.\n";
        } else {
            triggerPlanetExplosion(selectedPlanet);
            updateWindowTitle(window);
        }
        return;
    }

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
                gravPlanetIdx  = selectedPlanet;
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
        } else if (key == GLFW_KEY_E) {              // heavier (x2)
            gravMass *= 2.0f;
            if (gravMass > 10000.0f) gravMass = 10000.0f;
            std::cout << "Mass: " << gravMass << " kg " << massComparison(gravMass) << "\n";
        } else if (key == GLFW_KEY_Q) {              // lighter (/2)
            gravMass *= 0.5f;
            if (gravMass < 0.001f) gravMass = 0.001f;
            std::cout << "Mass: " << gravMass << " kg " << massComparison(gravMass) << "\n";
        } else if (key == GLFW_KEY_PERIOD) {         // faster sim
            gravTimeScale *= 1.5f;
            if (gravTimeScale > 16.0f) gravTimeScale = 16.0f;
            std::cout << "Sim speed: " << gravTimeScale << "x\n";
        } else if (key == GLFW_KEY_COMMA) {          // slower sim
            gravTimeScale /= 1.5f;
            if (gravTimeScale < 0.1f) gravTimeScale = 0.1f;
            std::cout << "Sim speed: " << gravTimeScale << "x\n";
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
        flyVelocity = vec3(0.0f, 0.0f, 0.0f);
        isFlying = false;
        updateWindowTitle(window);
    }

    if (key == GLFW_KEY_R && action == GLFW_PRESS) resetToDefaults(window);

    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        if (focusedPlanet >= 0) {
            focusedPlanet = -1;
            flyVelocity = vec3(0.0f, 0.0f, 0.0f);
            isFlying = false;
            std::cout << "Focus released\n";
        } else if (selectedPlanet >= 0) {
            focusedPlanet = selectedPlanet;
            flyVelocity = vec3(0.0f, 0.0f, 0.0f);
            isFlying = false;
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

// Tiny 1x1 solid-color texture — used to give the gravity ball a flat hue
// while still routing through the Phong-lit planet shader for shading.
static GLuint makeSolidTex(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char rgba[4] = { r, g, b, 255 };
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// Human-relatable comparison for the mass value (shown next to the kg number).
static const char* massComparison(float m) {
    if (m < 0.01f)    return "(lighter than a paperclip)";
    if (m < 0.1f)     return "(about a tennis ball)";
    if (m < 1.0f)     return "(about an apple)";
    if (m < 10.0f)    return "(about a brick)";
    if (m < 100.0f)   return "(about a person)";
    if (m < 1000.0f)  return "(about a car)";
    if (m < 10000.0f) return "(about an elephant)";
    return "(heavier than a truck)";
}

// Background gradient shader for the gravity scene.
static void initBackgroundShader() {
    const char* vs =
        "#version 150\n"
        "in vec2 inPos;\n"
        "out vec2 vUv;\n"
        "void main() {\n"
        "    vUv = inPos * 0.5 + 0.5;\n"
        "    gl_Position = vec4(inPos, 0.0, 1.0);\n"
        "}\n";
    const char* fs =
        "#version 150\n"
        "in vec2 vUv;\n"
        "uniform vec3 topColor;\n"
        "uniform vec3 bottomColor;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    float t = 1.0 - vUv.y;\n"          // vUv.y=1 is TOP in NDC -> we want dark at top
        "    vec3 c = mix(topColor, bottomColor, t);\n"
        "    fragColor = vec4(c, 1.0);\n"
        "}\n";

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, nullptr); glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, nullptr); glCompileShader(f);
    bgProgram = glCreateProgram();
    glAttachShader(bgProgram, v);
    glAttachShader(bgProgram, f);
    glBindAttribLocation(bgProgram, 0, "inPos");
    glLinkProgram(bgProgram);
    glDeleteShader(v); glDeleteShader(f);
    uBgTop    = glGetUniformLocation(bgProgram, "topColor");
    uBgBottom = glGetUniformLocation(bgProgram, "bottomColor");

    float quad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };
    glGenVertexArrays(1, &bgVAO);
    glBindVertexArray(bgVAO);
    glGenBuffers(1, &bgVBO);
    glBindBuffer(GL_ARRAY_BUFFER, bgVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

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

static void initFlightSpriteRendering() {
    const char* vs =
        "#version 150\n"
        "in vec2 inPos;\n"
        "in vec2 inTexCoord;\n"
        "uniform mat4 uProj;\n"
        "out vec2 texCoord;\n"
        "void main() {\n"
        "    texCoord = inTexCoord;\n"
        "    gl_Position = uProj * vec4(inPos, 0.0, 1.0);\n"
        "}\n";
    const char* fs =
        "#version 150\n"
        "in vec2 texCoord;\n"
        "uniform sampler2D uTex;\n"
        "uniform float uAlpha;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec4 c = texture(uTex, texCoord);\n"
        "    fragColor = vec4(c.rgb, c.a * uAlpha);\n"
        "}\n";

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, nullptr); glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, nullptr); glCompileShader(f);

    flightProgram = glCreateProgram();
    glAttachShader(flightProgram, v);
    glAttachShader(flightProgram, f);
    glBindAttribLocation(flightProgram, 0, "inPos");
    glBindAttribLocation(flightProgram, 1, "inTexCoord");
    glLinkProgram(flightProgram);
    glDeleteShader(v); glDeleteShader(f);

    uFlightProj  = glGetUniformLocation(flightProgram, "uProj");
    uFlightTex   = glGetUniformLocation(flightProgram, "uTex");
    uFlightAlpha = glGetUniformLocation(flightProgram, "uAlpha");

    glGenVertexArrays(1, &flightVAO);
    glBindVertexArray(flightVAO);

    glGenBuffers(1, &flightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, flightVBO);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    const unsigned char fallback[] = { 80, 120, 255, 255 };
    flightTex = loadTex("textures/flying_sprite.png", fallback);

    glBindVertexArray(0);
}

static void drawFlightSprite(GLFWwindow* window, const mat4& ortho) {
    if (!isFlying || !flightProgram || !flightVAO || !flightTex) return;

    int wsW, wsH;
    glfwGetWindowSize(window, &wsW, &wsH);
    if (wsW <= 0 || wsH <= 0) return;

    float size = std::min(180.0f, std::max(110.0f, std::min((float)wsW, (float)wsH) * 0.18f));
    float centerX = (float)wsW * 0.5f;
    float centerY = (float)wsH * 0.74f;
    float x0 = centerX - size * 0.5f;
    float y0 = centerY - size * 0.5f;
    float x1 = x0 + size;
    float y1 = y0 + size;

    float verts[] = {
        x0, y0, 0.0f, 1.0f,
        x1, y0, 1.0f, 1.0f,
        x0, y1, 0.0f, 0.0f,
        x1, y0, 1.0f, 1.0f,
        x1, y1, 1.0f, 0.0f,
        x0, y1, 0.0f, 0.0f,
    };

    float speedSq = dot(flyVelocity, flyVelocity);
    float alpha = std::min(0.72f, 0.24f + speedSq * 0.06f);

    glUseProgram(flightProgram);
    glUniformMatrix4fv(uFlightProj, 1, GL_TRUE, ortho);
    glUniform1i(uFlightTex, 0);
    glUniform1f(uFlightAlpha, alpha);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, flightTex);
    glBindVertexArray(flightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, flightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

static void initSkybox() {
    const float cubeVerts[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };

    skyboxProgram = Angel::InitShader("shaders/skybox_vshader.glsl",
                                      "shaders/skybox_fshader.glsl");

    glGenVertexArrays(1, &skyboxVAO);
    glBindVertexArray(skyboxVAO);

    glGenBuffers(1, &skyboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);

    GLuint posLoc = glGetAttribLocation(skyboxProgram, "vPosition");
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    uSkyboxView = glGetUniformLocation(skyboxProgram, "view");
    uSkyboxProj = glGetUniformLocation(skyboxProgram, "proj");
    uSkyboxTex  = glGetUniformLocation(skyboxProgram, "uSkybox");

    skyboxCubemap = loadSkyboxCubemap("textures/starmap_random_2020_4k.jpg");

    glBindVertexArray(0);
}

static void drawSkybox(const mat4& view, const mat4& proj) {
    if (!skyboxProgram || !skyboxVAO || !skyboxCubemap) return;

    mat4 skyView = view;
    skyView[0][3] = 0.0f;
    skyView[1][3] = 0.0f;
    skyView[2][3] = 0.0f;

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    glUseProgram(skyboxProgram);
    glUniformMatrix4fv(uSkyboxView, 1, GL_TRUE, skyView);
    glUniformMatrix4fv(uSkyboxProj, 1, GL_TRUE, proj);
    glUniform1i(uSkyboxTex, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

static void initLightingEffects(GLuint spherePosLoc, GLuint sphereNorLoc) {
    const char* glowVS =
        "#version 150\n"
        "in vec3 inPos;\n"
        "in vec2 inTexCoord;\n"
        "uniform mat4 view;\n"
        "uniform mat4 proj;\n"
        "out vec2 texCoord;\n"
        "void main() {\n"
        "    texCoord = inTexCoord;\n"
        "    gl_Position = proj * view * vec4(inPos, 1.0);\n"
        "}\n";
    const char* glowFS =
        "#version 150\n"
        "in vec2 texCoord;\n"
        "uniform vec3 glowColor;\n"
        "uniform float glowAlpha;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    float d = length(texCoord - vec2(0.5));\n"
        "    float core = smoothstep(0.50, 0.0, d);\n"
        "    float soft = pow(core, 1.8);\n"
        "    fragColor = vec4(glowColor * (0.35 + soft), soft * glowAlpha);\n"
        "}\n";

    GLuint gv = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(gv, 1, &glowVS, nullptr); glCompileShader(gv);
    GLuint gf = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(gf, 1, &glowFS, nullptr); glCompileShader(gf);

    glowProgram = glCreateProgram();
    glAttachShader(glowProgram, gv);
    glAttachShader(glowProgram, gf);
    glBindAttribLocation(glowProgram, 0, "inPos");
    glBindAttribLocation(glowProgram, 1, "inTexCoord");
    glLinkProgram(glowProgram);
    glDeleteShader(gv); glDeleteShader(gf);

    uGlowView  = glGetUniformLocation(glowProgram, "view");
    uGlowProj  = glGetUniformLocation(glowProgram, "proj");
    uGlowColor = glGetUniformLocation(glowProgram, "glowColor");
    uGlowAlpha = glGetUniformLocation(glowProgram, "glowAlpha");

    glGenVertexArrays(1, &glowVAO);
    glBindVertexArray(glowVAO);
    glGenBuffers(1, &glowVBO);
    glBindBuffer(GL_ARRAY_BUFFER, glowVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    const char* atmosphereVS =
        "#version 150\n"
        "in vec3 vPosition;\n"
        "in vec3 vNormal;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 proj;\n"
        "out vec3 worldPos;\n"
        "out vec3 worldNormal;\n"
        "void main() {\n"
        "    vec4 wp = model * vec4(vPosition, 1.0);\n"
        "    worldPos = wp.xyz;\n"
        "    worldNormal = mat3(transpose(inverse(model))) * vNormal;\n"
        "    gl_Position = proj * view * wp;\n"
        "}\n";
    const char* atmosphereFS =
        "#version 150\n"
        "in vec3 worldPos;\n"
        "in vec3 worldNormal;\n"
        "uniform vec3 viewPos;\n"
        "uniform vec3 atmosphereColor;\n"
        "uniform float atmosphereStrength;\n"
        "uniform float atmosphereFill;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec3 n = normalize(worldNormal);\n"
        "    vec3 v = normalize(viewPos - worldPos);\n"
        "    float rim = pow(1.0 - max(dot(n, v), 0.0), 2.1);\n"
        "    float fill = atmosphereFill * (0.28 + rim * 0.72);\n"
        "    float alpha = rim * atmosphereStrength + fill;\n"
        "    fragColor = vec4(atmosphereColor * (0.55 + rim * 1.6), alpha);\n"
        "}\n";

    GLuint av = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(av, 1, &atmosphereVS, nullptr); glCompileShader(av);
    GLuint af = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(af, 1, &atmosphereFS, nullptr); glCompileShader(af);

    atmosphereProgram = glCreateProgram();
    glAttachShader(atmosphereProgram, av);
    glAttachShader(atmosphereProgram, af);
    glBindAttribLocation(atmosphereProgram, spherePosLoc, "vPosition");
    glBindAttribLocation(atmosphereProgram, sphereNorLoc, "vNormal");
    glLinkProgram(atmosphereProgram);
    glDeleteShader(av); glDeleteShader(af);

    uAtmModel    = glGetUniformLocation(atmosphereProgram, "model");
    uAtmView     = glGetUniformLocation(atmosphereProgram, "view");
    uAtmProj     = glGetUniformLocation(atmosphereProgram, "proj");
    uAtmViewPos  = glGetUniformLocation(atmosphereProgram, "viewPos");
    uAtmColor    = glGetUniformLocation(atmosphereProgram, "atmosphereColor");
    uAtmStrength = glGetUniformLocation(atmosphereProgram, "atmosphereStrength");
    uAtmFill     = glGetUniformLocation(atmosphereProgram, "atmosphereFill");

    glGenVertexArrays(1, &atmosphereVAO);
    glBindVertexArray(atmosphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glEnableVertexAttribArray(spherePosLoc);
    glVertexAttribPointer(spherePosLoc, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(sphereNorLoc);
    glVertexAttribPointer(sphereNorLoc, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

static void drawWorldGlow(const vec3& center, float size, const vec3& color, float alpha,
                          const mat4& view, const mat4& proj) {
    if (!glowProgram || !glowVAO) return;

    vec3 right = normalize(cross(camFront, camUp));
    vec3 up = normalize(cross(right, camFront));
    vec3 r = right * size;
    vec3 u = up * size;

    vec3 p0 = center - r - u;
    vec3 p1 = center + r - u;
    vec3 p2 = center - r + u;
    vec3 p3 = center + r + u;

    float verts[] = {
        p0.x, p0.y, p0.z, 0.0f, 0.0f,
        p1.x, p1.y, p1.z, 1.0f, 0.0f,
        p2.x, p2.y, p2.z, 0.0f, 1.0f,
        p1.x, p1.y, p1.z, 1.0f, 0.0f,
        p3.x, p3.y, p3.z, 1.0f, 1.0f,
        p2.x, p2.y, p2.z, 0.0f, 1.0f,
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glUseProgram(glowProgram);
    glUniformMatrix4fv(uGlowView, 1, GL_TRUE, view);
    glUniformMatrix4fv(uGlowProj, 1, GL_TRUE, proj);
    glUniform3f(uGlowColor, color.x, color.y, color.z);
    glUniform1f(uGlowAlpha, alpha);

    glBindVertexArray(glowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, glowVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

static void drawSunGlow(const mat4& view, const mat4& proj) {
    drawWorldGlow(vec3(0.0f, 0.0f, 0.0f), 5.4f, vec3(1.0f, 0.62f, 0.16f), 0.70f, view, proj);
}

static void drawExplosionEffects(const mat4& view, const mat4& proj) {
    glUseProgram(program);
    glUniformMatrix4fv(uView, 1, GL_TRUE, view);
    glUniformMatrix4fv(uProj, 1, GL_TRUE, proj);
    glUniform3f(uLightPos, 0.0f, 0.0f, 0.0f);
    glUniform3f(uViewPos, camPos.x, camPos.y, camPos.z);
    glUniform3f(uLightColor, 1.0f, 0.82f, 0.48f);
    glUniform1i(uUseSolidColor, 1);
    glUniform1i(uIsEmissive, 1);
    glUniform1f(uHighlight, 0.0f);
    glUniform1i(uFlipTexX, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao);

    for (const ExplosionParticle& e : explosionParticles) {
        if (e.age <= 0.0f || e.age >= e.life) continue;
        float fade = 1.0f - e.age / e.life;
        fade = fade * fade;
        float pulse = 0.75f + 0.25f * sinf((e.age * 18.0f) + e.size * 80.0f);
        float s = e.size * (0.35f + fade * 1.55f) * pulse;
        vec3 color = e.color * fade;

        mat4 model = Translate(e.pos.x, e.pos.y, e.pos.z) * Scale(s, s, s);
        glUniform3f(uSolidColor, color.x, color.y, color.z);
        glUniformMatrix4fv(uModel, 1, GL_TRUE, model);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glUniform1i(uUseSolidColor, 0);

    for (int i = 1; i < NUM_PLANETS; ++i) {
        if (!planetExploded[i]) continue;
        float flash = 1.0f - planetExplosionAge[i] / 1.25f;
        if (flash <= 0.0f) continue;
        float size = std::max(planets[i].scale * (4.5f + planetExplosionAge[i] * 3.5f), 0.55f);
        drawWorldGlow(planets[i].worldPos, size, vec3(1.0f, 0.36f, 0.08f), 0.85f * flash, view, proj);
    }
}

static void drawEarthAtmosphere(const mat4& earthModel, const mat4& view, const mat4& proj,
                                const vec3& color, float strength, float fill) {
    if (!atmosphereProgram || !atmosphereVAO) return;
    if (strength <= 0.001f && fill <= 0.001f) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glUseProgram(atmosphereProgram);
    glUniformMatrix4fv(uAtmModel, 1, GL_TRUE, earthModel);
    glUniformMatrix4fv(uAtmView, 1, GL_TRUE, view);
    glUniformMatrix4fv(uAtmProj, 1, GL_TRUE, proj);
    glUniform3f(uAtmViewPos, camPos.x, camPos.y, camPos.z);
    glUniform3f(uAtmColor, color.x, color.y, color.z);
    glUniform1f(uAtmStrength, strength);
    glUniform1f(uAtmFill, fill);

    glBindVertexArray(atmosphereVAO);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

static void drawTurkeyMarkerAt(const mat4& earthBaseTF, float earthRadius, float markerScale) {
    vec3 turkeyDir = turkeySurfaceDirection();

    float surfaceRadius = earthRadius * 1.02f;
    turkeyWorldPos = transformPoint(earthBaseTF, turkeyDir * surfaceRadius);
    turkeyMarkerVisible = true;

    mat4 markerModel = earthBaseTF
                     * Translate(turkeyDir.x * surfaceRadius,
                                 turkeyDir.y * surfaceRadius,
                                 turkeyDir.z * surfaceRadius)
                     * Scale(markerScale, markerScale, markerScale);

    glUseProgram(program);
    glUniform1i(uUseSolidColor, 1);
    glUniform3f(uSolidColor, 1.0f, 0.86f, 0.22f);
    glUniform1i(uIsEmissive, 1);
    glUniform1f(uHighlight, 0.0f);
    glUniformMatrix4fv(uModel, 1, GL_TRUE, markerModel);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);

    glUniform1i(uUseSolidColor, 0);
}

static void drawTurkeyMarker(const mat4& earthOrbitTF, float earthSpinAngle) {
    const Planet& earth = planets[3];
    mat4 earthRot = earthOrbitTF * RotateZ(earth.axialTilt) * RotateY(earthSpinAngle);
    drawTurkeyMarkerAt(earthRot, earth.scale, 0.018f);
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
    uAmbientStrength  = glGetUniformLocation(program, "ambientStrength");
    uSpecularStrength = glGetUniformLocation(program, "specularStrength");
    uShininess        = glGetUniformLocation(program, "shininess");
    uFlipTexX         = glGetUniformLocation(program, "flipTexX");

    initLightingEffects(posLoc, norLoc);

    for (int i = 0; i < NUM_PLANETS; ++i)
        planets[i].texID = loadTex(planets[i].texPath, planets[i].color);

    {
        const unsigned char nightFallback[4] = { 8, 16, 32, 255 };
        earthNightTex = loadTex("textures/earth_nightmap.jpg", nightFallback);
    }

    // Tangerine ball texture for the gravity scene (shaded by Phong via planet shader).
    gravBallTex = makeSolidTex(220, 130, 50);

    initSkybox();
    initBackgroundShader();
    initTextRendering();
    initFlightSpriteRendering();

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
    if (sceneMode == SCENE_EARTH) {
        displayEarthScene(window, realDt);
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
    updateExplosions(realDt);

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

    mat4 earthAtmosphereModel;
    bool hasEarthAtmosphere = false;
    turkeyMarkerVisible = false;

    for (int i = 0; i < NUM_PLANETS; ++i) {
        Planet& p = planets[i];
        if (isPlanetHiddenByExplosion(i)) continue;
        // model: orbitTF * tilt * spin * scale  (so planet spins about its tilted axis)
        mat4 model = orbitTF[i]
                   * RotateZ(p.axialTilt)
                   * RotateY(p.spinAngle)
                   * Scale(p.scale, p.scale, p.scale);

        glUniformMatrix4fv(uModel, 1, GL_TRUE, model);
        glUniform1i(uIsEmissive, p.isEmissive ? 1 : 0);
        glUniform1f(uHighlight, (i == selectedPlanet) ? 1.0f : 0.0f);
        glUniform1i(uFlipTexX, (i == 3) ? 1 : 0);
        setPlanetMaterial(i);
        glBindTexture(GL_TEXTURE_2D, p.texID);

        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);

        if (i == 3) {
            float atmosphereScale = p.scale * 1.10f;
            earthAtmosphereModel = orbitTF[i]
                                 * RotateZ(p.axialTilt)
                                 * RotateY(p.spinAngle)
                                 * Scale(atmosphereScale, atmosphereScale, atmosphereScale);
            hasEarthAtmosphere = true;
        }
    }

    if (!isPlanetHiddenByExplosion(3)) {
        drawTurkeyMarker(orbitTF[3], planets[3].spinAngle);
    }

    // 7. Draw the cubemap skybox behind all scene geometry.
    drawSkybox(view, proj);

    // 8. Additive light effects.
    drawSunGlow(view, proj);
    drawExplosionEffects(view, proj);
    if (hasEarthAtmosphere) drawEarthAtmosphere(earthAtmosphereModel, view, proj);

    // 9. Text overlay (HUD + selected-planet info + 2D selection marker)
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

static void drawEarthHorizonHaze(int wsW, int wsH, float fade) {
    fade = clamp01(fade);
    if (fade <= 0.01f) return;

    float y0 = (float)wsH * 0.54f;
    float y1 = (float)wsH;
    float verts[] = {
        0.0f,       y0,
        (float)wsW, y0,
        0.0f,       y1,
        (float)wsW, y1,
    };

    glBindVertexArray(markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glUniform4f(uTextColor, 0.42f, 0.72f, 1.0f, 0.10f * fade);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    float y2 = (float)wsH * 0.72f;
    float verts2[] = {
        0.0f,       y2,
        (float)wsW, y2,
        0.0f,       y1,
        (float)wsW, y1,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts2), verts2, GL_DYNAMIC_DRAW);
    glUniform4f(uTextColor, 0.85f, 0.92f, 1.0f, 0.10f * fade);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void drawAtmosphericEntryOverlay(int wsW, int wsH, float altitude) {
    float entry = descendFade(altitude, 0.45f, 6.2f);
    if (entry <= 0.01f) return;

    float topHaze = 0.12f * entry;
    float lowerHaze = 0.18f * descendFade(altitude, 0.35f, 2.2f);
    float top[] = {
        0.0f,       0.0f,
        (float)wsW, 0.0f,
        0.0f,       (float)wsH * 0.28f,
        (float)wsW, (float)wsH * 0.28f,
    };
    float bottom[] = {
        0.0f,       (float)wsH * 0.62f,
        (float)wsW, (float)wsH * 0.62f,
        0.0f,       (float)wsH,
        (float)wsW, (float)wsH,
    };

    glBindVertexArray(markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(top), top, GL_DYNAMIC_DRAW);
    glUniform4f(uTextColor, 0.20f, 0.58f, 1.0f, topHaze);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBufferData(GL_ARRAY_BUFFER, sizeof(bottom), bottom, GL_DYNAMIC_DRAW);
    glUniform4f(uTextColor, 0.66f, 0.86f, 1.0f, lowerHaze);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    float speed = clamp01(length(flyVelocity) / 1.25f);
    float streakAlpha = (0.18f + 0.35f * speed) * entry;
    if (streakAlpha <= 0.02f) return;

    std::vector<float> streaks;
    const int count = 18;
    streaks.reserve(count * 4);
    float phase = fmodf((float)glfwGetTime() * (55.0f + speed * 70.0f), 90.0f);
    for (int i = 0; i < count; ++i) {
        float side = (i % 2 == 0) ? 1.0f : -1.0f;
        float lane = (float)(i / 2) / (float)(count / 2 - 1);
        float y = 70.0f + lane * ((float)wsH - 140.0f) + phase;
        while (y > (float)wsH - 40.0f) y -= (float)wsH - 110.0f;
        float edge = (side > 0.0f) ? (float)wsW - 28.0f : 28.0f;
        float inner = (side > 0.0f) ? (float)wsW - 120.0f - 40.0f * speed
                                    : 120.0f + 40.0f * speed;
        streaks.push_back(edge);  streaks.push_back(y - 26.0f);
        streaks.push_back(inner); streaks.push_back(y + 26.0f);
    }
    glBufferData(GL_ARRAY_BUFFER, streaks.size() * sizeof(float), streaks.data(), GL_DYNAMIC_DRAW);
    glUniform4f(uTextColor, 0.72f, 0.92f, 1.0f, streakAlpha);
    glDrawArrays(GL_LINES, 0, (int)(streaks.size() / 2));
}

static void drawEarthLandingRing(GLFWwindow* window, const mat4& view, const mat4& proj, float fade) {
    fade = clamp01(fade);
    if (fade <= 0.01f || !turkeyMarkerVisible) return;

    vec4 wp = vec4(turkeyWorldPos.x, turkeyWorldPos.y, turkeyWorldPos.z, 1.0f);
    vec4 clip = mulMV(proj, mulMV(view, wp));
    if (clip.w <= 0.001f) return;

    float nx = clip.x / clip.w;
    float ny = clip.y / clip.w;
    if (nx < -1.4f || nx > 1.4f || ny < -1.4f || ny > 1.4f) return;

    int wsW, wsH;
    glfwGetWindowSize(window, &wsW, &wsH);
    if (wsW <= 0 || wsH <= 0) return;

    float cx = (nx * 0.5f + 0.5f) * wsW;
    float cy = (1.0f - (ny * 0.5f + 0.5f)) * wsH;
    float pulse = 0.5f + 0.5f * sinf((float)glfwGetTime() * 3.2f);
    float rPx = 34.0f + pulse * 12.0f + fade * 22.0f;

    static std::vector<float> ringPx;
    ringPx.resize(MARKER_SEGMENTS * 2);

    glBindVertexArray(markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);

    for (int pass = 0; pass < 4; ++pass) {
        float r = rPx + pass * (12.0f + 2.0f * pulse);
        for (int i = 0; i < MARKER_SEGMENTS; ++i) {
            float t = i * 2.0f * (float)M_PI / MARKER_SEGMENTS;
            ringPx[i*2+0] = cx + r * cosf(t);
            ringPx[i*2+1] = cy + r * sinf(t);
        }
        glBufferData(GL_ARRAY_BUFFER, ringPx.size() * sizeof(float), ringPx.data(), GL_DYNAMIC_DRAW);
        glUniform4f(uTextColor,
                    pass < 2 ? 1.0f : 0.35f,
                    pass < 2 ? 0.86f : 0.95f,
                    pass < 2 ? 0.25f : 1.0f,
                    (0.85f - pass * 0.13f) * fade);
        glDrawArrays(GL_LINE_LOOP, 0, MARKER_SEGMENTS);
    }

    float cross[] = {
        cx - rPx - 12.0f, cy, cx - rPx + 8.0f, cy,
        cx + rPx - 8.0f,  cy, cx + rPx + 12.0f, cy,
        cx, cy - rPx - 12.0f, cx, cy - rPx + 8.0f,
        cx, cy + rPx - 8.0f,  cx, cy + rPx + 12.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(cross), cross, GL_DYNAMIC_DRAW);
    glUniform4f(uTextColor, 1.0f, 0.92f, 0.45f, 0.75f * fade);
    glDrawArrays(GL_LINES, 0, 8);
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

    drawFlightSprite(window, ortho);

    glUseProgram(textProgram);
    glUniformMatrix4fv(uTextProj, 1, GL_TRUE, ortho);
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

    if (mouseCaptured && focusedPlanet < 0 && sceneMode == SCENE_SOLAR && earthApproachAltitude < 3.0f) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2)
            << "Earth approach   altitude " << earthApproachAltitude
            << "   fly speed " << currentFlySpeed
            << "   target Turkey";
        glUniform4f(uTextColor, 1.0f, 0.86f, 0.35f, 1.0f);
        drawTextString(12.0f, 44.0f, oss.str().c_str());
    }

    // Layout uses 2x-scaled text, so account for that when spacing/anchoring.
    const float L = 16.0f;      // line height in source units (becomes ~32 px on screen)
    // PANEL_W is how many SCREEN pixels we reserve on the right for the info panel.
    // Text is rendered at TEXT_SCALE (2x), so this needs to cover the widest line × 2.
    const float PANEL_W = 760.0f;

    // -- Controls hint (bottom-left)
    {
        glUniform4f(uTextColor, 0.65f, 0.7f, 0.8f, 1.0f);
        drawTextString(12.0f, (float)wsH - 3.0f * L - 16.0f,
            "Press E for Earth travel mode   X explode selected   1-9 select planets   F focus   G gravity   M mouse-look");
        drawTextString(12.0f, (float)wsH - 2.0f * L - 12.0f,
            "WASD fly: auto-slow near Earth   0 clear   Left/Right prev/next body");
        drawTextString(12.0f, (float)wsH - L - 8.0f,
            "Space pause   Up/Down speed   R reset   ESC quit");
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

static void drawEarthModeOverlay(GLFWwindow* window, const mat4& view, const mat4& proj) {
#ifdef HAVE_STB_EASY_FONT
    int wsW, wsH;
    glfwGetWindowSize(window, &wsW, &wsH);
    if (wsW <= 0 || wsH <= 0) return;

    mat4 ortho = Ortho(0.0f, (float)wsW, (float)wsH, 0.0f, -1.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(textProgram);
    glUniformMatrix4fv(uTextProj, 1, GL_TRUE, ortho);

    float altitude = earthModeAltitude();
    float entryFactor = descendFade(altitude, 0.45f, 6.2f);
    float landingFactor = descendFade(altitude, 0.45f, 3.6f);
    drawAtmosphericEntryOverlay(wsW, wsH, altitude);
    drawEarthHorizonHaze(wsW, wsH, entryFactor);
    drawEarthLandingRing(window, view, proj, landingFactor);

    glBindVertexArray(textVAO);

    std::ostringstream status;
    status << std::fixed << std::setprecision(2)
           << "EARTH TRAVEL MODE   altitude " << altitude
           << "   fly speed " << currentFlySpeed
           << "   layer " << earthModeLayerName(altitude)
           << "   map " << (earthNightMode ? "night" : "day")
           << "   target Turkey";
    glUniform4f(uTextColor, 0.88f, 0.96f, 1.0f, 1.0f);
    drawTextString(12.0f, 16.0f, status.str().c_str());

    glUniform4f(uTextColor, 0.65f, 0.72f, 0.82f, 1.0f);
    drawTextString(12.0f, (float)wsH - 2.0f * 16.0f - 16.0f,
                   "WASD fly around Earth   Look down + W to descend   N day/night map   R reset above Turkey");
    drawTextString(12.0f, (float)wsH - 16.0f - 12.0f,
                   "E exit to solar system   M toggle mouse-look   ESC quit");

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
#endif
}

static void displayEarthScene(GLFWwindow* window, float realDt) {
    (void)realDt;
    glClearColor(0.015f, 0.018f, 0.035f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float altitude = earthModeAltitude();
    float orbitFactor = smooth01(altitude / 5.8f);
    float cloudFactor = descendFade(altitude, 2.6f, 7.0f);
    float landingFactor = descendFade(altitude, 0.45f, 3.6f);

    mat4 view = LookAt(camPos, camPos + camFront, camUp);
    mat4 proj = Perspective(55.0f, (float)winWidth / (float)winHeight, 0.05f, 200.0f);

    drawSkybox(view, proj);

    glUseProgram(program);
    glUniformMatrix4fv(uView, 1, GL_TRUE, view);
    glUniformMatrix4fv(uProj, 1, GL_TRUE, proj);
    glUniform3f(uLightPos, 18.0f, 8.0f, 14.0f);
    glUniform3f(uViewPos, camPos.x, camPos.y, camPos.z);
    glUniform3f(uLightColor, 1.0f, 1.0f, 1.0f);
    glUniform1i(uTex, 0);
    glUniform1i(uUseSolidColor, 0);
    glUniform1i(uIsEmissive, 1);   // TEST: fully light the whole globe (flat texture, no day/night terminator)
    glUniform1f(uHighlight, 0.0f);
    glUniform1i(uFlipTexX, 1);
    setPlanetMaterial(3);

    mat4 earthModel = Scale(EARTH_MODE_RADIUS, EARTH_MODE_RADIUS, EARTH_MODE_RADIUS);
    glUniformMatrix4fv(uModel, 1, GL_TRUE, earthModel);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, earthNightMode ? earthNightTex : planets[3].texID);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);

    mat4 cloudModel = Scale(EARTH_MODE_RADIUS * 1.016f,
                            EARTH_MODE_RADIUS * 1.016f,
                            EARTH_MODE_RADIUS * 1.016f);
    drawEarthAtmosphere(cloudModel, view, proj,
                        vec3(0.80f, 0.92f, 1.0f),
                        0.16f * cloudFactor,
                        0.095f * cloudFactor);

    turkeyMarkerVisible = false;
    drawTurkeyMarkerAt(mat4(), EARTH_MODE_RADIUS, 0.10f + 0.055f * landingFactor);

    mat4 atmosphereModel = Scale(EARTH_MODE_RADIUS * 1.035f,
                                 EARTH_MODE_RADIUS * 1.035f,
                                 EARTH_MODE_RADIUS * 1.035f);
    drawEarthAtmosphere(atmosphereModel, view, proj,
                        vec3(0.30f, 0.70f, 1.0f),
                        0.34f + 0.34f * orbitFactor,
                        0.035f * cloudFactor + 0.060f * landingFactor);

    drawEarthModeOverlay(window, view, proj);
}

// ── Gravity simulator scene ──────────────────────────────────────────────────

static void displayGravityScene(GLFWwindow* window, float realDt) {
    // 1. Update physics (semi-implicit Euler — energy-stable enough for this).
    // We tick simulated time at gravTimeScale x real time so falls feel punchier
    // on screen; displayed quantities are still real-world physical values.
    if (gravFalling && !gravLanded) {
        float dt = realDt * gravTimeScale;
        gravObjVy   += gravG * dt;
        gravObjY    -= gravObjVy * dt;
        gravElapsed += dt;
        if (gravObjVy > gravPeakV) gravPeakV = gravObjVy;
        if (gravObjY <= 0.0f) {
            gravObjY    = 0.0f;
            gravFalling = false;
            gravLanded  = true;
        }
    }

    // 2. Set up rendering.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int wsW, wsH;
    glfwGetWindowSize(window, &wsW, &wsH);
    if (wsW <= 0 || wsH <= 0) return;

    // 2a. Planet-themed gradient background — dark space top, planet hue at horizon.
    {
        float tr = 0.02f, tg = 0.03f, tb = 0.10f;   // top: deep space
        float br = 0.15f, bg = 0.17f, bb = 0.24f;
        if (gravPlanetIdx >= 0 && gravPlanetIdx < NUM_PLANETS) {
            br = (planets[gravPlanetIdx].color[0] / 255.0f) * 0.45f + 0.03f;
            bg = (planets[gravPlanetIdx].color[1] / 255.0f) * 0.45f + 0.03f;
            bb = (planets[gravPlanetIdx].color[2] / 255.0f) * 0.45f + 0.03f;
        }
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUseProgram(bgProgram);
        glBindVertexArray(bgVAO);
        glUniform3f(uBgTop,    tr, tg, tb);
        glUniform3f(uBgBottom, br, bg, bb);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // 2b. Switch to text program for 2D markers + text.
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
    // Ball radius scales logarithmically with mass so values across many decades
    // are still visible (0.001 kg ~ 11 px, 1 kg ~ 30 px, 1000 kg ~ 85 px).
    float ballR      = 30.0f * powf(std::max(gravMass, 0.001f), 0.15f);
    ballR            = std::max(10.0f, std::min(ballR, 150.0f));
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

    // 3e. Falling object — render the existing 3D sphere mesh with Phong shading,
    // using an orthographic projection in window-pixel space so it sits exactly
    // at (cx, ballY) with screen radius ballR. Visibly scales with mass.
    {
        glClear(GL_DEPTH_BUFFER_BIT);     // fresh depth buffer for the 3D pass
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        glUseProgram(program);
        mat4 ballProj  = Ortho(0.0f, (float)wsW, (float)wsH, 0.0f, -1000.0f, 1000.0f);
        mat4 ballView  = mat4();
        mat4 ballModel = Translate(cx, ballY, 0.0f)
                       * Scale(ballR, ballR, ballR);

        glUniformMatrix4fv(uView,  1, GL_TRUE, ballView);
        glUniformMatrix4fv(uProj,  1, GL_TRUE, ballProj);
        glUniformMatrix4fv(uModel, 1, GL_TRUE, ballModel);
        // Sun-like light from upper-left; viewer "in front" of screen.
        glUniform3f(uLightPos,   cx - 350.0f, ballY - 350.0f, 350.0f);
        glUniform3f(uViewPos,    cx, ballY, 500.0f);
        glUniform3f(uLightColor, 1.0f, 0.97f, 0.92f);
        glUniform1i(uIsEmissive,    0);
        glUniform1i(uUseSolidColor, 0);
        glUniform1f(uHighlight,     0.0f);
        glUniform1i(uFlipTexX,      0);
        glUniform1i(uTex, 0);
        // The newer shader uses these extra Phong knobs — set sensible defaults.
        if (uAmbientStrength  >= 0) glUniform1f(uAmbientStrength,  0.15f);
        if (uSpecularStrength >= 0) glUniform1f(uSpecularStrength, 0.6f);
        if (uShininess        >= 0) glUniform1f(uShininess,        24.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gravBallTex);

        glBindVertexArray(vao);   // shared planet sphere VAO
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        // Restore text-program state for the upcoming text overlays.
        glUseProgram(textProgram);
        glUniformMatrix4fv(uTextProj, 1, GL_TRUE, ortho);
    }

#ifdef HAVE_STB_EASY_FONT
    // 4. Text overlays.
    glBindVertexArray(textVAO);

    // Title (top-left)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "GRAVITY DEMO  ::  %s   g = %.2f m/s^2   sim speed %.2fx",
                 gravPlanetName, gravG, gravTimeScale);
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

    // Live stats — two rows: motion on top, mass-derived below.
    {
        char buf[200];
        snprintf(buf, sizeof(buf),
                 "Height %5.1f m   Velocity %5.1f m/s   Time %5.2f s   Peak v %5.1f m/s",
                 gravObjY, gravObjVy, gravElapsed, gravPeakV);
        glUniform4f(uTextColor, 0.9f, 0.9f, 0.95f, 1.0f);
        drawTextString(40.0f, groundY - 44.0f, buf);

        float weightN = gravMass * gravG;
        float keJ     = 0.5f * gravMass * gravObjVy * gravObjVy;
        float momKgMS = gravMass * gravObjVy;
        snprintf(buf, sizeof(buf),
                 "Mass %.3g kg %s   Weight %.2f N   KE %.1f J   Momentum %.1f kg.m/s",
                 gravMass, massComparison(gravMass), weightN, keJ, momKgMS);
        glUniform4f(uTextColor, 0.95f, 0.85f, 0.55f, 1.0f);
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
            "Space drop/pause   R reset   Up/Down drop height   Q/E mass /2 or x2   ,/. sim speed");
        drawTextString(40.0f, (float)wsH - 28.0f,
            "G exit gravity mode   ESC quit");
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
              << "  WASD    fly camera (auto-slows near Earth)\n"
              << "  E       Earth travel mode\n"
              << "  X       explode selected planet/moon (Sun excluded)\n"
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

        if (mouseCaptured && focusedPlanet < 0 &&
            (sceneMode == SCENE_SOLAR || sceneMode == SCENE_EARTH)) {
            vec3 right = normalize(cross(camFront, camUp));
            vec3 inputDir(0.0f, 0.0f, 0.0f);
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) inputDir = inputDir + camFront;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) inputDir = inputDir - camFront;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) inputDir = inputDir - right;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) inputDir = inputDir + right;

            if (sceneMode == SCENE_EARTH) {
                earthApproachAltitude = earthModeAltitude();
                currentFlySpeed = earthModeFlySpeed(earthApproachAltitude);
            } else {
                currentFlySpeed = earthAwareFlySpeed();
            }

            float inputLenSq = dot(inputDir, inputDir);
            if (inputLenSq > 0.0001f) {
                inputDir = normalize(inputDir);
                vec3 targetVelocity = inputDir * currentFlySpeed;
                float accelRate = 4.5f;
                if (sceneMode == SCENE_EARTH) {
                    float altitudeT = smooth01(earthApproachAltitude / EARTH_MODE_MAX_ALTITUDE);
                    accelRate = 1.5f + 3.2f * altitudeT;
                }
                float accel = 1.0f - expf(-accelRate * dtRaw);
                flyVelocity = flyVelocity + (targetVelocity - flyVelocity) * accel;
            } else {
                float dragRate = 3.0f;
                if (sceneMode == SCENE_EARTH) {
                    float altitudeT = smooth01(earthApproachAltitude / EARTH_MODE_MAX_ALTITUDE);
                    dragRate = 1.15f + 1.85f * altitudeT;
                }
                float drag = expf(-dragRate * dtRaw);
                flyVelocity = flyVelocity * drag;
            }

            camPos = camPos + flyVelocity * dtRaw;
            if (sceneMode == SCENE_EARTH) {
                keepEarthModeCameraInFlightBand();
            } else {
                keepCameraAboveEarthSurface();
            }
            isFlying = dot(flyVelocity, flyVelocity) > 0.02f;
        } else {
            currentFlySpeed = 0.0f;
            earthApproachAltitude = 999.0f;
            flyVelocity = vec3(0.0f, 0.0f, 0.0f);
            isFlying = false;
        }

        display(window, dt, dtRaw);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
