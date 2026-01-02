#version 410 core

out vec4 fragc;

uniform vec3 color;
vec3 lightPosition = vec3(100.0);

uniform sampler2D tex;

in prop {
    vec3 normal;
    vec3 fragp;
    vec2 uv;
} fs_in;

void main() {

    vec3 ambient = texture(tex, fs_in.uv).rgb * 0.4;

    vec3 lightDirection = normalize(lightPosition - fs_in.fragp);
    float diff = max(dot(fs_in.normal, lightDirection), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    fragc = vec4(texture(tex, fs_in.uv).rgb * (ambient + diffuse), 1.0);
}