#version 450 compatibility


out vec3 albedoData;

/* DRAWBUFFERS:6 */

uniform sampler2D tex;

flat in vec3 tint;
in vec2 texcoord;

void main() {
	vec4 albedo = texture(tex, texcoord);

	if (albedo.a < 0.1) discard;

	albedoData = albedo.rgb * tint;
}
