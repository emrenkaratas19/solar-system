//
//  vshader.glsl
//  compgraphics


#version 150

in vec4 vPosition;
uniform mat4 model;

void main()
{
    gl_Position = model * vPosition;
}
