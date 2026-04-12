//
//  vshader.glsl
//  compgraphics
//

#version 150

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

out vec3 fragPos;
out vec3 fragNormal;
out vec2 fragTexCoord;

void main()
{
    vec4 worldPos = model * vec4(vPosition, 1.0);
    fragPos = worldPos.xyz;

    fragNormal = mat3(transpose(inverse(model))) * vNormal;
    fragTexCoord = vTexCoord;

    gl_Position = proj * view * worldPos;
}
