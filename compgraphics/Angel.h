#pragma once

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <cstdlib>

#define BUFFER_OFFSET(offset) ((void*)(offset))

//----------------------------------------------------------------------------
// vec2

struct vec2 {
    GLfloat x, y;
    vec2(GLfloat x = 0, GLfloat y = 0) : x(x), y(y) {}
    GLfloat& operator[](int i) { return (&x)[i]; }
    const GLfloat& operator[](int i) const { return (&x)[i]; }
    operator const GLfloat*() const { return &x; }
};

//----------------------------------------------------------------------------
// vec3

struct vec3 {
    GLfloat x, y, z;
    vec3(GLfloat x = 0, GLfloat y = 0, GLfloat z = 0) : x(x), y(y), z(z) {}
    GLfloat& operator[](int i) { return (&x)[i]; }
    const GLfloat& operator[](int i) const { return (&x)[i]; }
    operator const GLfloat*() const { return &x; }
};

inline vec3 operator+(const vec3& a, const vec3& b) { return vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
inline vec3 operator-(const vec3& a, const vec3& b) { return vec3(a.x-b.x, a.y-b.y, a.z-b.z); }
inline vec3 operator-(const vec3& v)                { return vec3(-v.x, -v.y, -v.z); }
inline vec3 operator*(const vec3& v, GLfloat s)     { return vec3(v.x*s, v.y*s, v.z*s); }
inline vec3 operator*(GLfloat s, const vec3& v)     { return v * s; }

inline GLfloat dot(const vec3& a, const vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
inline vec3 cross(const vec3& a, const vec3& b) {
    return vec3(a.y*b.z - a.z*b.y,
                a.z*b.x - a.x*b.z,
                a.x*b.y - a.y*b.x);
}
inline GLfloat length(const vec3& v) { return sqrtf(dot(v, v)); }
inline vec3 normalize(const vec3& v) { GLfloat l = length(v); return v * (1.0f / l); }

//----------------------------------------------------------------------------
// vec4

struct vec4 {
    GLfloat x, y, z, w;
    vec4(GLfloat x = 0, GLfloat y = 0, GLfloat z = 0, GLfloat w = 0)
        : x(x), y(y), z(z), w(w) {}
    GLfloat& operator[](int i) { return (&x)[i]; }
    const GLfloat& operator[](int i) const { return (&x)[i]; }
    operator const GLfloat*() const { return &x; }
};

//----------------------------------------------------------------------------
// mat4 — row-major; pass GL_TRUE to glUniformMatrix4fv to transpose for OpenGL

struct mat4 {
    vec4 m[4]; // m[i] = row i

    mat4(GLfloat d = 1.0f) {
        m[0] = vec4(d, 0, 0, 0);
        m[1] = vec4(0, d, 0, 0);
        m[2] = vec4(0, 0, d, 0);
        m[3] = vec4(0, 0, 0, d);
    }
    mat4(vec4 r0, vec4 r1, vec4 r2, vec4 r3) {
        m[0] = r0; m[1] = r1; m[2] = r2; m[3] = r3;
    }

    vec4& operator[](int i) { return m[i]; }
    const vec4& operator[](int i) const { return m[i]; }
    operator const GLfloat*() const { return &m[0].x; }
};

inline mat4 operator*(const mat4& A, const mat4& B) {
    mat4 C(0.0f);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                C[i][j] += A[i][k] * B[k][j];
    return C;
}

//----------------------------------------------------------------------------
// Transformation matrices

inline mat4 Translate(GLfloat x, GLfloat y, GLfloat z) {
    mat4 T;
    T[0][3] = x; T[1][3] = y; T[2][3] = z;
    return T;
}

inline mat4 Scale(GLfloat x, GLfloat y, GLfloat z) {
    mat4 S(0.0f);
    S[0][0] = x; S[1][1] = y; S[2][2] = z; S[3][3] = 1.0f;
    return S;
}

inline mat4 RotateX(GLfloat theta) {
    GLfloat a = theta * (GLfloat)M_PI / 180.0f;
    mat4 R;
    R[1][1] =  cosf(a); R[1][2] = -sinf(a);
    R[2][1] =  sinf(a); R[2][2] =  cosf(a);
    return R;
}

inline mat4 RotateY(GLfloat theta) {
    GLfloat a = theta * (GLfloat)M_PI / 180.0f;
    mat4 R;
    R[0][0] =  cosf(a); R[0][2] =  sinf(a);
    R[2][0] = -sinf(a); R[2][2] =  cosf(a);
    return R;
}

inline mat4 RotateZ(GLfloat theta) {
    GLfloat a = theta * (GLfloat)M_PI / 180.0f;
    mat4 R;
    R[0][0] =  cosf(a); R[0][1] = -sinf(a);
    R[1][0] =  sinf(a); R[1][1] =  cosf(a);
    return R;
}

// Standard perspective matrix (right-handed, fovy in degrees)
inline mat4 Perspective(GLfloat fovy, GLfloat aspect, GLfloat near, GLfloat far) {
    GLfloat f = 1.0f / tanf(fovy * (GLfloat)M_PI / 360.0f);
    mat4 P(0.0f);
    P[0][0] = f / aspect;
    P[1][1] = f;
    P[2][2] = (far + near) / (near - far);
    P[2][3] = 2.0f * far * near / (near - far);
    P[3][2] = -1.0f;
    return P;
}

// Standard view matrix
inline mat4 LookAt(const vec3& eye, const vec3& center, const vec3& up) {
    vec3 f = normalize(center - eye);
    vec3 r = normalize(cross(f, up));
    vec3 u = cross(r, f);
    return mat4(
        vec4( r.x,  r.y,  r.z, -dot(r, eye)),
        vec4( u.x,  u.y,  u.z, -dot(u, eye)),
        vec4(-f.x, -f.y, -f.z,  dot(f, eye)),
        vec4( 0,    0,    0,    1)
    );
}

//----------------------------------------------------------------------------
// InitShader declaration

namespace Angel {
    GLuint InitShader(const char* vShaderFile, const char* fShaderFile);
}
