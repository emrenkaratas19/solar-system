//
//  fshader.glsl
//  compgraphics
//

#version 150

in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D uTex;
uniform vec3  lightPos;
uniform vec3  viewPos;
uniform vec3  lightColor;
uniform bool  isEmissive;
uniform float ambientStrength;
uniform float specularStrength;
uniform float shininess;
uniform bool  flipTexX;

uniform bool  useSolidColor;
uniform vec3  solidColor;

// 0 = not selected, 1 = selected (adds a soft cyan rim highlight)
uniform float highlight;

out vec4 fragColor;

void main()
{
    if (useSolidColor) {
        fragColor = vec4(solidColor, 1.0);
        return;
    }

    vec2 texCoord = fragTexCoord;
    if (flipTexX) {
        texCoord.x = 1.0 - texCoord.x;
    }
    vec4 texSample = texture(uTex, texCoord);

    if (isEmissive) {
        // Emissive bodies (the Sun) — still allow a highlight tint when picked.
        vec3 base = texSample.rgb;
        if (highlight > 0.5) base = mix(base, vec3(0.7, 1.0, 1.0), 0.25);
        fragColor = vec4(base, 1.0);
        return;
    }

    vec3 norm     = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPos);
    vec3 viewDir  = normalize(viewPos  - fragPos);
    vec3 reflDir  = reflect(-lightDir, norm);

    float ambient = ambientStrength;
    float diff    = max(dot(norm, lightDir), 0.0);
    float spec    = 0.0;
    if (diff > 0.0) {
        spec = pow(max(dot(reflDir, viewDir), 0.0), shininess) * specularStrength;
    }

    vec3 diffuse = (ambient + diff) * lightColor * texSample.rgb;
    vec3 specular = spec * lightColor;
    vec3 result = diffuse + specular;

    // Bright cyan rim halo when this body is the selected one — visible even on tiny moons.
    if (highlight > 0.5) {
        float rim = 1.0 - max(dot(norm, viewDir), 0.0);
        rim = pow(rim, 1.5);
        result = mix(result, result + vec3(0.4, 0.9, 1.3), rim);
        result += vec3(0.10, 0.25, 0.35);  // overall brightness lift so even small bodies stand out
    }

    fragColor = vec4(result, 1.0);
}
