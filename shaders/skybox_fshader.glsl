//
//  skybox_fshader.glsl
//  compgraphics
//

#version 150

in vec3 texCoord;

uniform samplerCube uSkybox;

out vec4 fragColor;

void main()
{
    vec3 color = min(texture(uSkybox, texCoord).rgb * 2.2, vec3(1.0));
    fragColor = vec4(color, 1.0);
}
