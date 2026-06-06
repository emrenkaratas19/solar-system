//
//  skybox_vshader.glsl
//  compgraphics
//

#version 150

in vec3 vPosition;

uniform mat4 view;
uniform mat4 proj;

out vec3 texCoord;

void main()
{
    texCoord = vPosition;
    vec4 pos = proj * view * vec4(vPosition, 1.0);
    gl_Position = pos.xyww;
}
