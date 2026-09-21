#version 450 compatibility

out vec3 albedoData;

/* DRAWBUFFERS:0 */

uniform sampler2D tex;

in vec2 texcoord;
in float tint;


void main() {
    float albedoAlpha = texture(tex, texcoord * vec2(4.0, 2.0)).a;

    if (albedoAlpha < 0.1) discard;

	albedoData.b = albedoAlpha * tint;
}
