//
//  fshader.glsl
//  compgraphics
//

#version 150

in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D uTex;
uniform vec3  lightPos;     // Sun origin (world space)
uniform vec3  viewPos;      // Camera position (world space)
uniform vec3  lightColor;
uniform bool  isEmissive;   // True for the Sun — skip lighting

out vec4 fragColor;

void main()
{
    vec4 texSample = texture(uTex, fragTexCoord);

    if (isEmissive) {
        fragColor = texSample;
        return;
    }

    vec3 norm     = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPos);
    vec3 viewDir  = normalize(viewPos  - fragPos);
    vec3 reflDir  = reflect(-lightDir, norm);

    float ambient  = 0.12;
    float diff     = max(dot(norm, lightDir), 0.0);
    float spec     = pow(max(dot(reflDir, viewDir), 0.0), 32.0) * 0.5;

    vec3 result = (ambient + diff + spec) * lightColor * texSample.rgb;
    fragColor   = vec4(result, 1.0);
}
